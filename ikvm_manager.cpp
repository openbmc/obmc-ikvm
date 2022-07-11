#include "ikvm_manager.hpp"

#include <thread>

namespace ikvm
{

Manager::Manager(const Args& args) :
    continueExecuting(true), serverDone(false), videoDone(true),
    input(args.getKeyboardPath(), args.getPointerPath()),
    video(args.getVideoPath(), input, args.getFrameRate(),
          args.getSubsampling()),
    server(args, input, video)
{}

void Manager::run()
{
    std::thread run(serverThread, this);
    std::thread runStatus(statusThread, this);

    io.run();
    run.join();
    runStatus.join();
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

void Manager::statusThread(Manager* manager)
{
    while (manager->continueExecuting)
    {
        if (manager->server.wantsFrame() || manager->shotFlag.load())
        {
            manager->video.start();
            manager->video.getFrame();

            if (manager->server.wantsFrame())
            {
                manager->server.sendFrame();
            }

            if (manager->shotFlag.load())
            {
                manager->video.writeFile(manager->shotPath);
                manager->shotFlag.store(false);
            }
        }
        else
        {
            manager->video.stop();
        }

        if (manager->video.needsResize())
        {
            manager->videoDone = false;
            manager->waitServer();

            manager->video.resize();

            manager->server.resize();
            manager->setVideoDone();
        }
        else
        {
            manager->setVideoDone();
            manager->waitServer();
        }
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

void Manager::waitServer()
{
    std::unique_lock<std::mutex> ulock(lock);

    while (!serverDone)
    {
        sync.wait(ulock);
    }

    serverDone = false;
}

void Manager::waitVideo()
{
    std::unique_lock<std::mutex> ulock(lock);

    while (!videoDone)
    {
        sync.wait(ulock);
    }

    // don't reset videoDone
}

} // namespace ikvm
