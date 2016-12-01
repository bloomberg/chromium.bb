// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/display_snapshot_virtual.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "ui/display/types/display_mode.h"

namespace ui {

namespace {

// For a non hi-DPI display (96 dpi) use a pitch of 265µm.
static const float kVirtualDisplayPitchMM = 0.265;

}  // namespace

DisplaySnapshotVirtual::DisplaySnapshotVirtual(int64_t display_id,
                                               const gfx::Size& display_size)
    : DisplaySnapshot(display_id,
                      gfx::Point(0, 0),
                      // Calculate physical size assuming 96dpi display.
                      gfx::Size(display_size.width() * kVirtualDisplayPitchMM,
                                display_size.height() * kVirtualDisplayPitchMM),
                      DISPLAY_CONNECTION_TYPE_VIRTUAL,
                      false,
                      false,
                      false,
                      "Virtual display",
                      base::FilePath(),
                      std::vector<std::unique_ptr<const DisplayMode>>(),
                      std::vector<uint8_t>(),  // Virtual displays have no EDID.
                      nullptr,
                      nullptr) {
  mode_.reset(new DisplayMode(display_size, false, 30));
  modes_.push_back(mode_->Clone());

  native_mode_ = modes_.front().get();
}

DisplaySnapshotVirtual::~DisplaySnapshotVirtual() {}

std::string DisplaySnapshotVirtual::ToString() const {
  return base::StringPrintf(
      "Virtual id=%" PRId64 " current_mode=%s physical_size=%s", display_id_,
      current_mode_ ? current_mode_->ToString().c_str() : "nullptr",
      physical_size_.ToString().c_str());
}

}  // namespace ui
