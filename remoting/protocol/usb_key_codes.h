// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _REMOTING_PROTOCOL_USB_KEY_CODES_H_
#define _REMOTING_PROTOCOL_USB_KEY_CODES_H_

// Frequently used USB key-codes (not comprehensive--add more as needed).

enum {
  kUsbTab          = 0x07002b,
  kUsbCapsLock     = 0x070039,
  kUsbInsert       = 0x070049,
  kUsbDelete       = 0x07004c,
  kUsbLeftControl  = 0x0700e0,
  kUsbLeftShift    = 0x0700e1,
  kUsbLeftAlt      = 0x0700e2,
  kUsbLeftOs       = 0x0700e3,
  kUsbRightControl = 0x0700e4,
  kUsbRightShift   = 0x0700e5,
  kUsbRightAlt     = 0x0700e6,
  kUsbRightOs      = 0x0700e7
};

#endif
