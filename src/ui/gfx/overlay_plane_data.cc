// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/overlay_plane_data.h"

namespace gfx {

OverlayPlaneData::OverlayPlaneData() = default;

OverlayPlaneData::OverlayPlaneData(
    int z_order,
    OverlayTransform plane_transform,
    const Rect& display_bounds,
    const RectF& crop_rect,
    bool enable_blend,
    const Rect& damage_rect,
    float opacity,
    OverlayPriorityHint priority_hint,
    const gfx::RRectF& rounded_corners,
    const gfx::ColorSpace& color_space,
    const absl::optional<HDRMetadata>& hdr_metadata,
    absl::optional<SkColor> solid_color)
    : z_order(z_order),
      plane_transform(plane_transform),
      display_bounds(display_bounds),
      crop_rect(crop_rect),
      enable_blend(enable_blend),
      damage_rect(damage_rect),
      opacity(opacity),
      priority_hint(priority_hint),
      rounded_corners(rounded_corners),
      color_space(color_space),
      hdr_metadata(hdr_metadata),
      solid_color(solid_color) {}

OverlayPlaneData::~OverlayPlaneData() = default;

OverlayPlaneData::OverlayPlaneData(const OverlayPlaneData& other) = default;

}  // namespace gfx
