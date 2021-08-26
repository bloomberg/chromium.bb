// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_CA_LAYER_OVERLAY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_CA_LAYER_OVERLAY_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "skia/ext/skia_matrix_44.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/rrect_f.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/ca_renderer_layer_params.h"

class SkDeferredDisplayList;

namespace viz {
class AggregatedRenderPassDrawQuad;
class DisplayResourceProvider;
class DrawQuad;

// Holds information that is frequently shared between consecutive
// CALayerOverlays.
class VIZ_SERVICE_EXPORT CALayerOverlaySharedState
    : public base::RefCountedThreadSafe<CALayerOverlaySharedState> {
 public:
  CALayerOverlaySharedState() {}
  // Layers in a non-zero sorting context exist in the same 3D space and should
  // intersect.
  unsigned sorting_context_id = 0;
  // If |is_clipped| is true, then clip to |clip_rect| in the target space, and
  // |clip_rect_corner_radius| represents the corner radius of the clip rect.
  bool is_clipped = false;
  gfx::RectF clip_rect;
  gfx::RRectF rounded_corner_bounds;
  // The opacity property for the CAayer.
  float opacity = 1;
  // The transform to apply to the CALayer.
  skia::Matrix44 transform =
      skia::Matrix44(skia::Matrix44::kIdentity_Constructor);

 private:
  friend class base::RefCountedThreadSafe<CALayerOverlaySharedState>;
  ~CALayerOverlaySharedState() {}
};

// Holds all information necessary to construct a CALayer from a DrawQuad.
class VIZ_SERVICE_EXPORT CALayerOverlay {
 public:
  CALayerOverlay();
  CALayerOverlay(const CALayerOverlay& other);
  ~CALayerOverlay();
  CALayerOverlay& operator=(const CALayerOverlay& other);

  // State that is frequently shared between consecutive CALayerOverlays.
  scoped_refptr<CALayerOverlaySharedState> shared_state;

  // Texture that corresponds to an IOSurface to set as the content of the
  // CALayer. If this is 0 then the CALayer is a solid color.
  ResourceId contents_resource_id = kInvalidResourceId;
  // Mailbox from contents_resource_id. It is used by SkiaRenderer.
  gpu::Mailbox mailbox;
  // The contents rect property for the CALayer.
  gfx::RectF contents_rect;
  // The bounds for the CALayer in pixels.
  gfx::RectF bounds_rect;
  // The background color property for the CALayer.
  SkColor background_color = SK_ColorTRANSPARENT;
  // The edge anti-aliasing mask property for the CALayer.
  unsigned edge_aa_mask = 0;
  // The minification and magnification filters for the CALayer.
  unsigned filter = 0;
  // The protected video status of the AVSampleBufferDisplayLayer.
  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;
  // If |rpdq| is present, then the renderer must draw the filter effects and
  // copy the result into an IOSurface.
  const AggregatedRenderPassDrawQuad* rpdq = nullptr;
  // The DDL for generating render pass overlay buffer with SkiaRenderer.
  sk_sp<SkDeferredDisplayList> ddl;
};

typedef std::vector<CALayerOverlay> CALayerOverlayList;

// TODO(weiliangc): Eventually fold this class into OverlayProcessorMac and fold
// CALayerOverlay into OverlayCandidate.
class VIZ_SERVICE_EXPORT CALayerOverlayProcessor {
 public:
  CALayerOverlayProcessor() = default;
  virtual ~CALayerOverlayProcessor() = default;

  bool AreClipSettingsValid(const CALayerOverlay& ca_layer_overlay,
                            CALayerOverlayList* ca_layer_overlay_list) const;
  void PutForcedOverlayContentIntoUnderlays(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPass* render_pass,
      const gfx::RectF& display_rect,
      QuadList* quad_list,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_filters,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_backdrop_filters,
      CALayerOverlayList* ca_layer_overlays) const;

  // Returns true if all quads in the root render pass have been replaced by
  // CALayerOverlays. Virtual for testing.
  virtual bool ProcessForCALayerOverlays(
      DisplayResourceProvider* resource_provider,
      const gfx::RectF& display_rect,
      const QuadList& quad_list,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_filters,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_backdrop_filters,
      CALayerOverlayList* ca_layer_overlays) const;

 private:
  // Returns whether future candidate quads should be considered
  bool PutQuadInSeparateOverlay(
      QuadList::Iterator at,
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPass* render_pass,
      const gfx::RectF& display_rect,
      const DrawQuad* quad,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_filters,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_backdrop_filters,
      gfx::ProtectedVideoType protected_video_type,
      CALayerOverlayList* ca_layer_overlays) const;

  DISALLOW_COPY_AND_ASSIGN(CALayerOverlayProcessor);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_CA_LAYER_OVERLAY_H_
