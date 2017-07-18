// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_INTERFACES_OVERLAY_SURFACE_CANDIDATE_STRUCT_TRAITS_H_
#define UI_OZONE_PUBLIC_INTERFACES_OVERLAY_SURFACE_CANDIDATE_STRUCT_TRAITS_H_

#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "ui/gfx/mojo/buffer_types_struct_traits.h"
#include "ui/gfx/mojo/overlay_transform_struct_traits.h"
#include "ui/ozone/public/interfaces/overlay_surface_candidate.mojom.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace mojo {

template <>
struct StructTraits<ui::ozone::mojom::OverlaySurfaceCandidateDataView,
                    ui::OverlaySurfaceCandidate> {
  static const gfx::OverlayTransform& transform(
      const ui::OverlaySurfaceCandidate& osc) {
    return osc.transform;
  }

  static const gfx::BufferFormat& format(
      const ui::OverlaySurfaceCandidate& osc) {
    return osc.format;
  }

  static const gfx::Size& buffer_size(const ui::OverlaySurfaceCandidate& osc) {
    return osc.buffer_size;
  }

  static const gfx::RectF& display_rect(
      const ui::OverlaySurfaceCandidate& osc) {
    return osc.display_rect;
  }

  static const gfx::RectF& crop_rect(const ui::OverlaySurfaceCandidate& osc) {
    return osc.crop_rect;
  }

  static const gfx::Rect& quad_rect_in_target_space(
      const ui::OverlaySurfaceCandidate& osc) {
    return osc.quad_rect_in_target_space;
  }

  static const gfx::Rect& clip_rect(const ui::OverlaySurfaceCandidate& osc) {
    return osc.clip_rect;
  }

  static bool is_clipped(const ui::OverlaySurfaceCandidate& osc) {
    return osc.is_clipped;
  }

  static int plane_z_order(const ui::OverlaySurfaceCandidate& osc) {
    return osc.plane_z_order;
  }

  static bool overlay_handled(const ui::OverlaySurfaceCandidate& osc) {
    return osc.overlay_handled;
  }

  static bool Read(ui::ozone::mojom::OverlaySurfaceCandidateDataView data,
                   ui::OverlaySurfaceCandidate* out) {
    out->is_clipped = data.is_clipped();
    out->plane_z_order = data.plane_z_order();
    out->overlay_handled = data.overlay_handled();
    return data.ReadTransform(&out->transform) &&
           data.ReadFormat(&out->format) &&
           data.ReadBufferSize(&out->buffer_size) &&
           data.ReadDisplayRect(&out->display_rect) &&
           data.ReadCropRect(&out->crop_rect) &&
           data.ReadQuadRectInTargetSpace(&out->quad_rect_in_target_space) &&
           data.ReadClipRect(&out->clip_rect);
  }
};

}  // namespace mojo

#endif  // UI_OZONE_PUBLIC_INTERFACES_OVERLAY_SURFACE_CANDIDATE_STRUCT_TRAITS_H_
