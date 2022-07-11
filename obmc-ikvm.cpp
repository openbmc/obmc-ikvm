#include "ikvm_args.hpp"
#include "ikvm_manager.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

int main(int argc, char* argv[])
{
    ikvm::Args args(argc, argv);
    ikvm::Manager manager(args);
    std::atomic_init(&manager.shotFlag, false);
    constexpr char const* fileName = "/tmp/screenshot.jpg";

    auto bus = std::make_shared<sdbusplus::asio::connection>(manager.io);
    sdbusplus::asio::object_server kvmServer(bus);

    auto interface = kvmServer.add_interface(
        "/xyz/openbmc_project/kvm", "xyz.openbmc_project.kvm_interface");

    interface->register_method("Screenshot", [&manager]() {
            if (!manager.shotFlag.load())
            {
                manager.shotPath = fileName;
                manager.shotFlag.store(true);
                return manager.shotPath;
            }
            return std::string("Screenshot busy");
        }
    );

    interface->initialize();

    bus->request_name("xyz.openbmc_project.kvm_service");

    manager.run();

    return 0;
}
