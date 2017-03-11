// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_snapshot_mojo.h"

#include "base/memory/ptr_util.h"
#include "ui/display/types/display_constants.h"

namespace display {

DisplaySnapshotMojo::DisplaySnapshotMojo(int64_t display_id,
                                         const gfx::Point& origin,
                                         const gfx::Size& physical_size,
                                         DisplayConnectionType type,
                                         bool is_aspect_preserving_scaling,
                                         bool has_overscan,
                                         bool has_color_correction_matrix,
                                         std::string display_name,
                                         const base::FilePath& sys_path,
                                         int64_t product_id,
                                         DisplayModeList modes,
                                         const std::vector<uint8_t>& edid,
                                         const DisplayMode* current_mode,
                                         const DisplayMode* native_mode,
                                         const gfx::Size& maximum_cursor_size)
    : DisplaySnapshot(display_id,
                      origin,
                      physical_size,
                      type,
                      is_aspect_preserving_scaling,
                      has_overscan,
                      has_color_correction_matrix,
                      display_name,
                      sys_path,
                      std::move(modes),
                      edid,
                      current_mode,
                      native_mode) {
  product_id_ = product_id;
  maximum_cursor_size_ = maximum_cursor_size;
}

DisplaySnapshotMojo::~DisplaySnapshotMojo() = default;

std::unique_ptr<DisplaySnapshotMojo> DisplaySnapshotMojo::Clone() const {
  DisplayModeList clone_modes;
  for (auto& mode : modes())
    clone_modes.push_back(mode->Clone());

  return base::MakeUnique<DisplaySnapshotMojo>(
      display_id(), origin(), physical_size(), type(),
      is_aspect_preserving_scaling(), has_overscan(),
      has_color_correction_matrix(), display_name(), sys_path(), product_id(),
      std::move(clone_modes), edid(), current_mode(), native_mode(),
      maximum_cursor_size());
}

// TODO(thanhph): Implement ToString() for debugging purposes.
std::string DisplaySnapshotMojo::ToString() const {
  return "";
}

}  // namespace display
