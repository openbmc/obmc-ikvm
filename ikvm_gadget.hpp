#include <filesystem>
#include <optional>
#include <string>

namespace ikvm
{
void writeSysfsAttribute(const char* data, const char* attribute,
                         const std::filesystem::path& path);
std::optional<std::string>
    findFreePort(const std::filesystem::path& sysfsMountPoint);
void createHid(const std::filesystem::path& gadgetDir);
void destroyHid(const std::filesystem::path& gadgetDir);
} // namespace ikvm
