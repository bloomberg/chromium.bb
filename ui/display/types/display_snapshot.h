// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_SNAPSHOT_H_
#define UI_DISPLAY_TYPES_DISPLAY_SNAPSHOT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_mode.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace display {

// This class represents the state of a display at one point in time. Platforms
// may extend this class in order to add platform specific configuration and
// identifiers required to configure this display.
class DISPLAY_TYPES_EXPORT DisplaySnapshot {
 public:
  using DisplayModeList = std::vector<std::unique_ptr<const DisplayMode>>;

  DisplaySnapshot(int64_t display_id,
                  const gfx::Point& origin,
                  const gfx::Size& physical_size,
                  DisplayConnectionType type,
                  bool is_aspect_preserving_scaling,
                  bool has_overscan,
                  bool has_color_correction_matrix,
                  const gfx::ColorSpace& color_space,
                  std::string display_name,
                  const base::FilePath& sys_path,
                  DisplayModeList modes,
                  const std::vector<uint8_t>& edid,
                  const DisplayMode* current_mode,
                  const DisplayMode* native_mode,
                  int64_t product_id,
                  const gfx::Size& maximum_cursor_size);
  virtual ~DisplaySnapshot();

  int64_t display_id() const { return display_id_; }
  const gfx::Point& origin() const { return origin_; }
  void set_origin(const gfx::Point& origin) { origin_ = origin; }
  const gfx::Size& physical_size() const { return physical_size_; }
  DisplayConnectionType type() const { return type_; }
  bool is_aspect_preserving_scaling() const {
    return is_aspect_preserving_scaling_;
  }
  bool has_overscan() const { return has_overscan_; }
  bool has_color_correction_matrix() const {
    return has_color_correction_matrix_;
  }
  const gfx::ColorSpace& color_space() const { return color_space_; }
  const std::string& display_name() const { return display_name_; }
  const base::FilePath& sys_path() const { return sys_path_; }
  const DisplayModeList& modes() const { return modes_; }
  const std::vector<uint8_t>& edid() const { return edid_; }
  const DisplayMode* current_mode() const { return current_mode_; }
  void set_current_mode(const DisplayMode* mode) { current_mode_ = mode; }
  const DisplayMode* native_mode() const { return native_mode_; }
  int64_t product_id() const { return product_id_; }
  const gfx::Size& maximum_cursor_size() const { return maximum_cursor_size_; }

  void add_mode(const DisplayMode* mode) { modes_.push_back(mode->Clone()); }

  // Clones display state.
  std::unique_ptr<DisplaySnapshot> Clone();

  // Returns a textual representation of this display state.
  std::string ToString() const;

  // Used when no product id known.
  static const int64_t kInvalidProductID = -1;

  // Returns the buffer format to be used for the primary plane buffer.
  static gfx::BufferFormat PrimaryFormat();

 private:
  // Display id for this output.
  const int64_t display_id_;

  // Display's origin on the framebuffer.
  gfx::Point origin_;

  const gfx::Size physical_size_;

  const DisplayConnectionType type_;

  const bool is_aspect_preserving_scaling_;

  const bool has_overscan_;

  // Whether this display has advanced color correction available.
  const bool has_color_correction_matrix_;

  const gfx::ColorSpace color_space_;

  const std::string display_name_;

  const base::FilePath sys_path_;

  DisplayModeList modes_;

  // The display's EDID. It can be empty if nothing extracted such as in the
  // case of a virtual display.
  std::vector<uint8_t> edid_;

  // Mode currently being used by the output.
  const DisplayMode* current_mode_;

  // "Best" mode supported by the output.
  const DisplayMode* const native_mode_;

  // Combination of manufacturer and product code.
  const int64_t product_id_;

  // Maximum supported cursor size on this display.
  const gfx::Size maximum_cursor_size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplaySnapshot);
};

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_SNAPSHOT_H_
