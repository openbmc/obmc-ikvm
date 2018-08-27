#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>

#include "elog-errors.hpp"
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
    frameRate(fr),
    path(p)
{
    int rc;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_streamparm sparm;

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
        !(cap.capabilities & V4L2_CAP_READWRITE))
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

    resize();
}

void Video::getFrame(bool& needsResize)
{
    int rc;
    struct v4l2_format fmt;

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rc = ioctl(fd, VIDIOC_G_FMT, &fmt);
    if (rc < 0)
    {
        log<level::ERR>("Failed to query format",
                        entry("ERROR=%s", strerror(errno)));
        elog<ReadFailure>(
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_ERRNO(errno),
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_DEVICE_PATH(path.c_str()));
    }

    if (fmt.fmt.pix.height != height || fmt.fmt.pix.width != width)
    {
        height = fmt.fmt.pix.height;
        width = fmt.fmt.pix.width;
        needsResize = true;
        return;
    }

    rc = read(fd, data.data(), data.size());
    if (rc < 0)
    {
        log<level::ERR>("Failed to read frame",
                        entry("ERROR=%s", strerror(errno)));
        elog<ReadFailure>(
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_ERRNO(errno),
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_DEVICE_PATH(path.c_str()));
    }

    frameSize = rc;
}

void Video::reset()
{
    if (fd >= 0)
    {
        close(fd);
        fd = open(path.c_str(), O_RDWR);
        if (fd < 0)
        {
            log<level::ERR>("Failed to re-open video device",
                            entry("PATH=%s", path.c_str()),
                            entry("ERROR=%s", strerror(errno)));
            elog<Open>(
                xyz::openbmc_project::Common::File::Open::ERRNO(errno),
                xyz::openbmc_project::Common::File::Open::PATH(path.c_str()));
        }

        data.assign(data.size(), 0);
    }
}

void Video::resize()
{
    data.resize(width * height * bytesPerPixel, 0);
}

} // namespace ikvm
