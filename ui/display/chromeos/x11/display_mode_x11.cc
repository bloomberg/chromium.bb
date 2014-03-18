// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/x11/display_mode_x11.h"

namespace ui {

DisplayModeX11::DisplayModeX11(const gfx::Size& size,
                               bool interlaced,
                               float refresh_rate,
                               RRMode mode_id)
    : DisplayMode(size, interlaced, refresh_rate),
      mode_id_(mode_id) {}

DisplayModeX11::~DisplayModeX11() {}

}  // namespace ui
