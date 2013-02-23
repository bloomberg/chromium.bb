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

#define USB_KEYMAP(usb, xkb, win, mac) {usb, xkb}
#include "ui/base/keycodes/usb_keycode_map.h"
#undef USB_KEYMAP

}  // anonymous namespace

uint32_t UsbKeyCodeForKeyboardEvent(const WebKeyboardEvent& key_event) {
  // TODO(garykac): This code assumes that on Linux we're receiving events via
  // the XKB driver.  We should detect between "XKB", "kbd" and "evdev" at
  // run-time and re-map accordingly, but that's not possible here, inside the
  // sandbox.
  return NativeKeycodeToUsbKeycode(key_event.nativeKeyCode);
}

}  // namespace ppapi
}  // namespace webkit
