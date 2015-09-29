// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

namespace ui {

DisplayMode_Params::DisplayMode_Params() {
}

DisplayMode_Params::~DisplayMode_Params() {}

DisplaySnapshot_Params::DisplaySnapshot_Params() {
}

DisplaySnapshot_Params::~DisplaySnapshot_Params() {}

OverlayCheck_Params::OverlayCheck_Params() {}

OverlayCheck_Params::OverlayCheck_Params(
    const OverlayCandidatesOzone::OverlaySurfaceCandidate& candidate)
    : buffer_size(candidate.buffer_size),
      transform(candidate.transform),
      format(candidate.format),
      display_rect(gfx::ToNearestRect(candidate.display_rect)),
      crop_rect(candidate.crop_rect),
      plane_z_order(candidate.plane_z_order),
      weight(0) {}

OverlayCheck_Params::~OverlayCheck_Params() {
}

}  // namespace ui
