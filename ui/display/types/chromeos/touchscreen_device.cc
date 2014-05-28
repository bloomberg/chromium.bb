// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/chromeos/touchscreen_device.h"

namespace ui {

// static
const int TouchscreenDevice::kInvalidId = 0;

TouchscreenDevice::TouchscreenDevice(int id, const gfx::Size& size)
    : id(id),
      size(size) {}

}  // namespace ui
