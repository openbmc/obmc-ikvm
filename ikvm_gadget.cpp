#include "ikvm_gadget.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace ikvm
{
static constexpr auto gadgetbase = "kernel/config/usb_gadget";
static constexpr auto keyboardFunction = "hid.0";
static constexpr auto mouseFunction = "hid.1";

static std::filesystem::path
    getKeyboardFunctionDir(const std::filesystem::path& gadgetDir)
{
    return gadgetDir / "functions" / keyboardFunction;
}

static std::filesystem::path
    getMouseFunctionDir(const std::filesystem::path& gadgetDir)
{
    return gadgetDir / "functions" / mouseFunction;
}

static std::filesystem::path
    getGadgetConfigDir(const std::filesystem::path& gadgetDir)
{
    return gadgetDir / "configs" / "c.1";
}

static std::filesystem::path getLocaleDir(const std::filesystem::path& base)
{
    return base / "strings" / "0x409";
}

void writeSysfsAttribute(std::span<const char> data, const char* attribute,
                         const std::filesystem::path& path)
{
    std::ofstream attributeFile;
    attributeFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    attributeFile.open(path / attribute);
    attributeFile.write(data.data(), data.size());
}

static std::vector<std::string>
    getPortsInUse(const std::filesystem::path& sysfsMountPoint)
{
    std::filesystem::path gadgetBaseDir = sysfsMountPoint / gadgetbase;
    std::vector<std::string> inUsePorts;
    std::error_code ec;
    auto gadgets = std::filesystem::directory_iterator(gadgetBaseDir, ec);
    if (ec)
    {
        return {};
    }
    for (const auto& gadget : gadgets)
    {
        const auto gadgetPortPath = gadget.path() / "UDC";
        auto exists = std::filesystem::exists(gadgetPortPath, ec);
        if (!ec && exists)
        {
            std::string port;
            std::ifstream gadgetPortStream(gadgetPortPath);
            gadgetPortStream >> port;
            if (gadgetPortStream.good())
            {
                inUsePorts.push_back(std::move(port));
            }
        }
    }
    return inUsePorts;
}

std::optional<std::string>
    findFreePort(const std::filesystem::path& sysfsMountPoint)
{
    static constexpr auto udcbase = "class/udc";
    std::filesystem::path udcBaseDir = sysfsMountPoint / udcbase;
    std::error_code ec;
    auto it = std::filesystem::directory_iterator(udcBaseDir, ec);
    if (ec)
    {
        return {};
    }
    auto portsInUse = getPortsInUse(sysfsMountPoint);
    for (const auto& port : it)
    {
        auto asString = port.path().filename().string();
        if (std::find(portsInUse.begin(), portsInUse.end(), asString) ==
            portsInUse.end())
        {
            return asString;
        }
    }
    return {};
}

void createHid(const std::filesystem::path& gadgetDir)
{
    // create gadget
    std::filesystem::create_directories(gadgetDir);

    // add basic information
    writeSysfsAttribute("0x0100", "bcdDevice", gadgetDir);
    writeSysfsAttribute("0x0200", "bcdUSB", gadgetDir);
    writeSysfsAttribute("0x0104", "idProduct", gadgetDir);
    writeSysfsAttribute("0x1d6b", "idVendor", gadgetDir);

    // create English locale
    std::filesystem::path localeDir = getLocaleDir(gadgetDir);
    std::filesystem::create_directories(localeDir);
    writeSysfsAttribute("OpenBMC", "manufacturer", localeDir);
    writeSysfsAttribute("Virtual Keyboard and Mouse", "product", localeDir);
    writeSysfsAttribute("OBMC0001", "serialnumber", localeDir);

    // create HID keyboard function
    std::filesystem::path kbdFunctionDir = getKeyboardFunctionDir(gadgetDir);
    std::filesystem::create_directories(kbdFunctionDir);
    writeSysfsAttribute("1", "protocol", kbdFunctionDir);
    writeSysfsAttribute("8", "report_length", kbdFunctionDir);
    writeSysfsAttribute("1", "subclass", kbdFunctionDir);

    // Binary HID keyboard descriptor
    std::array<char, 63> kbdDesc{
        '\x05', '\x01', // USAGE_PAGE (Generic Desktop)
        '\x09', '\x06', // USAGE (Keyboard)
        '\xa1', '\x01', // COLLECTION (Application)
        '\x05', '\x07', //   USAGE_PAGE (Keyboard)
        '\x19', '\xe0', //   USAGE_MINIMUM (Keyboard LeftControl)
        '\x29', '\xe7', //   USAGE_MAXIMUM (Keyboard Right GUI)
        '\x15', '\x00', //   LOGICAL_MINIMUM (0)
        '\x25', '\x01', //   LOGICAL_MAXIMUM (1)
        '\x75', '\x01', //   REPORT_SIZE (1)
        '\x95', '\x08', //   REPORT_COUNT (8)
        '\x81', '\x02', //   INPUT (Data,Var,Abs)
        '\x95', '\x01', //   REPORT_COUNT (1)
        '\x75', '\x08', //   REPORT_SIZE (8)
        '\x81', '\x03', //   INPUT (Data,Var,Abs)
        '\x95', '\x05', //   REPORT_COUNT (5)
        '\x75', '\x01', //   REPORT_SIZE (1)
        '\x05', '\x08', //   USAGE_PAGE (LEDs)
        '\x19', '\x01', //   USAGE_MINIMUM (Num Lock)
        '\x29', '\x05', //   USAGE_MAXIMUM (Kana)
        '\x91', '\x02', //   OUTPUT (Data,Var,Abs)
        '\x95', '\x01', //   REPORT_COUNT (1)
        '\x75', '\x03', //   REPORT_SIZE (3)
        '\x91', '\x03', //   OUTPUT (Cnst,Var,Abs)
        '\x95', '\x06', //   REPORT_COUNT (6)
        '\x75', '\x08', //   REPORT_SIZE (8)
        '\x15', '\x00', //   LOGICAL_MINIMUM (0)
        '\x25', '\x65', //   LOGICAL_MAXIMUM (101)
        '\x05', '\x07', //   USAGE_PAGE (Keyboard)
        '\x19', '\x00', //   USAGE_MINIMUM (Reserved (no event indicated))
        '\x29', '\x65', //   USAGE_MAXIMUM (Keyboard Application)
        '\x81', '\x00', //   INPUT (Data,Ary,Abs)
        '\xc0'};        // END_COLLECTION
    writeSysfsAttribute(kbdDesc, "report_desc", kbdFunctionDir);

    // Create HID mouse function
    std::filesystem::path mouseFunctionDir = getMouseFunctionDir(gadgetDir);
    std::filesystem::create_directories(mouseFunctionDir);
    writeSysfsAttribute("2", "protocol", mouseFunctionDir);
    writeSysfsAttribute("6", "report_length", mouseFunctionDir);
    writeSysfsAttribute("1", "subclass", mouseFunctionDir);

    // Binary HID mouse descriptor (absolute coordinate)
    std::array<char, 76> mouseDesc{
        '\x05', '\x01',         // USAGE_PAGE (Generic Desktop)
        '\x09', '\x02',         // USAGE (Mouse)
        '\xa1', '\x01',         // COLLECTION (Application)
        '\x09', '\x01',         //   USAGE (Pointer)
        '\xa1', '\x00',         //   COLLECTION (Physical)
        '\x05', '\x09',         //     USAGE_PAGE (Button)
        '\x19', '\x01',         //     USAGE_MINIMUM (Button 1)
        '\x29', '\x03',         //     USAGE_MAXIMUM (Button 3)
        '\x15', '\x00',         //     LOGICAL_MINIMUM (0)
        '\x25', '\x01',         //     LOGICAL_MAXIMUM (1)
        '\x95', '\x03',         //     REPORT_COUNT (3)
        '\x75', '\x01',         //     REPORT_SIZE (1)
        '\x81', '\x02',         //     INPUT (Data,Var,Abs)
        '\x95', '\x01',         //     REPORT_COUNT (1)
        '\x75', '\x05',         //     REPORT_SIZE (5)
        '\x81', '\x03',         //     INPUT (Cnst,Var,Abs)
        '\x05', '\x01',         //     USAGE_PAGE (Generic Desktop)
        '\x09', '\x30',         //     USAGE (X)
        '\x09', '\x31',         //     USAGE (Y)
        '\x35', '\x00',         //     PHYSICAL_MINIMUM (0)
        '\x46', '\xff', '\x7f', //     PHYSICAL_MAXIMUM (32767)
        '\x15', '\x00',         //     LOGICAL_MINIMUM (0)
        '\x26', '\xff', '\x7f', //     LOGICAL_MAXIMUM (32767)
        '\x65', '\x11',         //     UNIT (SI Lin:Distance)
        '\x55', '\x00',         //     UNIT_EXPONENT (0)
        '\x75', '\x10',         //     REPORT_SIZE (16)
        '\x95', '\x02',         //     REPORT_COUNT (2)
        '\x81', '\x02',         //     INPUT (Data,Var,Abs)
        '\x09', '\x38',         //     Usage (Wheel)
        '\x15', '\xff',         //     LOGICAL_MINIMUM (-1)
        '\x25', '\x01',         //     LOGICAL_MAXIMUM (1)
        '\x35', '\x00',         //     PHYSICAL_MINIMUM (-127)
        '\x45', '\x00',         //     PHYSICAL_MAXIMUM (127)
        '\x75', '\x08',         //     REPORT_SIZE (8)
        '\x95', '\x01',         //     REPORT_COUNT (1)
        '\x81', '\x06',         //     INPUT (Data,Var,Rel)
        '\xc0',                 //   END_COLLECTION
        '\xc0'};                // END_COLLECTION
    writeSysfsAttribute(mouseDesc, "report_desc", mouseFunctionDir);

    // Create configuration
    std::filesystem::path configDir = getGadgetConfigDir(gadgetDir);
    std::filesystem::create_directories(configDir);
    std::filesystem::path configLocaleDir = getLocaleDir(configDir);
    std::filesystem::create_directories(configLocaleDir);

    writeSysfsAttribute("0xe0", "bmAttributes", configDir);
    writeSysfsAttribute("200", "MaxPower", configDir);
    writeSysfsAttribute("", "configuration", configLocaleDir);

    // Link HID functions to configuration
    std::filesystem::create_directory_symlink(kbdFunctionDir,
                                              configDir / keyboardFunction);
    std::filesystem::create_directory_symlink(mouseFunctionDir,
                                              configDir / mouseFunction);
}

void destroyHid(const std::filesystem::path& gadgetDir)
{
    std::filesystem::remove(getGadgetConfigDir(gadgetDir) / keyboardFunction);
    std::filesystem::remove(getGadgetConfigDir(gadgetDir) / mouseFunction);
    std::filesystem::remove(getKeyboardFunctionDir(gadgetDir));
    std::filesystem::remove(getMouseFunctionDir(gadgetDir));
    std::filesystem::remove(getLocaleDir(getGadgetConfigDir(gadgetDir)));
    std::filesystem::remove(getGadgetConfigDir(gadgetDir));
    std::filesystem::remove(getLocaleDir(gadgetDir));
    std::filesystem::remove(gadgetDir);
}
} // namespace ikvm
