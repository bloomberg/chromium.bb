// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DC_LAYER_OVERLAY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DC_LAYER_OVERLAY_H_

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gl/dc_renderer_layer_params.h"

namespace viz {
class DisplayResourceProvider;
class ContextProvider;

// Holds all information necessary to construct a DCLayer from a DrawQuad.
class VIZ_SERVICE_EXPORT DCLayerOverlay {
 public:
  DCLayerOverlay();
  DCLayerOverlay(const DCLayerOverlay& other);
  DCLayerOverlay& operator=(const DCLayerOverlay& other);
  ~DCLayerOverlay();

  // TODO(magchen): Once software protected video is enabled for all GPUs and
  // all configurations, RequiresOverlay() will be true for all protected video.
  // Currently, we only force the overlay swap chain path (RequiresOverlay) for
  // hardware protected video and soon for Finch experiment on software
  // protected video.
  bool RequiresOverlay() const {
    return (protected_video_type == ui::ProtectedVideoType::kHardwareProtected);
  }

  // Resource ids for video Y and UV planes.  Can be the same resource.
  // See DirectCompositionSurfaceWin for details.
  ResourceId y_resource_id = 0;
  ResourceId uv_resource_id = 0;

  // Stacking order relative to backbuffer which has z-order 0.
  int z_order = 1;

  // What part of the content to display in pixels.
  gfx::Rect content_rect;

  // Bounds of the overlay in pre-transform space.
  gfx::Rect quad_rect;

  // 2D flattened transform that maps |quad_rect| to root target space,
  // after applying the |quad_rect.origin()| as an offset.
  gfx::Transform transform;

  // If |is_clipped| is true, then clip to |clip_rect| in root target space.
  bool is_clipped = false;
  gfx::Rect clip_rect;

  // This is the color-space the texture should be displayed as. If invalid,
  // then the default for the texture should be used. For YUV textures, that's
  // normally BT.709.
  gfx::ColorSpace color_space;

  ui::ProtectedVideoType protected_video_type = ui::ProtectedVideoType::kClear;
};

typedef std::vector<DCLayerOverlay> DCLayerOverlayList;

class DCLayerOverlayProcessor {
 public:
  explicit DCLayerOverlayProcessor(const ContextProvider* context_provider);
  ~DCLayerOverlayProcessor();

  void Process(DisplayResourceProvider* resource_provider,
               const gfx::RectF& display_rect,
               RenderPassList* render_passes,
               gfx::Rect* damage_rect,
               DCLayerOverlayList* dc_layer_overlays);
  void ClearOverlayState();
  void SetHasHwOverlaySupport() { has_hw_overlay_support_ = true; }
  // This is the damage contribution due to previous frame's overlays which can
  // be empty.
  gfx::Rect previous_frame_overlay_damage_contribution() {
    return previous_frame_overlay_rect_union_;
  }

 private:
  // Returns an iterator to the element after |it|.
  QuadList::Iterator ProcessRenderPassDrawQuad(RenderPass* render_pass,
                                               gfx::Rect* damage_rect,
                                               QuadList::Iterator it);

  void ProcessRenderPass(DisplayResourceProvider* resource_provider,
                         const gfx::RectF& display_rect,
                         RenderPass* render_pass,
                         bool is_root,
                         gfx::Rect* damage_rect,
                         DCLayerOverlayList* dc_layer_overlays);
  void ProcessForOverlay(const gfx::RectF& display_rect,
                         QuadList* quad_list,
                         const gfx::Rect& quad_rectangle,
                         QuadList::Iterator* it,
                         gfx::Rect* damage_rect);
  void ProcessForUnderlay(const gfx::RectF& display_rect,
                          RenderPass* render_pass,
                          const gfx::Rect& quad_rectangle,
                          const QuadList::Iterator& it,
                          bool is_root,
                          gfx::Rect* damage_rect,
                          gfx::Rect* this_frame_underlay_rect,
                          DCLayerOverlay* dc_layer);

  gfx::Rect previous_frame_underlay_rect_;
  gfx::RectF previous_display_rect_;
  // previous and current overlay_rect_union_ include both overlay and underlay
  gfx::Rect previous_frame_overlay_rect_union_;
  gfx::Rect current_frame_overlay_rect_union_;
  int previous_frame_processed_overlay_count_ = 0;
  int current_frame_processed_overlay_count_ = 0;
  bool has_hw_overlay_support_ = true;

  // Store information about clipped punch-through rects in target space for
  // non-root render passes. These rects are used to clear the corresponding
  // areas in parent render passes.
  base::flat_map<RenderPassId, std::vector<gfx::Rect>>
      pass_punch_through_rects_;

  DISALLOW_COPY_AND_ASSIGN(DCLayerOverlayProcessor);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DC_LAYER_OVERLAY_H_
