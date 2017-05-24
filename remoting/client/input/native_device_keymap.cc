// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/native_device_keymap.h"

#include "base/logging.h"

namespace remoting {

// Default implementation.
uint32_t NativeDeviceKeycodeToUsbKeycode(size_t device_keycode) {
  NOTIMPLEMENTED();
  return device_keycode;
}

}  // namespace remoting
