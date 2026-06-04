#pragma once

#include <rfb/rfb.h>

#include <cstdint>

namespace ikvm
{

/*
 * @brief Translates an RFB-specific key code to its HID modifier bit
 *
 * @param[in] key - RFB key code
 * @return HID modifier bit, or 0 if the key is not a modifier
 */
uint8_t keyToMod(rfbKeySym key);

/*
 * @brief Translates an RFB-specific key code to its HID scancode
 *
 * @param[in] key - RFB key code
 * @return HID scancode, or 0 if the key has no scancode mapping
 */
uint8_t keyToScancode(rfbKeySym key);

} // namespace ikvm
