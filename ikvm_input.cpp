#include "ikvm_input.hpp"

#include "ikvm_server.hpp"
#include "scancodes.hpp"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <rfb/keysym.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>

namespace fs = std::filesystem;

namespace ikvm
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;

Input::Input(const std::string& kbdPath, const std::string& ptrPath,
             const std::string& udc) :
    keyboardFd(-1),
    pointerFd(-1), keyboardReport{0}, pointerReport{0}, keyboardPath(kbdPath),
    pointerPath(ptrPath), udcName(udc)
{
    hidUdcStream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    hidUdcStream.open(hidUdcPath, std::ios::out | std::ios::app);
}

Input::~Input()
{
    if (keyboardFd >= 0)
    {
        close(keyboardFd);
    }

    if (pointerFd >= 0)
    {
        close(pointerFd);
    }

    disconnect();
    hidUdcStream.close();
}

void Input::connect()
{
    try
    {
        if (udcName.empty())
        {
            bool found = false;
            for (const auto& port :
                fs::directory_iterator(usbVirtualHubPath))
            {
                // /sys/bus/platform/devices/1e6a0000.usb-vhub/1e6a0000.usb-vhub:pX
                if (fs::is_directory(port) && !fs::is_symlink(port))
                {
                    for (const auto& gadget : fs::directory_iterator(port.path())) {
                        /*
                        Kernel 6.0:
                        /sys/.../1e6a0000.usb-vhub:pX/gadget.Y/suspended
                        Kernel 5.15:
                        /sys/.../1e6a0000.usb-vhub:pX/gadget/suspended
                        */
                        if (fs::is_directory(gadget) &&
                            gadget.path().string().find("gadget") !=
                                std::string::npos &&
                            !fs::exists(gadget.path() / "suspended"))
                        {
                            const std::string portId = gadget.path().filename();
                            hidUdcStream << portId << std::endl;
                            found = true;
                            break;
                        }
                    }
                }
                if (found)
                    break;
            }
        }
        else // If UDC has been specified by '-u' parameter, connect to it.
        {
            hidUdcStream << udcName << std::endl;
        }
    }
    catch (fs::filesystem_error& e)
    {
        log<level::ERR>("Failed to search USB virtual hub port",
                        entry("ERROR=%s", e.what()));
        return;
    }
    catch (std::ofstream::failure& e)
    {
        log<level::ERR>("Failed to connect HID gadget",
                        entry("ERROR=%s", e.what()));
        return;
    }

    if (!keyboardPath.empty())
    {
        keyboardFd =
            open(keyboardPath.c_str(), O_RDWR | O_CLOEXEC | O_NONBLOCK);
        if (keyboardFd < 0)
        {
            log<level::ERR>("Failed to open input device",
                            entry("PATH=%s", keyboardPath.c_str()),
                            entry("ERROR=%s", strerror(errno)));
            elog<Open>(xyz::openbmc_project::Common::File::Open::ERRNO(errno),
                       xyz::openbmc_project::Common::File::Open::PATH(
                           keyboardPath.c_str()));
        }
    }

    if (!pointerPath.empty())
    {
        pointerFd = open(pointerPath.c_str(), O_RDWR | O_CLOEXEC | O_NONBLOCK);
        if (pointerFd < 0)
        {
            log<level::ERR>("Failed to open input device",
                            entry("PATH=%s", pointerPath.c_str()),
                            entry("ERROR=%s", strerror(errno)));
            elog<Open>(xyz::openbmc_project::Common::File::Open::ERRNO(errno),
                       xyz::openbmc_project::Common::File::Open::PATH(
                           pointerPath.c_str()));
        }
    }
}

void Input::disconnect()
{
    if (keyboardFd >= 0)
    {
        close(keyboardFd);
        keyboardFd = -1;
    }

    if (pointerFd >= 0)
    {
        close(pointerFd);
        pointerFd = -1;
    }

    try
    {
        hidUdcStream << "" << std::endl;
    }
    catch (std::ofstream::failure& e)
    {
        log<level::ERR>("Failed to disconnect HID gadget",
                        entry("ERROR=%s", e.what()));
    }
}

void Input::keyEvent(rfbBool down, rfbKeySym key, rfbClientPtr cl)
{
    Server::ClientData* cd = (Server::ClientData*)cl->clientData;
    Input* input = cd->input;
    bool sendKeyboard = false;

    if (input->keyboardFd < 0)
    {
        return;
    }

    if (down)
    {
        uint8_t sc = keyToScancode(key);

        if (sc)
        {
            if (input->keysDown.find(key) == input->keysDown.end())
            {
                for (unsigned int i = 2; i < KEY_REPORT_LENGTH; ++i)
                {
                    if (!input->keyboardReport[i])
                    {
                        input->keyboardReport[i] = sc;
                        input->keysDown.insert(std::make_pair(key, i));
                        sendKeyboard = true;
                        break;
                    }
                }
            }
        }
        else
        {
            uint8_t mod = keyToMod(key);

            if (mod)
            {
                input->keyboardReport[0] |= mod;
                sendKeyboard = true;
            }
        }
    }
    else
    {
        auto it = input->keysDown.find(key);

        if (it != input->keysDown.end())
        {
            input->keyboardReport[it->second] = 0;
            input->keysDown.erase(it);
            sendKeyboard = true;
        }
        else
        {
            uint8_t mod = keyToMod(key);

            if (mod)
            {
                input->keyboardReport[0] &= ~mod;
                sendKeyboard = true;
            }
        }
    }

    if (sendKeyboard)
    {
        input->writeKeyboard(input->keyboardReport);
    }
}

void Input::pointerEvent(int buttonMask, int x, int y, rfbClientPtr cl)
{
    Server::ClientData* cd = (Server::ClientData*)cl->clientData;
    Input* input = cd->input;
    Server* server = (Server*)cl->screen->screenData;
    const Video& video = server->getVideo();

    if (input->pointerFd < 0)
    {
        return;
    }

    if (buttonMask > 4)
    {
        input->pointerReport[0] = 0;
        if (buttonMask == 8)
        {
            input->pointerReport[5] = 1;
        }
        else if (buttonMask == 16)
        {
            input->pointerReport[5] = 0xff;
        }
    }
    else
    {
        input->pointerReport[0] = ((buttonMask & 0x4) >> 1) |
                                  ((buttonMask & 0x2) << 1) |
                                  (buttonMask & 0x1);
        input->pointerReport[5] = 0;
    }

    if (x >= 0 && (unsigned int)x < video.getWidth())
    {
        uint16_t xx = (uint16_t)(x * (SHRT_MAX + 1) / video.getWidth());

        memcpy(&input->pointerReport[1], &xx, 2);
    }

    if (y >= 0 && (unsigned int)y < video.getHeight())
    {
        uint16_t yy = (uint16_t)(y * (SHRT_MAX + 1) / video.getHeight());

        memcpy(&input->pointerReport[3], &yy, 2);
    }

    rfbDefaultPtrAddEvent(buttonMask, x, y, cl);
    input->writePointer(input->pointerReport);
}

void Input::sendWakeupPacket()
{
    uint8_t wakeupReport[KEY_REPORT_LENGTH] = {0};

    if (pointerFd >= 0)
    {
        uint16_t xy = SHRT_MAX / 2;

        memcpy(&wakeupReport[1], &xy, 2);
        memcpy(&wakeupReport[3], &xy, 2);

        writePointer(wakeupReport);
    }

    if (keyboardFd >= 0)
    {
        memset(&wakeupReport[0], 0, KEY_REPORT_LENGTH);

        wakeupReport[0] = keyToMod(XK_Shift_L);

        if (!writeKeyboard(wakeupReport))
        {
            return;
        }

        wakeupReport[0] = 0;

        writeKeyboard(wakeupReport);
    }
}

uint8_t Input::keyToMod(rfbKeySym key)
{
    uint8_t mod = 0;

    if (key >= XK_Shift_L && key <= XK_Control_R)
    {
        mod = shiftCtrlMap[key - XK_Shift_L];
    }
    else if (key >= XK_Meta_L && key <= XK_Alt_R)
    {
        mod = metaAltMap[key - XK_Meta_L];
    }

    return mod;
}

uint8_t Input::keyToScancode(rfbKeySym key)
{
    uint8_t scancode = 0;

    if ((key >= 'A' && key <= 'Z') || (key >= 'a' && key <= 'z'))
    {
        scancode = USBHID_KEY_A + ((key & 0x5F) - 'A');
    }
    else if (key >= '1' && key <= '9')
    {
        scancode = USBHID_KEY_1 + (key - '1');
    }
    else if (key >= XK_F1 && key <= XK_F12)
    {
        scancode = USBHID_KEY_F1 + (key - XK_F1);
    }
    else if (key >= XK_KP_F1 && key <= XK_KP_F4)
    {
        scancode = USBHID_KEY_F1 + (key - XK_KP_F1);
    }
    else if (key >= XK_KP_1 && key <= XK_KP_9)
    {
        scancode = USBHID_KEY_KP_1 + (key - XK_KP_1);
    }
    else
    {
        switch (key)
        {
            case XK_exclam:
                scancode = USBHID_KEY_1;
                break;
            case XK_at:
                scancode = USBHID_KEY_2;
                break;
            case XK_numbersign:
                scancode = USBHID_KEY_3;
                break;
            case XK_dollar:
                scancode = USBHID_KEY_4;
                break;
            case XK_percent:
                scancode = USBHID_KEY_5;
                break;
            case XK_asciicircum:
                scancode = USBHID_KEY_6;
                break;
            case XK_ampersand:
                scancode = USBHID_KEY_7;
                break;
            case XK_asterisk:
                scancode = USBHID_KEY_8;
                break;
            case XK_parenleft:
                scancode = USBHID_KEY_9;
                break;
            case XK_0:
            case XK_parenright:
                scancode = USBHID_KEY_0;
                break;
            case XK_Return:
                scancode = USBHID_KEY_RETURN;
                break;
            case XK_Escape:
                scancode = USBHID_KEY_ESC;
                break;
            case XK_BackSpace:
                scancode = USBHID_KEY_BACKSPACE;
                break;
            case XK_Tab:
            case XK_KP_Tab:
                scancode = USBHID_KEY_TAB;
                break;
            case XK_space:
            case XK_KP_Space:
                scancode = USBHID_KEY_SPACE;
                break;
            case XK_minus:
            case XK_underscore:
                scancode = USBHID_KEY_MINUS;
                break;
            case XK_plus:
            case XK_equal:
                scancode = USBHID_KEY_EQUAL;
                break;
            case XK_bracketleft:
            case XK_braceleft:
                scancode = USBHID_KEY_LEFTBRACE;
                break;
            case XK_bracketright:
            case XK_braceright:
                scancode = USBHID_KEY_RIGHTBRACE;
                break;
            case XK_backslash:
            case XK_bar:
                scancode = USBHID_KEY_BACKSLASH;
                break;
            case XK_colon:
            case XK_semicolon:
                scancode = USBHID_KEY_SEMICOLON;
                break;
            case XK_quotedbl:
            case XK_apostrophe:
                scancode = USBHID_KEY_APOSTROPHE;
                break;
            case XK_grave:
            case XK_asciitilde:
                scancode = USBHID_KEY_GRAVE;
                break;
            case XK_comma:
            case XK_less:
                scancode = USBHID_KEY_COMMA;
                break;
            case XK_period:
            case XK_greater:
                scancode = USBHID_KEY_DOT;
                break;
            case XK_slash:
            case XK_question:
                scancode = USBHID_KEY_SLASH;
                break;
            case XK_Caps_Lock:
                scancode = USBHID_KEY_CAPSLOCK;
                break;
            case XK_Print:
                scancode = USBHID_KEY_PRINT;
                break;
            case XK_Scroll_Lock:
                scancode = USBHID_KEY_SCROLLLOCK;
                break;
            case XK_Pause:
                scancode = USBHID_KEY_PAUSE;
                break;
            case XK_Insert:
            case XK_KP_Insert:
                scancode = USBHID_KEY_INSERT;
                break;
            case XK_Home:
            case XK_KP_Home:
                scancode = USBHID_KEY_HOME;
                break;
            case XK_Page_Up:
            case XK_KP_Page_Up:
                scancode = USBHID_KEY_PAGEUP;
                break;
            case XK_Delete:
            case XK_KP_Delete:
                scancode = USBHID_KEY_DELETE;
                break;
            case XK_End:
            case XK_KP_End:
                scancode = USBHID_KEY_END;
                break;
            case XK_Page_Down:
            case XK_KP_Page_Down:
                scancode = USBHID_KEY_PAGEDOWN;
                break;
            case XK_Right:
            case XK_KP_Right:
                scancode = USBHID_KEY_RIGHT;
                break;
            case XK_Left:
            case XK_KP_Left:
                scancode = USBHID_KEY_LEFT;
                break;
            case XK_Down:
            case XK_KP_Down:
                scancode = USBHID_KEY_DOWN;
                break;
            case XK_Up:
            case XK_KP_Up:
                scancode = USBHID_KEY_UP;
                break;
            case XK_Num_Lock:
                scancode = USBHID_KEY_NUMLOCK;
                break;
            case XK_KP_Enter:
                scancode = USBHID_KEY_KP_ENTER;
                break;
            case XK_KP_Equal:
                scancode = USBHID_KEY_KP_EQUAL;
                break;
            case XK_KP_Multiply:
                scancode = USBHID_KEY_KP_MULTIPLY;
                break;
            case XK_KP_Add:
                scancode = USBHID_KEY_KP_ADD;
                break;
            case XK_KP_Subtract:
                scancode = USBHID_KEY_KP_SUBTRACT;
                break;
            case XK_KP_Decimal:
                scancode = USBHID_KEY_KP_DECIMAL;
                break;
            case XK_KP_Divide:
                scancode = USBHID_KEY_KP_DIVIDE;
                break;
            case XK_KP_0:
                scancode = USBHID_KEY_KP_0;
                break;
        }
    }

    return scancode;
}

bool Input::writeKeyboard(const uint8_t* report)
{
    std::unique_lock<std::mutex> lk(keyMutex);
    uint retryCount = HID_REPORT_RETRY_MAX;

    while (retryCount > 0)
    {
        if (write(keyboardFd, report, KEY_REPORT_LENGTH) == KEY_REPORT_LENGTH)
        {
            return true;
        }

        if (errno != EAGAIN)
        {
            if (errno != ESHUTDOWN)
            {
                log<level::ERR>("Failed to write keyboard report",
                                entry("ERROR=%s", strerror(errno)));
            }

            break;
        }

        lk.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        lk.lock();
        retryCount--;
    }

    return false;
}

void Input::writePointer(const uint8_t* report)
{
    std::unique_lock<std::mutex> lk(ptrMutex);
    uint retryCount = HID_REPORT_RETRY_MAX;

    while (retryCount > 0)
    {
        if (write(pointerFd, report, PTR_REPORT_LENGTH) == PTR_REPORT_LENGTH)
        {
            break;
        }

        if (errno != EAGAIN)
        {
            if (errno != ESHUTDOWN)
            {
                log<level::ERR>("Failed to write pointer report",
                                entry("ERROR=%s", strerror(errno)));
            }

            break;
        }

        lk.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        lk.lock();
        retryCount--;
    }
}

} // namespace ikvm
