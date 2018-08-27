#pragma once

#include <rfb/rfb.h>

#include <map>
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
     * @param[in] p - Path to the USB input device
     */
    Input(const std::string& p);
    ~Input();
    Input(const Input&) = default;
    Input& operator=(const Input&) = default;
    Input(Input&&) = default;
    Input& operator=(Input&&) = default;

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

    /*
     * @brief Sends a data packet to the USB input device
     *
     * @param[in] data - pointer to data
     * @param[in] size - number of bytes to send
     */
    void sendRaw(char* data, int size);
    /* @brief Sends an HID report to the USB input device */
    void sendReport();

  private:
    enum
    {
        NUM_MODIFIER_BITS = 4,
        POINTER_LENGTH = 6,
        REPORT_LENGTH = 8
    };

    /* @brief Keyboard HID identifier byte */
    static const char keyboardID;
    /* @brief Pointer HID identifier byte */
    static const char pointerID;
    /* @brief HID modifier bits mapped to shift and control key codes */
    static const char shiftCtrlMap[NUM_MODIFIER_BITS];
    /* @brief HID modifier bits mapped to meta and alt key codes */
    static const char metaAltMap[NUM_MODIFIER_BITS];

    /*
     * @brief Translates a RFB-specific key code to HID modifier bit
     *
     * @param[in] key - key code
     */
    static char keyToMod(rfbKeySym key);
    /*
     * @brief Translates a RFB-specific key code to HID scancode
     *
     * @param[in] key - key code
     */
    static char keyToScancode(rfbKeySym key);

    /* @brief Indicates whether or not to send a keyboard report */
    bool sendKeyboard;
    /* @brief Indicates whether or not to send a pointer report */
    bool sendPointer;
    /* @brief File descriptor for the USB input device */
    int fd;
    /* @brief Data for keyboard report */
    char keyboardReport[REPORT_LENGTH];
    /* @brief Data for pointer report */
    char pointerReport[REPORT_LENGTH];
    /* @brief Path to the USB input device */
    std::string path;
    /*
     * @brief Mapping of RFB key code to report data index to keep track
     *        of which keys are down
     */
    std::map<int, int> keysDown;
};

} // namespace ikvm
