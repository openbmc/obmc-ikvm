#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>

#include "ikvm_video.hpp"

namespace ikvm
{

const int Video::bitsPerSample(8);
const int Video::bytesPerPixel(4);
const int Video::samplesPerPixel(3);

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
using namespace sdbusplus::xyz::openbmc_project::Common::Device::Error;

Video::Video(const std::string &p, Input& input, int fr) :
    isStreaming(false),
    frameRate(fr),
    lastFrameIndex(-1),
    path(p)
{
    int rc;
    v4l2_capability cap;
    v4l2_format fmt;
    v4l2_requestbuffers req;
    v4l2_streamparm sparm;

    fd = open(path.c_str(), O_RDWR);
    if (fd < 0)
    {
        unsigned short xx = SHRT_MAX;
        char wakeupReport[6] = { 0 };

        wakeupReport[0] = 2;
        memcpy(&wakeupReport[2], &xx, 2);

        input.sendRaw(wakeupReport, 6);

        fd = open(path.c_str(), O_RDWR);
        if (fd < 0)
        {
            log<level::ERR>("Failed to open video device",
                            entry("PATH=%s", path.c_str()),
                            entry("ERROR=%s", strerror(errno)));
            elog<Open>(
                xyz::openbmc_project::Common::File::Open::ERRNO(errno),
                xyz::openbmc_project::Common::File::Open::PATH(path.c_str()));
        }
    }

    rc = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (rc < 0)
    {
        log<level::ERR>("Failed to query video device capabilities",
                        entry("ERROR=%s", strerror(errno)));
        elog<ReadFailure>(
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_ERRNO(errno),
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_DEVICE_PATH(path.c_str()));
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
        !(cap.capabilities & V4L2_CAP_STREAMING))
    {
        log<level::ERR>("Video device doesn't support this application");
        elog<Open>(
            xyz::openbmc_project::Common::File::Open::ERRNO(errno),
            xyz::openbmc_project::Common::File::Open::PATH(path.c_str()));
    }

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rc = ioctl(fd, VIDIOC_G_FMT, &fmt);
    if (rc < 0)
    {
        log<level::ERR>("Failed to query video device format",
                        entry("ERROR=%s", strerror(errno)));
        elog<ReadFailure>(
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_ERRNO(errno),
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_DEVICE_PATH(path.c_str()));
    }

    sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sparm.parm.capture.timeperframe.numerator = 1;
    sparm.parm.capture.timeperframe.denominator = frameRate;
    rc = ioctl(fd, VIDIOC_S_PARM, &sparm);
    if (rc < 0)
    {
        log<level::WARNING>("Failed to set video device frame rate",
                            entry("ERROR=%s", strerror(errno)));
    }

    height = fmt.fmt.pix.height;
    width = fmt.fmt.pix.width;

    req.count = 3;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    rc = ioctl(fd, VIDIOC_REQBUFS, &req);
    if (rc < 0 || req.count < 2)
    {
        log<level::ERR>("Failed to request streaming buffers",
                        entry("ERROR=%s", strerror(errno)));
        elog<ReadFailure>(
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_ERRNO(errno),
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_DEVICE_PATH(path.c_str()));
    }

    buffers.resize(req.count);

    resize();
}

Video::~Video()
{
    for (auto i = buffers.begin(); i != buffers.end(); ++i)
    {
        if (i->data)
        {
            munmap(i->data, i->size);
        }
    }

    close(fd);
}

char* Video::getData()
{
    if (lastFrameIndex >= 0)
    {
         return (char*)buffers[lastFrameIndex].data;
    }

    return nullptr;
}

void Video::getFrame()
{
    bool queue(false);
    int rc(0);
    v4l2_buffer buf;

    lock.lock();

    buf.flags = 0;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    while (rc >= 0)
    {
        rc = ioctl(fd, VIDIOC_DQBUF, &buf);
        if (rc >= 0)
        {
            buffers[buf.index].queued = false;

            if (!(buf.flags & V4L2_BUF_FLAG_ERROR))
            {
                lastFrameIndex = buf.index;
                buffers[lastFrameIndex].size = buf.bytesused;
                queue = true;
                break;
            }
            else
            {
                buffers[buf.index].size = 0;
            }
        }
    }

    if (queue)
    {
        for (unsigned int i = 0; i < buffers.size(); ++i)
        {
            if (i == (unsigned int )lastFrameIndex)
            {
                continue;
            }

            if (!buffers[i].queued)
            {
                memset(&buf, 0, sizeof(v4l2_buffer));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                rc = ioctl(fd, VIDIOC_QBUF, &buf);
                if (rc)
                {
                    log<level::ERR>("Failed to queue buffer",
                                    entry("ERROR=%s", strerror(errno)));
                    elog<ReadFailure>(
                        xyz::openbmc_project::Common::Device::ReadFailure::
                            CALLOUT_ERRNO(errno),
                        xyz::openbmc_project::Common::Device::ReadFailure::
                            CALLOUT_DEVICE_PATH(path.c_str()));
                }

                buffers[i].queued = true;
            }
        }
    }

    lock.unlock();
}

bool Video::needsResize()
{
    int rc;
    v4l2_dv_timings timings;

    rc = ioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &timings);
    if (rc < 0)
    {
        log<level::ERR>("Failed to query timings",
                        entry("ERROR=%s", strerror(errno)));
        return false;
    }

    if (timings.bt.width != width || timings.bt.height != height)
    {
        width = timings.bt.width;
        height = timings.bt.height;

        if (!width || !height)
        {
            log<level::ERR>("Failed to get new resolution",
                            entry("WIDTH=%d", width),
                            entry("HEIGHT=%d", height));
            elog<Open>(
                xyz::openbmc_project::Common::File::Open::ERRNO(-EPROTO),
                xyz::openbmc_project::Common::File::Open::PATH(path.c_str()));
        }

        lastFrameIndex = -1;

        return true;
    }

    return false;
}

void Video::reset()
{
    int rc;
    v4l2_buf_type type;

    lock.lock();

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rc = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if (rc)
    {
        log<level::ERR>("Failed to stop streaming",
                        entry("ERROR=%s", strerror(errno)));
        elog<ReadFailure>(
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_ERRNO(errno),
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_DEVICE_PATH(path.c_str()));
    }

    isStreaming = false;

    for (unsigned int i = 0; i < buffers.size(); ++i)
    {
        v4l2_buffer buf;

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        rc = ioctl(fd, VIDIOC_QBUF, &buf);
        if (rc < 0)
        {
            log<level::ERR>("Failed to queue buffer",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }

        buffers[i].queued = true;
    }

    lock.unlock();
}

void Video::resize()
{
    int rc;
    bool needsResizeCall(false);

    if (isStreaming)
    {
        v4l2_buf_type type;

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        rc = ioctl(fd, VIDIOC_STREAMOFF, &type);
        if (rc)
        {
            log<level::ERR>("Failed to stop streaming",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }
    }

    for (auto i = buffers.begin(); i != buffers.end(); ++i)
    {
        if (i->data)
        {
            munmap(i->data, i->size);
            *i = Buffer();
            needsResizeCall = true;
        }
    }

    if (needsResizeCall)
    {
        v4l2_dv_timings timings;

        rc = ioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &timings);
        if (rc < 0)
        {
            log<level::ERR>("Failed to query timings",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }

        rc = ioctl(fd, VIDIOC_S_DV_TIMINGS, &timings);
        if (rc < 0)
        {
            log<level::ERR>("Failed to set timings",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }
    }

    for (unsigned int i = 0; i < buffers.size(); ++i)
    {
        v4l2_buffer buf;

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        rc = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        if (rc < 0)
        {
            log<level::ERR>("Failed to query buffer",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }

        buffers[i].data = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].data == MAP_FAILED)
        {
            log<level::ERR>("Failed to mmap buffer",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }

        buffers[i].size = buf.length;

        rc = ioctl(fd, VIDIOC_QBUF, &buf);
        if (rc < 0)
        {
            log<level::ERR>("Failed to queue buffer",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }

        buffers[i].queued = true;
    }

    if (isStreaming)
    {
        isStreaming = false;
        start();
    }
}

void Video::start()
{
    int rc;
    v4l2_buf_type type;

    if (isStreaming)
    {
        return;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rc = ioctl(fd, VIDIOC_STREAMON, &type);
    if (rc)
    {
        log<level::ERR>("Failed to start streaming",
                        entry("ERROR=%s", strerror(errno)));
        elog<ReadFailure>(
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_ERRNO(errno),
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_DEVICE_PATH(path.c_str()));
    }

    isStreaming = true;
}

} // namespace ikvm
