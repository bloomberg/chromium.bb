// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_snapshot_mojo.h"

#include "base/memory/ptr_util.h"
#include "ui/display/types/display_constants.h"

namespace display {

// static
std::unique_ptr<DisplaySnapshotMojo> DisplaySnapshotMojo::CreateFrom(
    const DisplaySnapshot& other) {
  DisplayModeList clone_modes;
  const DisplayMode* current_mode = nullptr;
  const DisplayMode* native_mode = nullptr;

  // Clone the display modes and find equivalent pointers to the native and
  // current mode.
  for (auto& mode : other.modes()) {
    clone_modes.push_back(mode->Clone());
    if (mode.get() == other.current_mode())
      current_mode = mode.get();
    if (mode.get() == other.native_mode())
      native_mode = mode.get();
  }

  return base::MakeUnique<DisplaySnapshotMojo>(
      other.display_id(), other.origin(), other.physical_size(), other.type(),
      other.is_aspect_preserving_scaling(), other.has_overscan(),
      other.has_color_correction_matrix(), other.display_name(),
      other.sys_path(), other.product_id(), std::move(clone_modes),
      other.edid(), current_mode, native_mode, other.maximum_cursor_size());
}

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

DisplaySnapshotMojo::DisplaySnapshotMojo(int64_t display_id,
                                         const gfx::Point& origin,
                                         const gfx::Size& physical_size,
                                         DisplayConnectionType type,
                                         bool is_aspect_preserving_scaling,
                                         bool has_overscan,
                                         bool has_color_correction_matrix,
                                         std::string display_name,
                                         const base::FilePath& sys_path,
                                         DisplayModeList modes,
                                         const std::vector<uint8_t>& edid,
                                         const DisplayMode* current_mode,
                                         const DisplayMode* native_mode,
                                         std::string string_representation)
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
                      native_mode),
      string_representation_(string_representation) {}

DisplaySnapshotMojo::~DisplaySnapshotMojo() = default;

std::string DisplaySnapshotMojo::ToString() const {
  return string_representation_;
}

}  // namespace display
