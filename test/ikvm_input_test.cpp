#include "keymap.hpp"
#include "scancodes.hpp"

#include <rfb/keysym.h>

#include <gtest/gtest.h>

namespace ikvm
{

/*
 * @class KeymapTest
 * @brief Exercises the pure key-translation helpers keyToScancode() and
 *        keyToMod().
 *
 * The helpers are free functions in the ikvm namespace, so the tests reach
 * them through these thin wrappers without any Input instance or USB HID
 * hardware.
 */
class KeymapTest : public ::testing::Test
{
  protected:
    static uint8_t scancode(rfbKeySym key)
    {
        return keyToScancode(key);
    }
    static uint8_t mod(rfbKeySym key)
    {
        return keyToMod(key);
    }
};

// keyToScancode -------------------------------------------------------------

TEST_F(KeymapTest, ScancodeUppercaseLetters)
{
    EXPECT_EQ(scancode(XK_A), USBHID_KEY_A);
    EXPECT_EQ(scancode(XK_Z), USBHID_KEY_Z);
    EXPECT_EQ(scancode(XK_M), USBHID_KEY_M);
}

TEST_F(KeymapTest, ScancodeLowercaseLettersMatchUppercase)
{
    // 'a'..'z' map to the same scancodes as 'A'..'Z' (case folded via & 0x5F)
    EXPECT_EQ(scancode(XK_a), USBHID_KEY_A);
    EXPECT_EQ(scancode(XK_z), USBHID_KEY_Z);
    EXPECT_EQ(scancode(XK_a), scancode(XK_A));
    EXPECT_EQ(scancode(XK_q), scancode(XK_Q));
}

TEST_F(KeymapTest, ScancodeDigits)
{
    EXPECT_EQ(scancode(XK_1), USBHID_KEY_1);
    EXPECT_EQ(scancode(XK_9), USBHID_KEY_9);
    // '0' is handled as a special case, not in the '1'..'9' range
    EXPECT_EQ(scancode(XK_0), USBHID_KEY_0);
}

TEST_F(KeymapTest, ScancodeFunctionKeys)
{
    EXPECT_EQ(scancode(XK_F1), USBHID_KEY_F1);
    EXPECT_EQ(scancode(XK_F12), USBHID_KEY_F12);
    // Keypad function keys alias onto the same scancodes as F1..F4
    EXPECT_EQ(scancode(XK_KP_F1), USBHID_KEY_F1);
    EXPECT_EQ(scancode(XK_KP_F4), USBHID_KEY_F4);
}

TEST_F(KeymapTest, ScancodeKeypadNumbers)
{
    EXPECT_EQ(scancode(XK_KP_1), USBHID_KEY_KP_1);
    EXPECT_EQ(scancode(XK_KP_9), USBHID_KEY_KP_9);
    // KP_0 is a special case outside the KP_1..KP_9 range
    EXPECT_EQ(scancode(XK_KP_0), USBHID_KEY_KP_0);
}

TEST_F(KeymapTest, ScancodePunctuationAndSpecials)
{
    EXPECT_EQ(scancode(XK_Return), USBHID_KEY_RETURN);
    EXPECT_EQ(scancode(XK_Escape), USBHID_KEY_ESC);
    EXPECT_EQ(scancode(XK_BackSpace), USBHID_KEY_BACKSPACE);
    EXPECT_EQ(scancode(XK_Tab), USBHID_KEY_TAB);
    EXPECT_EQ(scancode(XK_space), USBHID_KEY_SPACE);
    EXPECT_EQ(scancode(XK_minus), USBHID_KEY_MINUS);
    EXPECT_EQ(scancode(XK_slash), USBHID_KEY_SLASH);
}

TEST_F(KeymapTest, ScancodeShiftedSymbolsShareBaseKey)
{
    // Shifted punctuation maps to the unshifted key's scancode; the modifier
    // is reported separately by keyToMod().
    EXPECT_EQ(scancode(XK_exclam), USBHID_KEY_1);
    EXPECT_EQ(scancode(XK_at), USBHID_KEY_2);
    EXPECT_EQ(scancode(XK_parenright), USBHID_KEY_0);
    EXPECT_EQ(scancode(XK_underscore), scancode(XK_minus));
    EXPECT_EQ(scancode(XK_question), scancode(XK_slash));
}

TEST_F(KeymapTest, ScancodeNavigationKeysAliasKeypad)
{
    EXPECT_EQ(scancode(XK_Home), USBHID_KEY_HOME);
    EXPECT_EQ(scancode(XK_KP_Home), USBHID_KEY_HOME);
    EXPECT_EQ(scancode(XK_Up), USBHID_KEY_UP);
    EXPECT_EQ(scancode(XK_KP_Up), USBHID_KEY_UP);
    EXPECT_EQ(scancode(XK_Delete), USBHID_KEY_DELETE);
    EXPECT_EQ(scancode(XK_KP_Delete), USBHID_KEY_DELETE);
}

TEST_F(KeymapTest, ScancodeContextMenuKey)
{
    // Regression guard for Context Menu key support.
    EXPECT_EQ(scancode(XK_Menu), USBHID_MENU);
}

TEST_F(KeymapTest, ScancodeUnmappedKeysReturnZero)
{
    // Modifier keys have no scancode (they go through keyToMod instead).
    EXPECT_EQ(scancode(XK_Shift_L), 0);
    EXPECT_EQ(scancode(XK_Control_R), 0);
    // An arbitrary unmapped keysym yields no scancode.
    EXPECT_EQ(scancode(XK_Super_L), 0);
}

// keyToMod ------------------------------------------------------------------

TEST_F(KeymapTest, ModifierShiftAndControl)
{
    EXPECT_EQ(mod(XK_Shift_L), USBHID_MOD_SHIFT_L);
    EXPECT_EQ(mod(XK_Shift_R), USBHID_MOD_SHIFT_R);
    EXPECT_EQ(mod(XK_Control_L), USBHID_MOD_CTRL_L);
    EXPECT_EQ(mod(XK_Control_R), USBHID_MOD_CTRL_R);
}

TEST_F(KeymapTest, ModifierMetaAndAlt)
{
    EXPECT_EQ(mod(XK_Meta_L), USBHID_MOD_META_L);
    EXPECT_EQ(mod(XK_Meta_R), USBHID_MOD_META_R);
    EXPECT_EQ(mod(XK_Alt_L), USBHID_MOD_ALT_L);
    EXPECT_EQ(mod(XK_Alt_R), USBHID_MOD_ALT_R);
}

TEST_F(KeymapTest, ModifierNonModifierKeysReturnZero)
{
    EXPECT_EQ(mod(XK_A), 0);
    EXPECT_EQ(mod(XK_Return), 0);
    EXPECT_EQ(mod(XK_F1), 0);
    EXPECT_EQ(mod(XK_Menu), 0);
}

} // namespace ikvm
