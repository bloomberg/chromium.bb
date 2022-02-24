// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/ca_layer_overlay.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/skia/include/core/SkDeferredDisplayList.h"
#include "ui/base/cocoa/remote_layer_api.h"

namespace viz {

namespace {

// The CoreAnimation renderer's performance starts suffering when too many
// quads are promoted to CALayers. At extremes, corruption can occur.
// https://crbug.com/1022116
// This number can be re-assigned by kMacCAOverlayQuadMaxNum when
// feature kMacCAOverlayQuad is enabled.
constexpr size_t kTooManyQuads = 128;

// If there are too many RenderPassDrawQuads, we shouldn't use Core
// Animation to present them as individual layers, since that potentially
// doubles the amount of work needed to present them. cc has to blit them into
// an IOSurface, and then Core Animation has to blit them to the final surface.
// https://crbug.com/636884.
const int kTooManyRenderPassDrawQuads = 30;

// Assume we are in a video conference if the total video count is bigger than
// or equal to this number.
const int kMaxNumVideos = 5;

void RecordCALayerHistogram(gfx::CALayerResult result) {
  UMA_HISTOGRAM_ENUMERATION("Compositing.Renderer.CALayerResult", result);
}

bool FilterOperationSupported(const cc::FilterOperation& operation) {
  switch (operation.type()) {
    case cc::FilterOperation::GRAYSCALE:
    case cc::FilterOperation::SEPIA:
    case cc::FilterOperation::SATURATE:
    case cc::FilterOperation::HUE_ROTATE:
    case cc::FilterOperation::INVERT:
    case cc::FilterOperation::BRIGHTNESS:
    case cc::FilterOperation::CONTRAST:
    case cc::FilterOperation::OPACITY:
    case cc::FilterOperation::BLUR:
    case cc::FilterOperation::DROP_SHADOW:
      return true;
    default:
      return false;
  }
}

gfx::CALayerResult FromRenderPassQuad(
    DisplayResourceProvider* resource_provider,
    const AggregatedRenderPassDrawQuad* quad,
    const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
        render_pass_filters,
    const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
        render_pass_backdrop_filters,
    CALayerOverlay* ca_layer_overlay) {
  if (render_pass_backdrop_filters.count(quad->render_pass_id)) {
    return gfx::kCALayerFailedRenderPassBackdropFilters;
  }

  auto* shared_quad_state = quad->shared_quad_state;
  if (shared_quad_state->sorting_context_id != 0)
    return gfx::kCALayerFailedRenderPassSortingContextId;

  auto it = render_pass_filters.find(quad->render_pass_id);
  if (it != render_pass_filters.end()) {
    for (const auto& operation : it->second->operations()) {
      bool success = FilterOperationSupported(operation);
      if (!success)
        return gfx::kCALayerFailedRenderPassFilterOperation;
    }
  }

  // TODO(crbug.com/1215491): support not 2d axis aligned clipping.
  if (shared_quad_state->clip_rect &&
      !shared_quad_state->quad_to_target_transform.Preserves2dAxisAlignment()) {
    return gfx::kCALayerFailedQuadClipping;
  }

  // TODO(crbug.com/1215491): support not 2d axis aligned mask.
  if (!shared_quad_state->mask_filter_info.IsEmpty() &&
      !shared_quad_state->quad_to_target_transform.Preserves2dAxisAlignment()) {
    return gfx::kCALayerFailedRenderPassPassMask;
  }

  ca_layer_overlay->rpdq = quad;
  ca_layer_overlay->contents_rect = gfx::RectF(0, 0, 1, 1);

  return gfx::kCALayerSuccess;
}

gfx::CALayerResult FromStreamVideoQuad(
    DisplayResourceProvider* resource_provider,
    const StreamVideoDrawQuad* quad,
    CALayerOverlay* ca_layer_overlay) {
  ResourceId resource_id = quad->resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return gfx::kCALayerFailedStreamVideoNotCandidate;
  ca_layer_overlay->contents_resource_id = resource_id;
  ca_layer_overlay->contents_rect =
      BoundingRect(quad->uv_top_left, quad->uv_bottom_right);
  return gfx::kCALayerSuccess;
}

gfx::CALayerResult FromSolidColorDrawQuad(const SolidColorDrawQuad* quad,
                                          CALayerOverlay* ca_layer_overlay,
                                          bool* skip) {
  // Do not generate quads that are completely transparent.
  if (SkColorGetA(quad->color) == 0) {
    *skip = true;
    return gfx::kCALayerSuccess;
  }
  ca_layer_overlay->background_color = quad->color;
  return gfx::kCALayerSuccess;
}

gfx::CALayerResult FromTextureQuad(DisplayResourceProvider* resource_provider,
                                   const TextureDrawQuad* quad,
                                   CALayerOverlay* ca_layer_overlay) {
  ResourceId resource_id = quad->resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return gfx::kCALayerFailedTextureNotCandidate;
  if (quad->y_flipped) {
    // The anchor point is at the bottom-left corner of the CALayer. The
    // transformation that flips the contents of the layer without changing its
    // frame is the composition of a vertical flip about the anchor point, and a
    // translation by the height of the layer.
    ca_layer_overlay->shared_state->transform.preTranslate(
        0, ca_layer_overlay->bounds_rect.height(), 0);
    ca_layer_overlay->shared_state->transform.preScale(1, -1, 1);
  }
  ca_layer_overlay->contents_resource_id = resource_id;
  ca_layer_overlay->contents_rect =
      BoundingRect(quad->uv_top_left, quad->uv_bottom_right);
  ca_layer_overlay->background_color = quad->background_color;
  for (int i = 1; i < 4; ++i) {
    if (quad->vertex_opacity[i] != quad->vertex_opacity[0])
      return gfx::kCALayerFailedDifferentVertexOpacities;
  }
  ca_layer_overlay->shared_state->opacity *= quad->vertex_opacity[0];
  ca_layer_overlay->filter = quad->nearest_neighbor ? GL_NEAREST : GL_LINEAR;
  if (quad->is_video_frame)
    ca_layer_overlay->protected_video_type = quad->protected_video_type;
  return gfx::kCALayerSuccess;
}

gfx::CALayerResult FromYUVVideoQuad(DisplayResourceProvider* resource_provider,
                                    const YUVVideoDrawQuad* quad,
                                    CALayerOverlay* ca_layer_overlay) {
  // For YUVVideoDrawQuads, the Y and UV planes alias the same underlying
  // IOSurface. Ensure all planes are overlays and have the same contents
  // rect. Then use the Y plane as the resource for the overlay.
  ResourceId y_resource_id = quad->y_plane_resource_id();
  if (!resource_provider->IsOverlayCandidate(y_resource_id) ||
      !resource_provider->IsOverlayCandidate(quad->u_plane_resource_id()) ||
      !resource_provider->IsOverlayCandidate(quad->v_plane_resource_id())) {
    return gfx::kCALayerFailedYUVNotCandidate;
  }
  if (quad->y_plane_resource_id() == quad->u_plane_resource_id() ||
      quad->y_plane_resource_id() == quad->v_plane_resource_id() ||
      quad->u_plane_resource_id() != quad->v_plane_resource_id()) {
    return gfx::kCALayerFailedYUVInvalidPlanes;
  }

  gfx::RectF ya_contents_rect =
      gfx::ScaleRect(quad->ya_tex_coord_rect, 1.f / quad->ya_tex_size.width(),
                     1.f / quad->ya_tex_size.height());
  gfx::RectF uv_contents_rect =
      gfx::ScaleRect(quad->uv_tex_coord_rect, 1.f / quad->uv_tex_size.width(),
                     1.f / quad->uv_tex_size.height());
  if (ya_contents_rect != uv_contents_rect)
    return gfx::kCALayerFailedYUVTexcoordMismatch;
  ca_layer_overlay->contents_resource_id = y_resource_id;
  ca_layer_overlay->contents_rect = ya_contents_rect;
  ca_layer_overlay->protected_video_type = quad->protected_video_type;
  return gfx::kCALayerSuccess;
}

gfx::CALayerResult FromTileQuad(DisplayResourceProvider* resource_provider,
                                const TileDrawQuad* quad,
                                CALayerOverlay* ca_layer_overlay) {
  ResourceId resource_id = quad->resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return gfx::kCALayerFailedTileNotCandidate;
  ca_layer_overlay->contents_resource_id = resource_id;
  ca_layer_overlay->contents_rect = quad->tex_coord_rect;
  ca_layer_overlay->contents_rect.Scale(1.f / quad->texture_size.width(),
                                        1.f / quad->texture_size.height());
  ca_layer_overlay->filter = quad->nearest_neighbor ? GL_NEAREST : GL_LINEAR;
  return gfx::kCALayerSuccess;
}

class CALayerOverlayProcessorInternal {
 public:
  gfx::CALayerResult FromDrawQuad(
      DisplayResourceProvider* resource_provider,
      const gfx::RectF& display_rect,
      const DrawQuad* quad,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_filters,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_backdrop_filters,
      CALayerOverlay* ca_layer_overlay,
      bool* skip,
      bool* render_pass_draw_quad,
      int& yuv_draw_quad_count) {
    if (quad->shared_quad_state->blend_mode != SkBlendMode::kSrcOver)
      return gfx::kCALayerFailedQuadBlendMode;

    // Early-out for invisible quads.
    if (quad->shared_quad_state->opacity == 0.f ||
        quad->visible_rect.IsEmpty()) {
      *skip = true;
      return gfx::kCALayerSuccess;
    }

    // Support rounded corner bounds when they have the same rect as the clip
    // rect, and all corners have the same radius. Note that it is entirely
    // possible to make rounded corner rects independent of clip rect (by adding
    // another CALayer to the tree). Handling non-single border radii is also,
    // but requires APIs not supported on all macOS versions.
    if (quad->shared_quad_state->mask_filter_info.HasRoundedCorners()) {
      DCHECK(quad->shared_quad_state->clip_rect);
      if (quad->shared_quad_state->mask_filter_info.rounded_corner_bounds()
              .GetType() > gfx::RRectF::Type::kSingle) {
        return gfx::kCALayerFailedQuadRoundedCornerNotUniform;
      }
    }

    // Enable edge anti-aliasing only on layer boundaries.
    ca_layer_overlay->edge_aa_mask = 0;
    if (quad->IsLeftEdge())
      ca_layer_overlay->edge_aa_mask |= GL_CA_LAYER_EDGE_LEFT_CHROMIUM;
    if (quad->IsRightEdge())
      ca_layer_overlay->edge_aa_mask |= GL_CA_LAYER_EDGE_RIGHT_CHROMIUM;
    if (quad->IsBottomEdge())
      ca_layer_overlay->edge_aa_mask |= GL_CA_LAYER_EDGE_BOTTOM_CHROMIUM;
    if (quad->IsTopEdge())
      ca_layer_overlay->edge_aa_mask |= GL_CA_LAYER_EDGE_TOP_CHROMIUM;

    if (most_recent_shared_quad_state_ != quad->shared_quad_state) {
      most_recent_shared_quad_state_ = quad->shared_quad_state;
      most_recent_overlay_shared_state_ = new CALayerOverlaySharedState;
      // Set rect clipping and sorting context ID.
      most_recent_overlay_shared_state_->sorting_context_id =
          quad->shared_quad_state->sorting_context_id;
      most_recent_overlay_shared_state_->is_clipped =
          quad->shared_quad_state->clip_rect.has_value();
      most_recent_overlay_shared_state_->clip_rect =
          gfx::RectF(quad->shared_quad_state->clip_rect.value_or(gfx::Rect()));
      most_recent_overlay_shared_state_->rounded_corner_bounds =
          quad->shared_quad_state->mask_filter_info.rounded_corner_bounds();

      most_recent_overlay_shared_state_->opacity =
          quad->shared_quad_state->opacity;
      most_recent_overlay_shared_state_->transform =
          quad->shared_quad_state->quad_to_target_transform.matrix();
    }
    ca_layer_overlay->shared_state = most_recent_overlay_shared_state_;

    ca_layer_overlay->bounds_rect = gfx::RectF(quad->rect);

    *render_pass_draw_quad =
        quad->material == DrawQuad::Material::kAggregatedRenderPass;
    switch (quad->material) {
      case DrawQuad::Material::kTextureContent:
        return FromTextureQuad(resource_provider,
                               TextureDrawQuad::MaterialCast(quad),
                               ca_layer_overlay);
      case DrawQuad::Material::kTiledContent:
        return FromTileQuad(resource_provider, TileDrawQuad::MaterialCast(quad),
                            ca_layer_overlay);
      case DrawQuad::Material::kSolidColor:
        return FromSolidColorDrawQuad(SolidColorDrawQuad::MaterialCast(quad),
                                      ca_layer_overlay, skip);
      case DrawQuad::Material::kStreamVideoContent:
        yuv_draw_quad_count++;
        return FromStreamVideoQuad(resource_provider,
                                   StreamVideoDrawQuad::MaterialCast(quad),
                                   ca_layer_overlay);
      case DrawQuad::Material::kDebugBorder:
        return gfx::kCALayerFailedDebugBoarder;
      case DrawQuad::Material::kPictureContent:
        return gfx::kCALayerFailedPictureContent;
      case DrawQuad::Material::kAggregatedRenderPass:
        return FromRenderPassQuad(
            resource_provider, AggregatedRenderPassDrawQuad::MaterialCast(quad),
            render_pass_filters, render_pass_backdrop_filters,
            ca_layer_overlay);
      case DrawQuad::Material::kSurfaceContent:
        return gfx::kCALayerFailedSurfaceContent;
      case DrawQuad::Material::kYuvVideoContent:
        yuv_draw_quad_count++;
        return FromYUVVideoQuad(resource_provider,
                                YUVVideoDrawQuad::MaterialCast(quad),
                                ca_layer_overlay);
      default:
        break;
    }

    return gfx::kCALayerFailedUnknown;
  }

 private:
  const SharedQuadState* most_recent_shared_quad_state_ = nullptr;
  scoped_refptr<CALayerOverlaySharedState> most_recent_overlay_shared_state_;
};

// Control using the CoreAnimation renderer, which is the path that replaces
// all quads with CALayers.
base::Feature kCARenderer{"CoreAnimationRenderer",
                          base::FEATURE_ENABLED_BY_DEFAULT};

// Control using the CoreAnimation renderer, which is the path that replaces
// all quads with CALayers.
base::Feature kHDRUnderlays{"CoreAnimationHDRUnderlays",
                            base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

CALayerOverlay::CALayerOverlay() : filter(GL_LINEAR) {}

CALayerOverlay::CALayerOverlay(const CALayerOverlay& other) = default;

CALayerOverlay::~CALayerOverlay() = default;

CALayerOverlay& CALayerOverlay::operator=(const CALayerOverlay& other) =
    default;

CALayerOverlayProcessor::CALayerOverlayProcessor()
    : overlays_allowed_(ui::RemoteLayerAPISupported()),
      enable_ca_renderer_(base::FeatureList::IsEnabled(kCARenderer)),
      enable_hdr_underlays_(base::FeatureList::IsEnabled(kHDRUnderlays)) {
  max_quad_list_size_ = kTooManyQuads;
  if (base::FeatureList::IsEnabled(features::kMacCAOverlayQuad)) {
    const int max_num = features::kMacCAOverlayQuadMaxNum.Get();
    if (max_num > 0)
      max_quad_list_size_ = max_num;
  }
}

bool CALayerOverlayProcessor::AreClipSettingsValid(
    const CALayerOverlay& ca_layer_overlay,
    CALayerOverlayList* ca_layer_overlay_list) const {
  // It is not possible to correctly represent two different clipping
  // settings within one sorting context.
  if (!ca_layer_overlay_list->empty()) {
    const CALayerOverlay& previous_ca_layer = ca_layer_overlay_list->back();
    if (ca_layer_overlay.shared_state->sorting_context_id &&
        previous_ca_layer.shared_state->sorting_context_id ==
            ca_layer_overlay.shared_state->sorting_context_id) {
      if (previous_ca_layer.shared_state->is_clipped !=
              ca_layer_overlay.shared_state->is_clipped ||
          previous_ca_layer.shared_state->clip_rect !=
              ca_layer_overlay.shared_state->clip_rect) {
        return false;
      }
    }
  }

  return true;
}

void CALayerOverlayProcessor::PutForcedOverlayContentIntoUnderlays(
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPass* render_pass,
    const gfx::RectF& display_rect,
    QuadList* quad_list,
    const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
        render_pass_filters,
    const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
        render_pass_backdrop_filters,
    CALayerOverlayList* ca_layer_overlays) const {
  bool failed = false;
  CALayerOverlayProcessorInternal processor;

  for (auto it = quad_list->begin(); it != quad_list->end(); ++it) {
    const DrawQuad* quad = *it;
    bool force_quad_to_overlay = false;
    gfx::ProtectedVideoType protected_video_type =
        gfx::ProtectedVideoType::kClear;

    // Put hardware protected video into an overlay
    if (quad->material == ContentDrawQuadBase::Material::kYuvVideoContent) {
      const YUVVideoDrawQuad* video_quad = YUVVideoDrawQuad::MaterialCast(quad);
      if (video_quad->protected_video_type != gfx::ProtectedVideoType::kClear) {
        force_quad_to_overlay = true;
        protected_video_type = video_quad->protected_video_type;
      }
    }

    if (quad->material == ContentDrawQuadBase::Material::kTextureContent) {
      const TextureDrawQuad* texture_quad = TextureDrawQuad::MaterialCast(quad);

      // Put hardware protected video into an overlay
      if (texture_quad->is_video_frame && texture_quad->protected_video_type !=
                                              gfx::ProtectedVideoType::kClear) {
        force_quad_to_overlay = true;
        protected_video_type = texture_quad->protected_video_type;
      }

      // Put HDR videos into an underlay.
      if (enable_hdr_underlays_) {
        if (resource_provider->GetColorSpace(texture_quad->resource_id())
                .IsHDR())
          force_quad_to_overlay = true;
      }
    }

    if (force_quad_to_overlay) {
      if (!PutQuadInSeparateOverlay(it, resource_provider, render_pass,
                                    display_rect, quad, render_pass_filters,
                                    render_pass_backdrop_filters,
                                    protected_video_type, ca_layer_overlays)) {
        failed = true;
        break;
      }
    }
  }
  if (failed)
    ca_layer_overlays->clear();
}

bool CALayerOverlayProcessor::ProcessForCALayerOverlays(
    AggregatedRenderPass* render_pass,
    DisplayResourceProvider* resource_provider,
    const gfx::RectF& display_rect,
    const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
        render_pass_filters,
    const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
        render_pass_backdrop_filters,
    CALayerOverlayList* ca_layer_overlays) {
  const QuadList& quad_list = render_pass->quad_list;
  gfx::CALayerResult result = gfx::kCALayerSuccess;
  size_t num_visible_quads = quad_list.size();

  // Skip overlay processing
  if (!overlays_allowed_ || !enable_ca_renderer_) {
    result = gfx::kCALayerFailedOverlayDisabled;
  } else if (video_capture_enabled_) {
    result = gfx::kCALayerFailedVideoCaptureEnabled;
  } else if (!render_pass->copy_requests.empty()) {
    result = gfx::kCALayerFailedCopyRequests;
  } else if (num_visible_quads > max_quad_list_size_) {
    // |max_quad_list_size_| might be set by finch and is bigger than
    // kTooManyQuads (128).
    result = gfx::kCALayerFailedTooManyQuads;
  }

  if (result != gfx::kCALayerSuccess) {
    RecordCALayerHistogram(result);
    SaveCALayerResult(result);
    return false;
  }

  // Start overlay processing
  ca_layer_overlays->reserve(num_visible_quads);

  int render_pass_draw_quad_count = 0;
  int yuv_draw_quad_count = 0;
  CALayerOverlayProcessorInternal processor;
  for (auto it = quad_list.BackToFrontBegin();
       result == gfx::kCALayerSuccess && it != quad_list.BackToFrontEnd();
       ++it) {
    const DrawQuad* quad = *it;
    CALayerOverlay ca_layer;
    bool skip = false;
    bool render_pass_draw_quad = false;
    result = processor.FromDrawQuad(
        resource_provider, display_rect, quad, render_pass_filters,
        render_pass_backdrop_filters, &ca_layer, &skip, &render_pass_draw_quad,
        yuv_draw_quad_count);
    if (result != gfx::kCALayerSuccess)
      break;

    if (render_pass_draw_quad) {
      ++render_pass_draw_quad_count;
      if (render_pass_draw_quad_count > kTooManyRenderPassDrawQuads) {
        result = gfx::kCALayerFailedTooManyRenderPassDrawQuads;
        break;
      }
    }

    if (skip)
      continue;

    if (!AreClipSettingsValid(ca_layer, ca_layer_overlays)) {
      result = gfx::kCALayerFailedDifferentClipSettings;
      break;
    }

    ca_layer_overlays->push_back(ca_layer);
  }

  // In the case of |max_quad_list_size_| > |num_visible_quads| > kTooManyQuads,
  // Accept CALayerOverlay if in a video conference (video count >=
  // kMaxNumVideos(5)). Otherwise, do not use CALayerOverlay.
  if (num_visible_quads > kTooManyQuads &&
      yuv_draw_quad_count < kMaxNumVideos) {
    result = gfx::kCALayerFailedTooManyQuads;
  }

  RecordCALayerHistogram(result);
  SaveCALayerResult(result);

  if (result != gfx::kCALayerSuccess) {
    ca_layer_overlays->clear();
    return false;
  }
  return true;
}

bool CALayerOverlayProcessor::PutQuadInSeparateOverlay(
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
    CALayerOverlayList* ca_layer_overlays) const {
  CALayerOverlayProcessorInternal processor;
  CALayerOverlay ca_layer;
  bool skip = false;
  bool render_pass_draw_quad = false;
  int yuv_draw_quad_count = 0;
  gfx::CALayerResult result = processor.FromDrawQuad(
      resource_provider, display_rect, quad, render_pass_filters,
      render_pass_backdrop_filters, &ca_layer, &skip, &render_pass_draw_quad,
      yuv_draw_quad_count);
  if (result != gfx::kCALayerSuccess)
    return false;

  if (skip)
    return true;

  if (!AreClipSettingsValid(ca_layer, ca_layer_overlays))
    return true;

  ca_layer.protected_video_type = protected_video_type;
  render_pass->ReplaceExistingQuadWithSolidColor(at, SK_ColorTRANSPARENT,
                                                 SkBlendMode::kSrcOver);
  ca_layer_overlays->push_back(ca_layer);
  return true;
}

// Expand this function to save the results of the last N frames.
void CALayerOverlayProcessor::SaveCALayerResult(gfx::CALayerResult result) {
  ca_layer_result_ = result;
}

}  // namespace viz
