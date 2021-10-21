// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_SHARED_QUAD_STATE_H_
#define COMPONENTS_VIZ_COMMON_QUADS_SHARED_QUAD_STATE_H_

#include "components/viz/common/viz_common_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace viz {


// SharedQuadState holds a set of properties that are common across multiple
// DrawQuads. It's purely an optimization - the properties behave in exactly the
// same way as if they were replicated on each DrawQuad. A given SharedQuadState
// can only be shared by DrawQuads that are adjacent in their RenderPass'
// QuadList.
class VIZ_COMMON_EXPORT SharedQuadState {
 public:
  SharedQuadState();
  SharedQuadState(const SharedQuadState& other);
  ~SharedQuadState();

  void SetAll(const gfx::Transform& transform,
              const gfx::Rect& layer_rect,
              const gfx::Rect& visible_layer_rect,
              const gfx::MaskFilterInfo& filter_info,
              const absl::optional<gfx::Rect>& clip,
              bool contents_opaque,
              float opacity_f,
              SkBlendMode blend,
              int sorting_context);
  void AsValueInto(base::trace_event::TracedValue* dict) const;

  // Transforms quad rects into the target content space.
  gfx::Transform quad_to_target_transform;
  // The rect of the quads' originating layer in the space of the quad rects.
  // Note that the |quad_layer_rect| represents the union of the |rect| of
  // DrawQuads in this SharedQuadState. If it does not hold, then
  // |are_contents_opaque| needs to be set to false.
  gfx::Rect quad_layer_rect;
  // The size of the visible area in the quads' originating layer, in the space
  // of the quad rects.
  gfx::Rect visible_quad_layer_rect;
  // This mask filter's coordinates is in the target content space. It defines
  // the corner radius to clip the quads with.
  gfx::MaskFilterInfo mask_filter_info;
  // This rect lives in the target content space.
  absl::optional<gfx::Rect> clip_rect;
  // Indicates whether the content in |quad_layer_rect| are fully opaque.
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  int sorting_context_id = 0;
  // Used by SurfaceAggregator to decide whether to merge quads for a surface
  // into their target render pass. It is a performance optimization by avoiding
  // render passes as much as possible.
  bool is_fast_rounded_corner = false;
  // This is for underlay optimization and used only in the SurfaceAggregator
  // and the OverlayProcessor. Do not set the value in CompositorRenderPass.
  // This index points to the damage rect in the surface damage rect list where
  // the overlay quad belongs to. SetAll() doesn't update this data.
  absl::optional<size_t> overlay_damage_index;
  // The amount to skew quads in this layer. For experimental de-jelly effect.
  float de_jelly_delta_y = 0.0f;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_SHARED_QUAD_STATE_H_
