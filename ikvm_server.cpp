#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <rfb/rfbproto.h>
#include <xyz/openbmc_project/Common/error.hpp>

#include "elog-errors.hpp"
#include "ikvm_server.hpp"

namespace ikvm {

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

Server::Server(const Args& args, Input& i, Video& v) :
    numClients(0),
    input(i),
    video(v)
{
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

    server->screenData = this;
    server->desktopName = "OpenBMC IKVM";
    server->frameBuffer = video.getData();
    server->alwaysShared = true;
    server->newClientHook = newClient;

    rfbInitServer(server);

    rfbMarkRectAsModified(server, 0, 0, video.getWidth(), video.getHeight());

    server->kbdAddEvent = Input::keyEvent;
    server->ptrAddEvent = Input::pointerEvent;

    processTime = (1000000 / video.getFrameRate()) - 100;
}

void Server::resize()
{
    rfbNewFramebuffer(server, video.getData(), video.getWidth(),
                      video.getHeight(), Video::bitsPerSample,
                      Video::samplesPerPixel, Video::bytesPerPixel);
    rfbMarkRectAsModified(server, 0, 0, video.getWidth(), video.getHeight());
}

void Server::run()
{
    rfbProcessEvents(server, processTime);

    if (server->clientHead)
    {
        input.sendReport();
    }
}

void Server::sendFrame()
{
    rfbClientIteratorPtr it = rfbGetClientIterator(server);
    rfbClientPtr cl;

    while ((cl = rfbClientIteratorNext(it)))
    {
        ClientData *cd = (ClientData *)cl->clientData;
        rfbFramebufferUpdateMsg *fu = (rfbFramebufferUpdateMsg *)cl->updateBuf;

	if (!cd)
	{
		continue;
	}

        if (cd->skipFrame)
        {
            cd->skipFrame--;
            continue;
        }

        if (cl->enableLastRectEncoding)
        {
            fu->nRects = 0xFFFF;
        }
        else
        {
            fu->nRects = Swap16IfLE(1);
        }

        fu->type = rfbFramebufferUpdate;
        cl->ublen = sz_rfbFramebufferUpdateMsg;
        rfbSendUpdateBuf(cl);

        cl->tightEncoding = rfbEncodingTight;
        rfbSendTightHeader(cl, 0, 0, video.getWidth(), video.getHeight());

        cl->updateBuf[cl->ublen++] = (char)(rfbTightJpeg << 4);
        rfbSendCompressedDataTight(cl, video.getData(), video.getFrameSize());

        if (cl->enableLastRectEncoding)
        {
            rfbSendLastRectMarker(cl);
        }

        rfbSendUpdateBuf(cl);
    }

    rfbReleaseClientIterator(it);
}

void Server::clientGone(rfbClientPtr cl)
{
    Server *server = (Server *)cl->screen->screenData;

    delete (ClientData *)cl->clientData;

    if (server->numClients-- == 1)
    {
        server->video.reset();
        rfbMarkRectAsModified(server->server, 0, 0, server->video.getWidth(),
                              server->video.getHeight());
    }
}

enum rfbNewClientAction Server::newClient(rfbClientPtr cl)
{
    Server *server = (Server *)cl->screen->screenData;

    cl->clientData = new ClientData(server->video.getFrameRate(),
                                    &server->input);
    cl->clientGoneHook = clientGone;
    server->numClients++;

    return RFB_CLIENT_ACCEPT;
}

} // namespace ikvm
