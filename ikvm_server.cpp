#include "ikvm_server.hpp"

#include <linux/videodev2.h>
#include <rfb/rfbproto.h>

#include <boost/crc.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace ikvm
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

Server::Server(const Args& args, Input& i, Video& v) :
    pendingResize(false), frameCounter(0), numClients(0), input(i), video(v)
{
    std::string ip("localhost");
    const Args::CommandLine& commandLine = args.getCommandLine();
    int argc = commandLine.argc;

    server = rfbGetScreen(&argc, commandLine.argv, video.getWidth(),
                          video.getHeight(), Video::bitsPerSample,
                          Video::samplesPerPixel, Video::bytesPerPixel);

    if (!server)
    {
        log<level::ERR>("Failed to get VNC screen due to invalid arguments");
        elog<InvalidArgument>(
            xyz::openbmc_project::Common::InvalidArgument::ARGUMENT_NAME(""),
            xyz::openbmc_project::Common::InvalidArgument::ARGUMENT_VALUE(""));
    }

    framebuffer.resize(
        video.getHeight() * video.getWidth() * Video::bytesPerPixel, 0);

    rfbSetServerPixelFormat(server);

    server->screenData = this;
    server->desktopName = "OpenBMC IKVM";
    server->frameBuffer = framebuffer.data();
    server->newClientHook = newClient;
    server->cursor = rfbMakeXCursor(cursorWidth, cursorHeight, (char*)cursor,
                                    (char*)cursorMask);
    server->cursor->xhot = 1;
    server->cursor->yhot = 1;

    rfbStringToAddr(&ip[0], &server->listenInterface);

    rfbInitServer(server);

    rfbMarkRectAsModified(server, 0, 0, video.getWidth(), video.getHeight());

    server->kbdAddEvent = Input::keyEvent;
    server->ptrAddEvent = Input::pointerEvent;

    processTime = (1000000 / video.getFrameRate()) - 100;

    calcFrameCRC = args.getCalcFrameCRC();
}

Server::~Server()
{
    rfbScreenCleanup(server);
}

std::map<uint32_t, Server::RGBBits> pixelFormatMap = {
    {V4L2_PIX_FMT_RGB565, {5, 6, 5}},
    {V4L2_PIX_FMT_HEXTILE, {5, 6, 5}},
};

void Server::rfbSetServerPixelFormat(rfbScreenInfoPtr screen)
{
    rfbPixelFormat* format = &screen->serverFormat;
    auto it = pixelFormatMap.find(video.getPixelformat());

    // query the pixel format map or use the original format
    if (it != pixelFormatMap.end())
    {
        format->redMax = (1 << it->second.redBits) - 1;
        format->greenMax = (1 << it->second.greenBits) - 1;
        format->blueMax = (1 << it->second.blueBits) - 1;
        format->redShift = 0;
        format->greenShift = it->second.redBits;
        format->blueShift = it->second.redBits + it->second.greenBits;
    }
}

void Server::resize()
{
    if (frameCounter > video.getFrameRate())
    {
        doResize();
    }
    else
    {
        pendingResize = true;
    }
}

void Server::run()
{
    rfbProcessEvents(server, processTime);

    if (server->clientHead)
    {
        frameCounter++;
        if (pendingResize && frameCounter > video.getFrameRate())
        {
            doResize();
            pendingResize = false;
        }
    }
}

void Server::sendFrame()
{
    char* data = video.getData();
    rfbClientIteratorPtr it;
    rfbClientPtr cl;
    int64_t frame_crc = -1;

    if (!data || pendingResize)
    {
        return;
    }

    it = rfbGetClientIterator(server);

    while ((cl = rfbClientIteratorNext(it)))
    {
        ClientData* cd = (ClientData*)cl->clientData;
        rfbFramebufferUpdateMsg* fu = (rfbFramebufferUpdateMsg*)cl->updateBuf;

        if (!cd)
        {
            continue;
        }

        if (cd->skipFrame)
        {
            cd->skipFrame--;
            continue;
        }

        if (!cd->needUpdate)
        {
            continue;
        }

        if (calcFrameCRC)
        {
            if (frame_crc == -1)
            {
                /* JFIF header contains some varying data so skip it for
                 * checksum calculation */
                frame_crc =
                    boost::crc<32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true,
                               true>(data + 0x30, video.getFrameSize() - 0x30);
            }

            if (cd->last_crc == frame_crc)
            {
                continue;
            }

            cd->last_crc = frame_crc;
        }

        cd->needUpdate = false;

        if (cl->enableLastRectEncoding)
        {
            fu->nRects = 0xFFFF;
        }
        else
        {
            fu->nRects = Swap16IfLE(1);
        }

        switch (video.getPixelformat())
        {
            case V4L2_PIX_FMT_RGB24:
            case V4L2_PIX_FMT_RGB565:
                framebuffer.assign(data, data + video.getFrameSize());
                rfbMarkRectAsModified(server, 0, 0, video.getWidth(),
                                      video.getHeight());
                break;

            case V4L2_PIX_FMT_JPEG:
                fu->type = rfbFramebufferUpdate;
                cl->ublen = sz_rfbFramebufferUpdateMsg;
                rfbSendUpdateBuf(cl);
                cl->tightEncoding = rfbEncodingTight;
                rfbSendTightHeader(cl, 0, 0, video.getWidth(),
                                   video.getHeight());
                cl->updateBuf[cl->ublen++] = (char)(rfbTightJpeg << 4);
                rfbSendCompressedDataTight(cl, data, video.getFrameSize());
                if (cl->enableLastRectEncoding)
                {
                    rfbSendLastRectMarker(cl);
                }
                rfbSendUpdateBuf(cl);
                break;

            case V4L2_PIX_FMT_HEXTILE:
                fu->type = rfbFramebufferUpdate;
                cl->ublen = sz_rfbFramebufferUpdateMsg;
                rfbSendUpdateBuf(cl);

                rfbSendCompressedDataHextile(cl, data, video.getFrameSize());

                if (cl->enableLastRectEncoding)
                {
                    rfbSendLastRectMarker(cl);
                }
                rfbSendUpdateBuf(cl);
                break;

            default:
                break;
        }
    }

    rfbReleaseClientIterator(it);
}

rfbBool Server::rfbSendCompressedDataHextile(rfbClientPtr cl, char* buf,
                                             int compressedLen)
{

    for (int i = 0, portionLen = UPDATE_BUF_SIZE; i < compressedLen;
	i += portionLen)
    {
        if (i + portionLen > compressedLen)
        {
            portionLen = compressedLen - i;
        }
        if (cl->ublen + portionLen > UPDATE_BUF_SIZE)
        {
            if (!rfbSendUpdateBuf(cl))
                return FALSE;
        }
        memcpy(&cl->updateBuf[cl->ublen], &buf[i], portionLen);
        cl->ublen += portionLen;
    }

    return TRUE;
}

void Server::clientFramebufferUpdateRequest(
    rfbClientPtr cl, rfbFramebufferUpdateRequestMsg* furMsg)
{
    ClientData* cd = (ClientData*)cl->clientData;

    if (!cd)
        return;

    // Ignore the furMsg info. This service uses full frame update always.
    (void)furMsg;

    cd->needUpdate = true;
}

void Server::clientGone(rfbClientPtr cl)
{
    Server* server = (Server*)cl->screen->screenData;

    delete (ClientData*)cl->clientData;
    cl->clientData = nullptr;

    if (server->numClients-- == 1)
    {
        server->input.disconnect();
        rfbMarkRectAsModified(server->server, 0, 0, server->video.getWidth(),
                              server->video.getHeight());
    }
}

enum rfbNewClientAction Server::newClient(rfbClientPtr cl)
{
    Server* server = (Server*)cl->screen->screenData;

    cl->clientData =
        new ClientData(server->video.getFrameRate(), &server->input);
    cl->clientGoneHook = clientGone;
    cl->clientFramebufferUpdateRequestHook = clientFramebufferUpdateRequest;
    if (!server->numClients++)
    {
        server->input.connect();
        server->pendingResize = false;
        server->frameCounter = 0;
    }

    return RFB_CLIENT_ACCEPT;
}

void Server::doResize()
{
    rfbClientIteratorPtr it;
    rfbClientPtr cl;

    framebuffer.resize(
        video.getHeight() * video.getWidth() * Video::bytesPerPixel, 0);

    rfbNewFramebuffer(server, framebuffer.data(), video.getWidth(),
                      video.getHeight(), Video::bitsPerSample,
                      Video::samplesPerPixel, Video::bytesPerPixel);

    rfbSetServerPixelFormat(server);

    rfbMarkRectAsModified(server, 0, 0, video.getWidth(), video.getHeight());

    it = rfbGetClientIterator(server);

    while ((cl = rfbClientIteratorNext(it)))
    {
        ClientData* cd = (ClientData*)cl->clientData;

        // Reset Translate function when pixel format changes
        server->setTranslateFunction(cl);

        if (!cd)
        {
            continue;
        }

        // delay video updates to give the client time to resize
        cd->skipFrame = video.getFrameRate();
    }

    rfbReleaseClientIterator(it);
}

} // namespace ikvm
