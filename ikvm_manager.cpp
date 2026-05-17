#include "ikvm_manager.hpp"

#include <thread>

namespace ikvm
{
Manager::Manager(const Args& args) :
    continueExecuting(true), serverDone(false), videoDone(true),
    videoPaused(false),
    input(args.getKeyboardPath(), args.getPointerPath(), args.getUdcName()),
    video(args.getVideoPath(), input, args.getFrameRate(),
          args.getSubsampling()),
    server(args, input, video)
{}

void Manager::run()
{
    std::thread run(serverThread, this);

    while (continueExecuting)
    {
        if (server.wantsFrame())
        {
            video.start();
            video.getFrame();
            server.sendFrame();
        }
        else
        {
            video.stop();
        }

        if (video.needsResize())
        {
            waitServer(true);
            video.resize();
            server.resize();
            setVideoDone();
        }
        else
        {
            setVideoDone();
            waitServer();
        }
    }

    run.join();
}

void Manager::serverThread(Manager* manager)
{
    while (manager->continueExecuting)
    {
        manager->server.run();
        manager->setServerDone();
        manager->waitVideo();
    }
}

void Manager::setServerDone()
{
    std::unique_lock<std::mutex> ulock(lock);

    serverDone = true;
    sync.notify_all();
}

void Manager::setVideoDone()
{
    std::unique_lock<std::mutex> ulock(lock);

    videoDone = true;
    sync.notify_all();
}

void Manager::waitServer(bool pauseVideo)
{
    std::unique_lock<std::mutex> ulock(lock);

    while (!serverDone)
    {
        sync.wait(ulock);
    }

    serverDone = false;

    if (pauseVideo)
    {
        videoDone = false;
        // Wait until the server thread has actually entered waitVideo() and
        // observed videoDone=false. Without this, the server could race
        // past waitVideo() (where videoDone was still true) and start
        // another iteration of server.run() concurrently with the resize.
        while (!videoPaused)
        {
            sync.wait(ulock);
        }
    }
}

void Manager::waitVideo()
{
    std::unique_lock<std::mutex> ulock(lock);

    while (!videoDone)
    {
        videoPaused = true;
        sync.notify_all();
        sync.wait(ulock);
    }
    videoPaused = false;

    // don't reset videoDone
}

} // namespace ikvm
