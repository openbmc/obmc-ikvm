#include "keymap.hpp"

#include "scancodes.hpp"

#include <rfb/keysym.h>

#include <utility>

namespace ikvm
{

namespace
{

static constexpr int NUM_MODIFIER_BITS = 8;

/* @brief RFB modifier key symbols mapped to HID modifier byte bit masks */
static constexpr std::pair<rfbKeySym, uint8_t> keyModMap[NUM_MODIFIER_BITS] = {
    {XK_Shift_L, USBHID_MOD_SHIFT_L},
    {XK_Shift_R, USBHID_MOD_SHIFT_R},
    {XK_Control_L, USBHID_MOD_CTRL_L},
    {XK_Control_R, USBHID_MOD_CTRL_R},
    {XK_Meta_L, USBHID_MOD_META_L},
    {XK_Meta_R, USBHID_MOD_META_R},
    {XK_Alt_L, USBHID_MOD_ALT_L},
    {XK_Alt_R, USBHID_MOD_ALT_R},
};

/* @brief Map keys that fall into contiguous key symbol ranges (letters,
 *        digits and function/keypad number keys) to their HID scancode.
 *        Returns 0 when the key is not part of a handled range.
 */
uint8_t rangeToScancode(rfbKeySym key)
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

    return scancode;
}

/* @brief Map printable punctuation and shifted number-row symbols to their
 *        HID scancode. Returns 0 when the key is not a handled symbol.
 */
uint8_t symbolToScancode(rfbKeySym key)
{
    uint8_t scancode = 0;

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
    }

    return scancode;
}

/* @brief Map editing and cursor navigation keys (and their keypad aliases) to
 *        their HID scancode. Returns 0 when the key is not a navigation key.
 */
uint8_t navigationToScancode(rfbKeySym key)
{
    uint8_t scancode = 0;

    switch (key)
    {
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
    }

    return scancode;
}

/* @brief Map control, lock, keypad operator and menu keys to their HID
 *        scancode. Returns 0 when the key is not handled here.
 */
uint8_t specialToScancode(rfbKeySym key)
{
    uint8_t scancode = 0;

    switch (key)
    {
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
        case XK_Menu:
            scancode = USBHID_MENU;
            break;
    }

    return scancode;
}

} // namespace

uint8_t keyToMod(rfbKeySym key)
{
    uint8_t mod = 0;

    for (const auto& [keySym, modBit] : keyModMap)
    {
        if (key == keySym)
        {
            mod = modBit;
            break;
        }
    }

    return mod;
}

uint8_t keyToScancode(rfbKeySym key)
{
    uint8_t scancode = rangeToScancode(key);

    if (scancode == 0)
    {
        scancode = symbolToScancode(key);
    }
    if (scancode == 0)
    {
        scancode = navigationToScancode(key);
    }
    if (scancode == 0)
    {
        scancode = specialToScancode(key);
    }

    return scancode;
}

} // namespace ikvm
