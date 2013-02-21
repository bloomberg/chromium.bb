// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/usb_key_code_conversion.h"

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

using WebKit::WebKeyboardEvent;

namespace webkit {
namespace ppapi {

namespace {

#define USB_KEYMAP(usb, xkb, win, mac) {usb, mac}
#include "ui/base/keycodes/usb_keycode_map.h"
#undef USB_KEYMAP

} // anonymous namespace

uint32_t UsbKeyCodeForKeyboardEvent(const WebKeyboardEvent& key_event) {
  if (key_event.nativeKeyCode < 0 || key_event.nativeKeyCode > 0x7f)
    return 0;

  for (size_t i = 0; i < arraysize(usb_keycode_map); i++) {
    if (usb_keycode_map[i].native_keycode == key_event.nativeKeyCode)
      return usb_keycode_map[i].usb_keycode;
  }

  return 0;
}

}  // namespace ppapi
}  // namespace webkit
