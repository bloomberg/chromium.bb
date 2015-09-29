// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_GPU_OZONE_GPU_MESSAGE_PARAMS_H_
#define UI_OZONE_COMMON_GPU_OZONE_GPU_MESSAGE_PARAMS_H_

#include <string>
#include <vector>

#include "ui/display/types/display_constants.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {

struct OZONE_EXPORT DisplayMode_Params {
  DisplayMode_Params();
  ~DisplayMode_Params();

  gfx::Size size;
  bool is_interlaced = false;
  float refresh_rate = 0.0f;
};

struct OZONE_EXPORT DisplaySnapshot_Params {
  DisplaySnapshot_Params();
  ~DisplaySnapshot_Params();

  int64_t display_id = 0;
  gfx::Point origin;
  gfx::Size physical_size;
  DisplayConnectionType type = DISPLAY_CONNECTION_TYPE_NONE;
  bool is_aspect_preserving_scaling = false;
  bool has_overscan = false;
  std::string display_name;
  std::vector<DisplayMode_Params> modes;
  bool has_current_mode = false;
  DisplayMode_Params current_mode;
  bool has_native_mode = false;
  DisplayMode_Params native_mode;
  int64_t product_id = 0;
  std::string string_representation;
};

struct OZONE_EXPORT OverlayCheck_Params {
  OverlayCheck_Params();
  OverlayCheck_Params(
      const OverlayCandidatesOzone::OverlaySurfaceCandidate& candidate);
  ~OverlayCheck_Params();

  gfx::Size buffer_size;
  gfx::OverlayTransform transform = gfx::OVERLAY_TRANSFORM_INVALID;
  gfx::BufferFormat format = gfx::BufferFormat::BGRA_8888;
  gfx::Rect display_rect;
  gfx::RectF crop_rect;
  int plane_z_order = 0;
  // Higher the value, the more important it is to ensure that this
  // overlay candidate finds a compatible free hardware plane to use.
  uint32_t weight;
  // Will be set in GPU process. These are unique plane ids of primary display
  // supporting this configuration.
  std::vector<uint32_t> plane_ids;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_GPU_OZONE_GPU_MESSAGE_PARAMS_H_

