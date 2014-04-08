// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/test/test_display_snapshot.h"

namespace ui {
TestDisplaySnapshot::TestDisplaySnapshot()
    : DisplaySnapshot(0,
                      false,
                      gfx::Point(0, 0),
                      gfx::Size(0, 0),
                      DISPLAY_CONNECTION_TYPE_UNKNOWN,
                      false,
                      false,
                      std::string(),
                      std::vector<const DisplayMode*>(),
                      NULL,
                      NULL) {}

TestDisplaySnapshot::TestDisplaySnapshot(
    int64_t display_id,
    bool has_proper_display_id,
    const gfx::Point& origin,
    const gfx::Size& physical_size,
    DisplayConnectionType type,
    bool is_aspect_preserving_scaling,
    const std::vector<const DisplayMode*>& modes,
    const DisplayMode* current_mode,
    const DisplayMode* native_mode)
    : DisplaySnapshot(display_id,
                      has_proper_display_id,
                      origin,
                      physical_size,
                      type,
                      is_aspect_preserving_scaling,
                      false,
                      std::string(),
                      modes,
                      current_mode,
                      native_mode) {}

TestDisplaySnapshot::~TestDisplaySnapshot() {}

std::string TestDisplaySnapshot::ToString() const { return ""; }

}  // namespace ui
