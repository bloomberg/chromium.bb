// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/containers/contains.h"
#include "base/cxx17_backports.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/video_types.h"

namespace viz {

// There is a bug in |base::optional| which causes the 'value_or' function to
// capture parameters (even constexpr parameters) as a reference.
constexpr uint32_t OverlayCandidate::kInvalidDamageIndex;
constexpr OverlayCandidate::TrackingId OverlayCandidate::kDefaultTrackingId;

namespace {

const gfx::BufferFormat kOverlayFormats[] = {
    gfx::BufferFormat::RGBX_8888, gfx::BufferFormat::RGBA_8888,
    gfx::BufferFormat::BGRX_8888, gfx::BufferFormat::BGRA_8888,
    gfx::BufferFormat::BGR_565,   gfx::BufferFormat::YUV_420_BIPLANAR,
    gfx::BufferFormat::P010};

enum Axis { NONE, AXIS_POS_X, AXIS_NEG_X, AXIS_POS_Y, AXIS_NEG_Y };

Axis VectorToAxis(const gfx::Vector3dF& vec) {
  if (!cc::MathUtil::IsWithinEpsilon(vec.z(), 0.f))
    return NONE;
  const bool x_zero = cc::MathUtil::IsWithinEpsilon(vec.x(), 0.f);
  const bool y_zero = cc::MathUtil::IsWithinEpsilon(vec.y(), 0.f);
  if (x_zero && !y_zero)
    return (vec.y() > 0.f) ? AXIS_POS_Y : AXIS_NEG_Y;
  else if (y_zero && !x_zero)
    return (vec.x() > 0.f) ? AXIS_POS_X : AXIS_NEG_X;
  else
    return NONE;
}

gfx::OverlayTransform GetOverlayTransform(const gfx::Transform& quad_transform,
                                          bool y_flipped) {
  if (!quad_transform.Preserves2dAxisAlignment()) {
    return gfx::OVERLAY_TRANSFORM_INVALID;
  }

  gfx::Vector3dF x_axis = cc::MathUtil::GetXAxis(quad_transform);
  gfx::Vector3dF y_axis = cc::MathUtil::GetYAxis(quad_transform);
  if (y_flipped) {
    y_axis.Scale(-1.f);
  }

  Axis x_to = VectorToAxis(x_axis);
  Axis y_to = VectorToAxis(y_axis);

  if (x_to == AXIS_POS_X && y_to == AXIS_POS_Y)
    return gfx::OVERLAY_TRANSFORM_NONE;
  else if (x_to == AXIS_NEG_X && y_to == AXIS_POS_Y)
    return gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
  else if (x_to == AXIS_POS_X && y_to == AXIS_NEG_Y)
    return gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
  else if (x_to == AXIS_NEG_Y && y_to == AXIS_POS_X)
    return gfx::OVERLAY_TRANSFORM_ROTATE_270;
  else if (x_to == AXIS_NEG_X && y_to == AXIS_NEG_Y)
    return gfx::OVERLAY_TRANSFORM_ROTATE_180;
  else if (x_to == AXIS_POS_Y && y_to == AXIS_NEG_X)
    return gfx::OVERLAY_TRANSFORM_ROTATE_90;
  else
    return gfx::OVERLAY_TRANSFORM_INVALID;
}

}  // namespace

OverlayCandidate::OverlayCandidate() = default;

OverlayCandidate::OverlayCandidate(const OverlayCandidate& other) = default;

OverlayCandidate::~OverlayCandidate() = default;

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromDrawQuad(
    const DrawQuad* quad,
    OverlayCandidate& candidate) const {
  // It is currently not possible to set a color conversion matrix on an HW
  // overlay plane.
  // TODO(https://crbug.com/792757): Remove this check once the bug is resolved.
  if (*output_color_matrix_ != SkM44())
    return CandidateStatus::kFailColorMatrix;

  const SharedQuadState* sqs = quad->shared_quad_state;

  // We don't support an opacity value different than one for an overlay plane.
  // Render pass quads should have their |sqs| opacity integrated directly into
  // their final output buffers.
  if (!is_delegated_context_ &&
      !cc::MathUtil::IsWithinEpsilon(sqs->opacity, 1.0f)) {
    return CandidateStatus::kFailOpacity;
  }
  candidate.opacity = sqs->opacity;
  candidate.rounded_corners = sqs->mask_filter_info.rounded_corner_bounds();

  // We support only kSrc (no blending) and kSrcOver (blending with premul).
  if (!(sqs->blend_mode == SkBlendMode::kSrc ||
        sqs->blend_mode == SkBlendMode::kSrcOver)) {
    return CandidateStatus::kFailBlending;
  }

  candidate.requires_overlay = OverlayCandidate::RequiresOverlay(quad);
  candidate.overlay_damage_index =
      sqs->overlay_damage_index.value_or(OverlayCandidate::kInvalidDamageIndex);

  switch (quad->material) {
    case DrawQuad::Material::kTextureContent:
      return FromTextureQuad(TextureDrawQuad::MaterialCast(quad), candidate);
    case DrawQuad::Material::kVideoHole:
      return FromVideoHoleQuad(VideoHoleDrawQuad::MaterialCast(quad),
                               candidate);
    case DrawQuad::Material::kStreamVideoContent:
      return FromStreamVideoQuad(StreamVideoDrawQuad::MaterialCast(quad),
                                 candidate);
    case DrawQuad::Material::kSolidColor:
      if (!is_delegated_context_)
        return CandidateStatus::kFailQuadNotSupported;
      return FromSolidColorQuad(SolidColorDrawQuad::MaterialCast(quad),
                                candidate);
    case DrawQuad::Material::kAggregatedRenderPass:
      if (!is_delegated_context_)
        return CandidateStatus::kFailQuadNotSupported;
      return FromAggregateQuad(AggregatedRenderPassDrawQuad::MaterialCast(quad),
                               candidate);
    case DrawQuad::Material::kTiledContent:
      if (!is_delegated_context_)
        return CandidateStatus::kFailQuadNotSupported;
      return FromTileQuad(TileDrawQuad::MaterialCast(quad), candidate);
    default:
      break;
  }

  return CandidateStatus::kFailQuadNotSupported;
}

// static
bool OverlayCandidate::IsInvisibleQuad(const DrawQuad* quad) {
  float opacity = quad->shared_quad_state->opacity;
  if (cc::MathUtil::IsWithinEpsilon(opacity, 0.f))
    return true;
  if (quad->material != DrawQuad::Material::kSolidColor)
    return false;
  const float alpha =
      SolidColorDrawQuad::MaterialCast(quad)->color.fA * opacity;
  return quad->ShouldDrawWithBlending() &&
         cc::MathUtil::IsWithinEpsilon(alpha, 0.f);
}

// static
bool OverlayCandidate::IsOccluded(const OverlayCandidate& candidate,
                                  QuadList::ConstIterator quad_list_begin,
                                  QuadList::ConstIterator quad_list_end) {
  // The rects are rounded as they're snapped by the compositor to pixel unless
  // it is AA'ed, in which case, it won't be overlaid.
  gfx::Rect display_rect = gfx::ToRoundedRect(candidate.display_rect);

  // Check that no visible quad overlaps the candidate.
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    gfx::Rect overlap_rect = gfx::ToRoundedRect(cc::MathUtil::MapClippedRect(
        overlap_iter->shared_quad_state->quad_to_target_transform,
        gfx::RectF(overlap_iter->rect)));

    if (!OverlayCandidate::IsInvisibleQuad(*overlap_iter) &&
        display_rect.Intersects(overlap_rect)) {
      return true;
    }
  }
  return false;
}

OverlayCandidateFactory::OverlayCandidateFactory(
    const AggregatedRenderPass* render_pass,
    DisplayResourceProvider* resource_provider,
    const SurfaceDamageRectList* surface_damage_rect_list,
    const SkM44* output_color_matrix,
    const gfx::RectF primary_rect,
    bool is_delegated_context)
    : render_pass_(render_pass),
      resource_provider_(resource_provider),
      surface_damage_rect_list_(surface_damage_rect_list),
      output_color_matrix_(output_color_matrix),
      primary_rect_(primary_rect),
      is_delegated_context_(is_delegated_context) {
  // TODO(crbug.com/1323002): Replace this set with a simple ordered linear
  // search when this bug is resolved.
  base::flat_set<size_t> indices_with_quad_damage;
  for (auto* sqs : render_pass_->shared_quad_state_list) {
    // If a |sqs| has a damage index it will only be associated with a single
    // draw quad.
    if (sqs->overlay_damage_index.has_value()) {
      indices_with_quad_damage.insert(sqs->overlay_damage_index.value());
    }
  }

  for (size_t i = 0; i < (*surface_damage_rect_list_).size(); i++) {
    // Add this damage only if it does not correspond to a specific quad.
    // Ideally any damage that we might want to separate out (think overlays)
    // will not end up in this |unassigned_surface_damage_| rect.
    if (!indices_with_quad_damage.contains(i)) {
      unassigned_surface_damage_.Union((*surface_damage_rect_list_)[i]);
    }
  }
}

OverlayCandidateFactory::~OverlayCandidateFactory() = default;

float OverlayCandidateFactory::EstimateVisibleDamage(
    const DrawQuad* quad,
    const OverlayCandidate& candidate,
    QuadList::ConstIterator quad_list_begin,
    QuadList::ConstIterator quad_list_end) const {
  gfx::Rect quad_damage = gfx::ToEnclosingRect(GetDamageRect(quad, candidate));
  float occluded_damage_estimate_total = 0.f;
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    gfx::Rect overlap_rect = gfx::ToEnclosingRect(cc::MathUtil::MapClippedRect(
        overlap_iter->shared_quad_state->quad_to_target_transform,
        gfx::RectF(overlap_iter->rect)));

    // Opaque quad that (partially) occludes this candidate.
    if (!OverlayCandidate::IsInvisibleQuad(*overlap_iter) &&
        !overlap_iter->ShouldDrawWithBlending()) {
      overlap_rect.Intersect(quad_damage);
      occluded_damage_estimate_total += overlap_rect.size().GetArea();
    }
  }
  // In the case of overlapping UI the |occluded_damage_estimate_total| may
  // exceed the |quad|'s damage rect that is in consideration. This is the
  // reason why this computation is an estimate and why we have the max clamping
  // below.
  return std::max(
      0.f, quad_damage.size().GetArea() - occluded_damage_estimate_total);
}

// static
bool OverlayCandidate::RequiresOverlay(const DrawQuad* quad) {
  // Regular priority hint.
  switch (quad->material) {
    case DrawQuad::Material::kTextureContent:
      return TextureDrawQuad::MaterialCast(quad)->protected_video_type ==
                 gfx::ProtectedVideoType::kHardwareProtected ||
             TextureDrawQuad::MaterialCast(quad)->overlay_priority_hint ==
                 OverlayPriority::kRequired;
    case DrawQuad::Material::kVideoHole:
      return true;
    case DrawQuad::Material::kYuvVideoContent:
      return YUVVideoDrawQuad::MaterialCast(quad)->protected_video_type ==
             gfx::ProtectedVideoType::kHardwareProtected;
    default:
      return false;
  }
}

bool OverlayCandidateFactory::IsOccludedByFilteredQuad(
    const OverlayCandidate& candidate,
    QuadList::ConstIterator quad_list_begin,
    QuadList::ConstIterator quad_list_end,
    const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
        render_pass_backdrop_filters) const {
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    if (overlap_iter->material == DrawQuad::Material::kAggregatedRenderPass) {
      gfx::RectF overlap_rect = cc::MathUtil::MapClippedRect(
          overlap_iter->shared_quad_state->quad_to_target_transform,
          gfx::RectF(overlap_iter->rect));
      const auto* render_pass_draw_quad =
          AggregatedRenderPassDrawQuad::MaterialCast(*overlap_iter);
      if (candidate.display_rect.Intersects(overlap_rect) &&
          render_pass_backdrop_filters.count(
              render_pass_draw_quad->render_pass_id)) {
        return true;
      }
    }
  }
  return false;
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromDrawQuadResource(
    const DrawQuad* quad,
    ResourceId resource_id,
    bool y_flipped,
    OverlayCandidate& candidate) const {
  if (resource_id != kInvalidResourceId &&
      !resource_provider_->IsOverlayCandidate(resource_id))
    return CandidateStatus::kFailNotOverlay;

  if (quad->visible_rect.IsEmpty())
    return CandidateStatus::kFailVisible;

  if (resource_id != kInvalidResourceId) {
    candidate.format = resource_provider_->GetBufferFormat(resource_id);
    candidate.color_space = resource_provider_->GetColorSpace(resource_id);
    candidate.hdr_metadata = resource_provider_->GetHDRMetadata(resource_id);

    if (!base::Contains(kOverlayFormats, candidate.format))
      return CandidateStatus::kFailBufferFormat;
  }

  const SharedQuadState* sqs = quad->shared_quad_state;

  gfx::OverlayTransform overlay_transform =
      GetOverlayTransform(sqs->quad_to_target_transform, y_flipped);
  if (overlay_transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return CandidateStatus::kFailNotAxisAligned;
  candidate.transform = overlay_transform;

  auto& transform = sqs->quad_to_target_transform;
  candidate.display_rect = gfx::RectF(quad->rect);
  transform.TransformRect(&candidate.display_rect);

  candidate.clip_rect = sqs->clip_rect;
  candidate.is_opaque =
      !quad->ShouldDrawWithBlendingForReasonOtherThanMaskFilter();
  candidate.has_mask_filter = !sqs->mask_filter_info.IsEmpty();

  if (resource_id != kInvalidResourceId) {
    candidate.resource_size_in_pixels =
        resource_provider_->GetResourceBackedSize(resource_id);
  } else {
    candidate.resource_size_in_pixels =
        gfx::Size(candidate.display_rect.size().width(),
                  candidate.display_rect.size().height());
  }

  AssignDamage(quad, candidate);
  candidate.resource_id = resource_id;

  struct TrackingIdData {
    gfx::Rect rect;
    FrameSinkId frame_sink_id;
  };

  TrackingIdData track_data{quad->rect, FrameSinkId()};
  if (resource_id != kInvalidResourceId) {
    candidate.mailbox = resource_provider_->GetMailbox(resource_id);
    track_data.frame_sink_id =
        resource_provider_->GetSurfaceId(resource_id).frame_sink_id();
  }

  // Delegated compositing does not yet support |clip_rect| so it is applied
  // here to the |display_rect| and |uv_rect| directly.

  if (is_delegated_context_) {
    if (candidate.clip_rect.has_value())
      OverlayCandidate::ApplyClip(candidate, gfx::RectF(*candidate.clip_rect));

    if (quad->visible_rect != quad->rect) {
      auto visible_rect = gfx::RectF(quad->visible_rect);
      transform.TransformRect(&visible_rect);
      OverlayCandidate::ApplyClip(candidate, gfx::RectF(visible_rect));
    }
    // TODO(https://crbug.com/1300552) : Tile quads can overlay other quads and
    // the window by one pixel. Exo does not yet clip these quads so we need to
    // clip here with the |primary_rect|.
    OverlayCandidate::ApplyClip(candidate, primary_rect_);

    if (candidate.display_rect.IsEmpty())
      return CandidateStatus::kFailVisible;
  }

  candidate.tracking_id = base::Hash(&track_data, sizeof(track_data));
  return CandidateStatus::kSuccess;
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromAggregateQuad(
    const AggregatedRenderPassDrawQuad* quad,
    OverlayCandidate& candidate) const {
  auto rtn = FromDrawQuadResource(quad, kInvalidResourceId, false, candidate);
  if (rtn == CandidateStatus::kSuccess) {
    candidate.rpdq = quad;
  }
  return rtn;
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromSolidColorQuad(
    const SolidColorDrawQuad* quad,
    OverlayCandidate& candidate) const {
  auto rtn = FromDrawQuadResource(quad, kInvalidResourceId, false, candidate);

  if (rtn == CandidateStatus::kSuccess) {
    // TODO(crbug/1308932) remove toSkColor and make all SkColor4f
    candidate.color = quad->color.toSkColor();
    // Mark this candidate a solid color as the |color| member can be either a
    // background of the overlay or a color of the solid color quad.
    candidate.is_solid_color = true;
  }
  return rtn;
}

// For VideoHoleDrawQuad, only calculate geometry information and put it in the
// |candidate|.
OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromVideoHoleQuad(
    const VideoHoleDrawQuad* quad,
    OverlayCandidate& candidate) const {
  gfx::OverlayTransform overlay_transform = GetOverlayTransform(
      quad->shared_quad_state->quad_to_target_transform, false);
  if (overlay_transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return CandidateStatus::kFailNotAxisAligned;

  auto& transform = quad->shared_quad_state->quad_to_target_transform;
  candidate.display_rect = gfx::RectF(quad->rect);
  transform.TransformRect(&candidate.display_rect);
  candidate.transform = overlay_transform;
  candidate.is_opaque =
      !quad->ShouldDrawWithBlendingForReasonOtherThanMaskFilter();
  candidate.has_mask_filter =
      !quad->shared_quad_state->mask_filter_info.IsEmpty();

  AssignDamage(quad, candidate);
  candidate.tracking_id = base::FastHash(quad->overlay_plane_id.AsBytes());

  return CandidateStatus::kSuccess;
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromTileQuad(
    const TileDrawQuad* quad,
    OverlayCandidate& candidate) const {
  if (quad->nearest_neighbor)
    return CandidateStatus::kFailNearFilter;

  candidate.resource_size_in_pixels =
      resource_provider_->GetResourceBackedSize(quad->resource_id());
  candidate.uv_rect = gfx::ScaleRect(
      quad->tex_coord_rect, 1.f / candidate.resource_size_in_pixels.width(),
      1.f / candidate.resource_size_in_pixels.height());

  auto rtn = FromDrawQuadResource(quad, quad->resource_id(), false, candidate);
  return rtn;
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromTextureQuad(
    const TextureDrawQuad* quad,
    OverlayCandidate& candidate) const {
  if (!is_delegated_context_ &&
      quad->overlay_priority_hint == OverlayPriority::kLow) {
    // For current implementation low priority means this does not promote to
    // overlay.
    return CandidateStatus::kFailPriority;
  }

  if (quad->nearest_neighbor)
    return CandidateStatus::kFailNearFilter;

  if (quad->background_color != SkColors::kTransparent &&
      (quad->background_color != SkColors::kBlack ||
       quad->ShouldDrawWithBlending())) {
    // This path can also be used by other platforms like Ash/Chrome, which does
    // not support overlays with background color. Only LaCros/Wayland supports
    // that.
    if (!is_delegated_context_)
      return CandidateStatus::kFailBlending;
    // TODO(crbug/1308932) remove toSkColor and make all SkColor4f
    candidate.color = quad->background_color.toSkColor();
  }

  candidate.uv_rect = BoundingRect(quad->uv_top_left, quad->uv_bottom_right);

  auto rtn = FromDrawQuadResource(quad, quad->resource_id(), quad->y_flipped,
                                  candidate);
  if (rtn == CandidateStatus::kSuccess) {
    // Only handle clip rect for required overlays
    if (!is_delegated_context_ && candidate.requires_overlay)
      HandleClipAndSubsampling(candidate);

    // Texture quads for UI elements like scroll bars have empty
    // |size_in_pixels| as 'set_resource_size_in_pixels' is not called as these
    // quads are not intended to become overlays.
    if (!quad->resource_size_in_pixels().IsEmpty())
      candidate.priority_hint = gfx::OverlayPriorityHint::kRegular;
  }
  return rtn;
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromStreamVideoQuad(
    const StreamVideoDrawQuad* quad,
    OverlayCandidate& candidate) const {
  auto rtn = FromDrawQuadResource(quad, quad->resource_id(), false, candidate);

  if (rtn == CandidateStatus::kSuccess) {
    candidate.resource_size_in_pixels = quad->resource_size_in_pixels();
    candidate.uv_rect = BoundingRect(quad->uv_top_left, quad->uv_bottom_right);
#if BUILDFLAG(IS_ANDROID)
    candidate.is_backed_by_surface_texture =
        resource_provider_->IsBackedBySurfaceTexture(quad->resource_id());
#endif
  }
  return rtn;
}

void OverlayCandidateFactory::HandleClipAndSubsampling(
    OverlayCandidate& candidate) const {
  // The purpose of this is to enable overlays that are required (i.e. protected
  // content) to be able to be shown in all cases. This will allow them to pass
  // the clipping check and also the 2x alignment requirement for subsampling in
  // the Intel DRM driver. This should not be used in cases where the surface
  // will not always be promoted to an overlay as it will lead to shifting of
  // the content when it switches between composition and overlay.
  if (!candidate.clip_rect)
    return;

  // Make sure it's in a format we can deal with, we only support YUV and P010.
  if (candidate.format != gfx::BufferFormat::YUV_420_BIPLANAR &&
      candidate.format != gfx::BufferFormat::P010) {
    return;
  }
  // Clip the clip rect to the primary plane. An overlay will only be shown on
  // a single display, so we want to perform our calculations within the bounds
  // of that display.
  if (!primary_rect_.IsEmpty())
    candidate.clip_rect->Intersect(gfx::ToNearestRect(primary_rect_));

  // Calculate |uv_rect| of |clip_rect| in |display_rect|
  gfx::RectF uv_rect = cc::MathUtil::ScaleRectProportional(
      candidate.uv_rect, candidate.display_rect,
      gfx::RectF(*candidate.clip_rect));

  // In case that |uv_rect| of candidate is not (0, 0, 1, 1)
  candidate.uv_rect.Intersect(uv_rect);

  // Update |display_rect| to avoid unexpected scaling and the candidate should
  // not be regarded as clippped after this.
  candidate.display_rect.Intersect(gfx::RectF(*candidate.clip_rect));
  candidate.clip_rect.reset();
  gfx::Rect rounded_display_rect = gfx::ToRoundedRect(candidate.display_rect);
  candidate.display_rect.SetRect(
      rounded_display_rect.x(), rounded_display_rect.y(),
      rounded_display_rect.width(), rounded_display_rect.height());

  // Now correct |uv_rect| if required so that the source rect aligns on a pixel
  // boundary that is a multiple of the chroma subsampling.

  // Get the rect for the source coordinates.
  gfx::RectF src_rect = gfx::ScaleRect(
      candidate.uv_rect, candidate.resource_size_in_pixels.width(),
      candidate.resource_size_in_pixels.height());
  // Make it an integral multiple of the subsampling factor.
  auto subsample_round = [](float val) {
    constexpr int kSubsamplingFactor = 2;
    return (std::lround(val) / kSubsamplingFactor) * kSubsamplingFactor;
  };

  src_rect.set_x(subsample_round(src_rect.x()));
  src_rect.set_y(subsample_round(src_rect.y()));
  src_rect.set_width(subsample_round(src_rect.width()));
  src_rect.set_height(subsample_round(src_rect.height()));
  // Scale it back into UV space and set it in the candidate.
  candidate.uv_rect =
      gfx::ScaleRect(src_rect, 1.0f / candidate.resource_size_in_pixels.width(),
                     1.0f / candidate.resource_size_in_pixels.height());
}

void OverlayCandidateFactory::AssignDamage(const DrawQuad* quad,
                                           OverlayCandidate& candidate) const {
  auto& transform = quad->shared_quad_state->quad_to_target_transform;
  auto damage_rect = GetDamageRect(quad, candidate);
  auto transformed_damage = damage_rect;
  gfx::Transform inv;
  if (transform.GetInverse(&inv)) {
    inv.TransformRect(&transformed_damage);
    // The quad's |rect| is in content space. To get to buffer space we need
    // to remove the |rect|'s pixel offset.
    auto buffer_damage_origin =
        transformed_damage.origin() - gfx::PointF(quad->rect.origin());
    transformed_damage.set_origin(
        gfx::PointF(buffer_damage_origin.x(), buffer_damage_origin.y()));

    if (!quad->rect.IsEmpty()) {
      // Normalize damage to be in UVs.
      transformed_damage.Scale(1.0f / quad->rect.width(),
                               1.0f / quad->rect.height());
    }

    // The normalization above is not enough if the |uv_rect| is not 0,0-1x1.
    // This is because texture uvs can effectively magnify damage.
    if (!candidate.uv_rect.IsEmpty()) {
      transformed_damage.Scale(candidate.uv_rect.width(),
                               candidate.uv_rect.height());
      transformed_damage.Offset(candidate.uv_rect.OffsetFromOrigin());
    }

    // Buffer damage is in texels not UVs so scale by resource size.
    transformed_damage.Scale(candidate.resource_size_in_pixels.width(),
                             candidate.resource_size_in_pixels.height());
  } else {
    // If not invertible, set to full damage.
    // TODO(https://crbug.com/1279965): |resource_size_in_pixels| might not be
    // properly initialized at this stage.
    transformed_damage =
        gfx::RectF(gfx::SizeF(candidate.resource_size_in_pixels));
  }
  // For underlays the function 'EstimateVisibleDamage()' is called to update
  // |damage_area_estimate| to more accurately reflect the actual visible
  // damage.
  candidate.damage_area_estimate = damage_rect.size().GetArea();
  candidate.damage_rect = transformed_damage;
}

// static
void OverlayCandidate::ApplyClip(OverlayCandidate& candidate,
                                 const gfx::RectF& clip_rect) {
  if (!clip_rect.Contains(candidate.display_rect)) {
    gfx::RectF intersect_clip_display = clip_rect;
    intersect_clip_display.Intersect(candidate.display_rect);
    gfx::RectF uv_rect = cc::MathUtil::ScaleRectProportional(
        candidate.uv_rect, candidate.display_rect, intersect_clip_display);
    candidate.display_rect = intersect_clip_display;
    candidate.uv_rect = uv_rect;
  }
}

gfx::RectF OverlayCandidateFactory::GetDamageRect(
    const DrawQuad* quad,
    const OverlayCandidate& candidate) const {
  const SharedQuadState* sqs = quad->shared_quad_state;
  if (!sqs->overlay_damage_index.has_value()) {
    // This is a special case where an overlay candidate may have damage but it
    // does not have a damage index since it was not the only quad in the
    // original surface. Here the |unassigned_surface_damage_| will contain all
    // unassigned damage and we use it to conservatively estimate the damage for
    // this quad. We limit the damage to the candidates quad rect in question.
    gfx::RectF intersection = candidate.display_rect;
    intersection.Intersect(gfx::RectF(unassigned_surface_damage_));
    return intersection;
  }

  size_t overlay_damage_index = sqs->overlay_damage_index.value();
  // Invalid index.
  if (overlay_damage_index >= surface_damage_rect_list_->size()) {
    DCHECK(false);
    return gfx::RectF();
  }

  return gfx::RectF((*surface_damage_rect_list_)[overlay_damage_index]);
}

}  // namespace viz
