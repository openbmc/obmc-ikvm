#include "ikvm_gadget.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace ikvm
{
static constexpr auto gadgetbase = "kernel/config/usb_gadget";
static constexpr auto keyboardFunction = "hid.0";
static constexpr auto mouseFunction = "hid.1";

std::filesystem::path
    getKeyboardFunctionDir(const std::filesystem::path& gadgetDir)
{
    return gadgetDir / "functions" / keyboardFunction;
}

std::filesystem::path
    getMouseFunctionDir(const std::filesystem::path& gadgetDir)
{
    return gadgetDir / "functions" / mouseFunction;
}

std::filesystem::path getGadgetConfigDir(const std::filesystem::path& gadgetDir)
{
    return gadgetDir / "configs" / "c.1";
}

std::filesystem::path getLocaleDir(const std::filesystem::path& base)
{
    return base / "strings" / "0x409";
}

void writeSysfsAttribute(const char* data, const char* attribute,
                         const std::filesystem::path& path)
{
    std::ofstream attributeFile;
    attributeFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    attributeFile.open(path / attribute);
    attributeFile << data << "\n" << std::flush;
}

void writeRawSysfsAttribute(std::span<const char> data, const char* attribute,
                            const std::filesystem::path& path)
{
    std::ofstream attributeFile;
    attributeFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    attributeFile.open(path / attribute);
    attributeFile.write(data.data(), data.size());
}

std::vector<std::string>
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
        if (portsInUse.end() ==
            std::find(portsInUse.begin(), portsInUse.end(), asString))
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
    //  0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    //  0x09, 0x06, // USAGE (Keyboard)
    //  0xa1, 0x01, // COLLECTION (Application)
    //  0x05, 0x07, //   USAGE_PAGE (Keyboard)
    //  0x19, 0xe0, //   USAGE_MINIMUM (Keyboard LeftControl)
    //  0x29, 0xe7, //   USAGE_MAXIMUM (Keyboard Right GUI)
    //  0x15, 0x00, //   LOGICAL_MINIMUM (0)
    //  0x25, 0x01, //   LOGICAL_MAXIMUM (1)
    //  0x75, 0x01, //   REPORT_SIZE (1)
    //  0x95, 0x08, //   REPORT_COUNT (8)
    //  0x81, 0x02, //   INPUT (Data,Var,Abs)
    //  0x95, 0x01, //   REPORT_COUNT (1)
    //  0x75, 0x08, //   REPORT_SIZE (8)
    //  0x81, 0x03, //   INPUT (Data,Var,Abs)
    //  0x95, 0x05, //   REPORT_COUNT (5)
    //  0x75, 0x01, //   REPORT_SIZE (1)
    //  0x05, 0x08, //   USAGE_PAGE (LEDs)
    //  0x19, 0x01, //   USAGE_MINIMUM (Num Lock)
    //  0x29, 0x05, //   USAGE_MAXIMUM (Kana)
    //  0x91, 0x02, //   OUTPUT (Data,Var,Abs)
    //  0x95, 0x01, //   REPORT_COUNT (1)
    //  0x75, 0x03, //   REPORT_SIZE (3)
    //  0x91, 0x03, //   OUTPUT (Cnst,Var,Abs)
    //  0x95, 0x06, //   REPORT_COUNT (6)
    //  0x75, 0x08, //   REPORT_SIZE (8)
    //  0x15, 0x00, //   LOGICAL_MINIMUM (0)
    //  0x25, 0x65, //   LOGICAL_MAXIMUM (101)
    //  0x05, 0x07, //   USAGE_PAGE (Keyboard)
    //  0x19, 0x00, //   USAGE_MINIMUM (Reserved (no event indicated))
    //  0x29, 0x65, //   USAGE_MAXIMUM (Keyboard Application)
    //  0x81, 0x00, //   INPUT (Data,Ary,Abs)
    //  0xc0        // END_COLLECTION
    std::array<char, 63> kbdDesc{
        '\x05', '\x01', '\x09', '\x06', '\xa1', '\x01', '\x05', '\x07', '\x19',
        '\xe0', '\x29', '\xe7', '\x15', '\x00', '\x25', '\x01', '\x75', '\x01',
        '\x95', '\x08', '\x81', '\x02', '\x95', '\x01', '\x75', '\x08', '\x81',
        '\x03', '\x95', '\x05', '\x75', '\x01', '\x05', '\x08', '\x19', '\x01',
        '\x29', '\x05', '\x91', '\x02', '\x95', '\x01', '\x75', '\x03', '\x91',
        '\x03', '\x95', '\x06', '\x75', '\x08', '\x15', '\x00', '\x25', '\x65',
        '\x05', '\x07', '\x19', '\x00', '\x29', '\x65', '\x81', '\x00', '\xc0'};
    writeRawSysfsAttribute(kbdDesc, "report_desc", kbdFunctionDir);

    // Create HID mouse function
    std::filesystem::path mouseFunctionDir = getMouseFunctionDir(gadgetDir);
    std::filesystem::create_directories(mouseFunctionDir);
    writeSysfsAttribute("2", "protocol", mouseFunctionDir);
    writeSysfsAttribute("6", "report_length", mouseFunctionDir);
    writeSysfsAttribute("1", "subclass", mouseFunctionDir);

    // Binary HID mouse descriptor (absolute coordinate)
    //  0x05, 0x01,       // USAGE_PAGE (Generic Desktop)
    //  0x09, 0x02,       // USAGE (Mouse)
    //  0xa1, 0x01,       // COLLECTION (Application)
    //  0x09, 0x01,       //   USAGE (Pointer)
    //  0xa1, 0x00,       //   COLLECTION (Physical)
    //  0x05, 0x09,       //     USAGE_PAGE (Button)
    //  0x19, 0x01,       //     USAGE_MINIMUM (Button 1)
    //  0x29, 0x03,       //     USAGE_MAXIMUM (Button 3)
    //  0x15, 0x00,       //     LOGICAL_MINIMUM (0)
    //  0x25, 0x01,       //     LOGICAL_MAXIMUM (1)
    //  0x95, 0x03,       //     REPORT_COUNT (3)
    //  0x75, 0x01,       //     REPORT_SIZE (1)
    //  0x81, 0x02,       //     INPUT (Data,Var,Abs)
    //  0x95, 0x01,       //     REPORT_COUNT (1)
    //  0x75, 0x05,       //     REPORT_SIZE (5)
    //  0x81, 0x03,       //     INPUT (Cnst,Var,Abs)
    //  0x05, 0x01,       //     USAGE_PAGE (Generic Desktop)
    //  0x09, 0x30,       //     USAGE (X)
    //  0x09, 0x31,       //     USAGE (Y)
    //  0x35, 0x00,       //     PHYSICAL_MINIMUM (0)
    //  0x46, 0xff, 0x7f, //     PHYSICAL_MAXIMUM (32767)
    //  0x15, 0x00,       //     LOGICAL_MINIMUM (0)
    //  0x26, 0xff, 0x7f, //     LOGICAL_MAXIMUM (32767)
    //  0x65, 0x11,       //     UNIT (SI Lin:Distance)
    //  0x55, 0x00,       //     UNIT_EXPONENT (0)
    //  0x75, 0x10,       //     REPORT_SIZE (16)
    //  0x95, 0x02,       //     REPORT_COUNT (2)
    //  0x81, 0x02,       //     INPUT (Data,Var,Abs)
    //  0x09, 0x38,       //     Usage (Wheel)
    //  0x15, 0xff,       //     LOGICAL_MINIMUM (-1)
    //  0x25, 0x01,       //     LOGICAL_MAXIMUM (1)
    //  0x35, 0x00,       //     PHYSICAL_MINIMUM (-127)
    //  0x45, 0x00,       //     PHYSICAL_MAXIMUM (127)
    //  0x75, 0x08,       //     REPORT_SIZE (8)
    //  0x95, 0x01,       //     REPORT_COUNT (1)
    //  0x81, 0x06,       //     INPUT (Data,Var,Rel)
    //  0xc0,             //   END_COLLECTION
    //  0xc0              // END_COLLECTION
    std::array<char, 76> mouseDesc{
        '\x05', '\x01', '\x09', '\x02', '\xa1', '\x01', '\x09', '\x01', '\xa1',
        '\x00', '\x05', '\x09', '\x19', '\x01', '\x29', '\x03', '\x15', '\x00',
        '\x25', '\x01', '\x95', '\x03', '\x75', '\x01', '\x81', '\x02', '\x95',
        '\x01', '\x75', '\x05', '\x81', '\x03', '\x05', '\x01', '\x09', '\x30',
        '\x09', '\x31', '\x35', '\x00', '\x46', '\xff', '\x7f', '\x15', '\x00',
        '\x26', '\xff', '\x7f', '\x65', '\x11', '\x55', '\x00', '\x75', '\x10',
        '\x95', '\x02', '\x81', '\x02', '\x09', '\x38', '\x15', '\xff', '\x25',
        '\x01', '\x35', '\x00', '\x45', '\x00', '\x75', '\x08', '\x95', '\x01',
        '\x81', '\x06', '\xc0', '\xc0'};
    writeRawSysfsAttribute(mouseDesc, "report_desc", mouseFunctionDir);

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
