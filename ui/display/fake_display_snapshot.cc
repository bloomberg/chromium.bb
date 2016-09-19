// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/fake_display_snapshot.h"

#include <inttypes.h>

#include <vector>

#include "base/strings/stringprintf.h"

namespace display {

namespace {

// For a non high-DPI display (96 dpi) use a pitch of 265µm.
static const float kDisplayPitchMM = 0.265f;

}  // namespace

FakeDisplaySnapshot::FakeDisplaySnapshot(int64_t display_id,
                                         const gfx::Size& display_size,
                                         ui::DisplayConnectionType type,
                                         std::string name)
    : DisplaySnapshot(display_id,
                      gfx::Point(0, 0),
                      gfx::Size(display_size.width() * kDisplayPitchMM,
                                display_size.height() * kDisplayPitchMM),
                      type,
                      false,
                      false,
                      false,
                      name,
                      base::FilePath(),
                      std::vector<std::unique_ptr<const ui::DisplayMode>>(),
                      std::vector<uint8_t>(),
                      nullptr,
                      nullptr) {
  ui::DisplayMode mode(display_size, false, 60.0f);
  modes_.push_back(mode.Clone());

  native_mode_ = modes_.front().get();
}

FakeDisplaySnapshot::~FakeDisplaySnapshot() {}

std::string FakeDisplaySnapshot::ToString() const {
  return base::StringPrintf(
      "id=%" PRId64 " current_mode=%s physical_size=%s", display_id_,
      current_mode_ ? current_mode_->ToString().c_str() : "nullptr",
      physical_size_.ToString().c_str());
}

}  // namespace display
