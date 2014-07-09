// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_GPU_OZONE_GPU_MESSAGE_PARAMS_H_
#define UI_OZONE_COMMON_GPU_OZONE_GPU_MESSAGE_PARAMS_H_

#include <string>
#include <vector>

#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

struct DisplayMode_Params {
  DisplayMode_Params();
  ~DisplayMode_Params();

  gfx::Size size;
  bool is_interlaced;
  float refresh_rate;
};

struct DisplaySnapshot_Params {
  DisplaySnapshot_Params();
  ~DisplaySnapshot_Params();

  int64_t display_id;
  bool has_proper_display_id;
  gfx::Point origin;
  gfx::Size physical_size;
  DisplayConnectionType type;
  bool is_aspect_preserving_scaling;
  bool has_overscan;
  std::string display_name;
  std::vector<DisplayMode_Params> modes;
  bool has_current_mode;
  DisplayMode_Params current_mode;
  bool has_native_mode;
  DisplayMode_Params native_mode;
  std::string string_representation;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_GPU_OZONE_GPU_MESSAGE_PARAMS_H_

