#pragma once

#include <rfb/rfb.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>

namespace ikvm
{
/*
 * @class Input
 * @brief Receives events from RFB clients and sends reports to the USB input
 *        device
 */
class Input
{
  public:
    /*
     * @brief Constructs Input object
     *
     * @param[in] kbdPath - Path to the USB keyboard device
     * @param[in] ptrPath - Path to the USB mouse device
     * @param[in] udc - Name of UDC
     */
    Input(const std::string& kbdPath, const std::string& ptrPath,
          const std::string& udc);
    ~Input();
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(Input&&) = delete;

    /* @brief Connects HID gadget to host */
    void connect();
    /* @brief Disconnects HID gadget from host */
    void disconnect();
    /*
     * @brief RFB client key event handler
     *
     * @param[in] down - Boolean indicating whether key is pressed or not
     * @param[in] key  - Key code
     * @param[in] cl   - Handle to the RFB client
     */
    static void keyEvent(rfbBool down, rfbKeySym key, rfbClientPtr cl);
    /*
     * @brief RFB client pointer event handler
     *
     * @param[in] buttonMask - Bitmask indicating which buttons have been
     *                         pressed
     * @param[in] x          - Pointer x-coordinate
     * @param[in] y          - Pointer y-coordinate
     * @param[in] cl         - Handle to the RFB client
     */
    static void pointerEvent(int buttonMask, int x, int y, rfbClientPtr cl);

    /* @brief Sends a wakeup data packet to the USB input device */
    void sendWakeupPacket();

  private:
    static constexpr int KEY_REPORT_LENGTH = 8;
    static constexpr int PTR_REPORT_LENGTH = 6;

    /* @brief Path to the HID gadget UDC */
    static constexpr const char* hidUdcPath =
        "/sys/kernel/config/usb_gadget/obmc_hid/UDC";
    /* @brief Path to the USB virtual hub */
    static constexpr const char* usbVirtualHubPath =
        "/sys/bus/platform/devices/1e6a0000.usb-vhub";
    /* @brief Retry limit for writing an HID report */
    static constexpr int HID_REPORT_RETRY_MAX = 5;

    bool writeKeyboard(const uint8_t* report);
    void writePointer(const uint8_t* report);

    /* @brief File descriptor for the USB keyboard device */
    int keyboardFd;
    /* @brief File descriptor for the USB mouse device */
    int pointerFd;
    /* @brief Data for keyboard report */
    uint8_t keyboardReport[KEY_REPORT_LENGTH];
    /* @brief Data for pointer report */
    uint8_t pointerReport[PTR_REPORT_LENGTH];
    /* @brief Path to the USB keyboard device */
    std::string keyboardPath;
    /* @brief Path to the USB mouse device */
    std::string pointerPath;
    /* @brief Name of UDC */
    std::string udcName;
    /*
     * @brief Mapping of RFB key code to report data index to keep track
     *        of which keys are down
     */
    std::map<int, int> keysDown;
    /* @brief Handle of the HID gadget UDC */
    std::ofstream hidUdcStream;
    /* @brief Mutex for sending keyboard reports */
    std::mutex keyMutex;
    /* @brief Mutex for sending pointer reports */
    std::mutex ptrMutex;
    /* @brief Mutex for key events */
    std::mutex keyEventMutex;
};

} // namespace ikvm
