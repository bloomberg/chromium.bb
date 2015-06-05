// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

namespace ui {

DisplayMode_Params::DisplayMode_Params()
    : size(), is_interlaced(false), refresh_rate(0.0f) {}

DisplayMode_Params::~DisplayMode_Params() {}

DisplaySnapshot_Params::DisplaySnapshot_Params()
    : display_id(0),
      origin(),
      physical_size(),
      type(ui::DISPLAY_CONNECTION_TYPE_NONE),
      is_aspect_preserving_scaling(false),
      has_overscan(false),
      display_name(),
      modes(),
      has_current_mode(false),
      current_mode(),
      has_native_mode(false),
      native_mode(),
      product_id(0) {
}

DisplaySnapshot_Params::~DisplaySnapshot_Params() {}

OverlayCheck_Params::OverlayCheck_Params()
    : transform(gfx::OVERLAY_TRANSFORM_INVALID),
      format(SurfaceFactoryOzone::UNKNOWN),
      plane_z_order(0) {
}

OverlayCheck_Params::OverlayCheck_Params(
    const OverlayCandidatesOzone::OverlaySurfaceCandidate& candidate)
    : buffer_size(candidate.buffer_size),
      transform(candidate.transform),
      format(candidate.format),
      display_rect(gfx::ToNearestRect(candidate.display_rect)),
      plane_z_order(candidate.plane_z_order) {
}

OverlayCheck_Params::~OverlayCheck_Params() {
}

}  // namespace ui
