// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_DOM4_KEYCODE_CONVERTER_H_
#define UI_EVENTS_KEYCODES_DOM4_KEYCODE_CONVERTER_H_

#include <stdint.h>
#include "base/basictypes.h"

// For reference, the W3C UI Event spec is located at:
// http://www.w3.org/TR/uievents/

namespace ui {

enum class DomCode;
enum class DomKey;

// This structure is used to define the keycode mapping table.
// It is defined here because the unittests need access to it.
typedef struct {
  // USB keycode:
  //  Upper 16-bits: USB Usage Page.
  //  Lower 16-bits: USB Usage Id: Assigned ID within this usage page.
  uint32_t usb_keycode;

  // Contains one of the following:
  //  On Linux: XKB scancode
  //  On Windows: Windows OEM scancode
  //  On Mac: Mac keycode
  int native_keycode;

  // The UIEvents (aka: DOM4Events) |code| value as defined in:
  // http://www.w3.org/TR/DOM-Level-3-Events-code/
  const char* code;
} KeycodeMapEntry;

// A class to convert between the current platform's native keycode (scancode)
// and platform-neutral |code| values (as defined in the W3C UI Events
// spec (http://www.w3.org/TR/uievents/).
class KeycodeConverter {
 public:
  // Return the value that identifies an invalid native keycode.
  static int InvalidNativeKeycode();

  // Convert a native (Mac/Win/Linux) keycode into the |code| string.
  // The returned pointer references a static global string.
  static const char* NativeKeycodeToCode(int native_keycode);

  // Convert a native (Mac/Win/Linux) keycode into a DomCode.
  static DomCode NativeKeycodeToDomCode(int native_keycode);

  // Convert a UI Events |code| string value into a native keycode.
  static int CodeToNativeKeycode(const char* code);

  // Convert a DomCode into a native keycode.
  static int DomCodeToNativeKeycode(DomCode code);

  // Convert a UI Events |code| string value into a DomCode.
  static DomCode CodeStringToDomCode(const char* code);

  // Convert a DomCode into a UI Events |code| string value.
  static const char* DomCodeToCodeString(DomCode dom_code);

  // Convert a UI Events |key| string value into a DomKey.
  static DomKey KeyStringToDomKey(const char* key);

  // Convert a DomKey into a UI Events |key| string value.
  static const char* DomKeyToKeyString(DomKey dom_key);

  // The following methods relate to USB keycodes.
  // Note that USB keycodes are not part of any web standard.
  // Please don't use USB keycodes in new code.

  // Return the value that identifies an invalid USB keycode.
  static uint32_t InvalidUsbKeycode();

  // Convert a USB keycode into an equivalent platform native keycode.
  static int UsbKeycodeToNativeKeycode(uint32_t usb_keycode);

  // Convert a platform native keycode into an equivalent USB keycode.
  static uint32_t NativeKeycodeToUsbKeycode(int native_keycode);

  // Convert a USB keycode into the string with the DOM3 |code| value.
  // The returned pointer references a static global string.
  static const char* UsbKeycodeToCode(uint32_t usb_keycode);

  // Convert a USB keycode into a DomCode.
  static DomCode UsbKeycodeToDomCode(uint32_t usb_keycode);

  // Convert a DOM3 Event |code| string into a USB keycode value.
  static uint32_t CodeToUsbKeycode(const char* code);

  // Static methods to support testing.
  static size_t NumKeycodeMapEntriesForTest();
  static const KeycodeMapEntry* GetKeycodeMapForTest();
  static const char* DomKeyStringForTest(size_t index);

 private:
  DISALLOW_COPY_AND_ASSIGN(KeycodeConverter);
};

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_DOM4_KEYCODE_CONVERTER_H_
