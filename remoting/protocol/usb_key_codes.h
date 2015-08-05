// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_USB_KEY_CODES_H_
#define REMOTING_PROTOCOL_USB_KEY_CODES_H_

// Frequently used USB key-codes (not comprehensive--add more as needed).
// TODO(liaoyuke): This could be cleaned up by reusing the data in
// "ui/events/keycodes/dom/keycode_converter_data.inc".

enum {
  kUsbA            = 0x070004,
  kUsbN            = 0x070011,
  kUsbW            = 0x07001a,
  kUsbEnter        = 0x070028,
  kUsbTab          = 0x07002b,
  kUsbCapsLock     = 0x070039,
  kUsbF4           = 0x07003d,
  kUsbInsert       = 0x070049,
  kUsbDelete       = 0x07004c,
  kUsbLeftControl  = 0x0700e0,
  kUsbLeftShift    = 0x0700e1,
  kUsbLeftAlt      = 0x0700e2,
  kUsbLeftOs       = 0x0700e3,
  kUsbRightArrow   = 0x07004f,
  kUsbRightControl = 0x0700e4,
  kUsbRightShift   = 0x0700e5,
  kUsbRightAlt     = 0x0700e6,
  kUsbRightOs      = 0x0700e7
};

#endif  // REMOTING_PROTOCOL_USB_KEY_CODES_H_
