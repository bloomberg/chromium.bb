// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/surface_aggregator.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/cxx20_erase.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "components/viz/common/display/de_jelly.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/renderer_utils.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_allocation_group.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_transform_utils.h"

namespace viz {

struct MaskFilterInfoExt {
  MaskFilterInfoExt() = default;
  MaskFilterInfoExt(const gfx::MaskFilterInfo& mask_filter_info_arg,
                    bool is_fast_rounded_corner_arg,
                    const gfx::Transform target_transform)
      : mask_filter_info(mask_filter_info_arg),
        is_fast_rounded_corner(is_fast_rounded_corner_arg) {
    if (mask_filter_info.IsEmpty())
      return;
    bool success = mask_filter_info.Transform(target_transform);
    DCHECK(success);
  }

  // Returns true if the quads from |merge_render_pass| can be merged into
  // the embedding render pass based on mask filter info.
  bool CanMergeMaskFilterInfo(
      const CompositorRenderPass& merge_render_pass) const {
    // If the embedding quad has no mask filter, then we do not have to block
    // merging.
    if (mask_filter_info.IsEmpty())
      return true;

    // If the embedding quad has rounded corner and it is not a fast rounded
    // corner, we cannot merge.
    if (mask_filter_info.HasRoundedCorners() && !is_fast_rounded_corner)
      return false;

    // If any of the quads in the render pass to merged has a mask filter of its
    // own, then we cannot merge.
    for (const auto* sqs : merge_render_pass.shared_quad_state_list) {
      if (!sqs->mask_filter_info.IsEmpty())
        return false;
    }
    return true;
  }

  gfx::MaskFilterInfo mask_filter_info;
  bool is_fast_rounded_corner;
};

namespace {

// Used for determine when to treat opacity close to 1.f as opaque. The value is
// chosen to be smaller than 1/255.
constexpr float kOpacityEpsilon = 0.001f;

void MoveMatchingRequests(
    CompositorRenderPassId render_pass_id,
    std::multimap<CompositorRenderPassId, std::unique_ptr<CopyOutputRequest>>*
        copy_requests,
    std::vector<std::unique_ptr<CopyOutputRequest>>* output_requests) {
  auto request_range = copy_requests->equal_range(render_pass_id);
  for (auto it = request_range.first; it != request_range.second; ++it) {
    DCHECK(it->second);
    output_requests->push_back(std::move(it->second));
  }
  copy_requests->erase(request_range.first, request_range.second);
}

// Returns true if the damage rect is valid.
bool CalculateQuadSpaceDamageRect(
    const gfx::Transform& quad_to_target_transform,
    const gfx::Transform& target_to_root_transform,
    const gfx::Rect& root_damage_rect,
    gfx::Rect* quad_space_damage_rect) {
  gfx::Transform quad_to_root_transform(target_to_root_transform,
                                        quad_to_target_transform);
  gfx::Transform inverse_transform(gfx::Transform::kSkipInitialization);
  bool inverse_valid = quad_to_root_transform.GetInverse(&inverse_transform);
  if (!inverse_valid)
    return false;

  *quad_space_damage_rect = cc::MathUtil::ProjectEnclosingClippedRect(
      inverse_transform, root_damage_rect);
  return true;
}

gfx::Rect GetExpandedRectWithPixelMovingForegroundFilter(
    const CompositorRenderPassDrawQuad* rpdq,
    const CompositorRenderPass& child_render_pass) {
  const SharedQuadState* shared_quad_state = rpdq->shared_quad_state;
  float max_pixel_movement = child_render_pass.filters.MaximumPixelMovement();
  gfx::RectF rect(rpdq->rect);
  rect.Inset(-max_pixel_movement, -max_pixel_movement);
  gfx::Rect expanded_rect = gfx::ToEnclosingRect(rect);

  // expanded_rect in the target space
  return cc::MathUtil::MapEnclosingClippedRect(
      shared_quad_state->quad_to_target_transform, expanded_rect);
}

// Create a clip rect for an aggregated quad from the original clip rect and
// the clip rect from the surface it's on.
absl::optional<gfx::Rect> CalculateClipRect(
    const absl::optional<gfx::Rect>& surface_clip,
    const absl::optional<gfx::Rect>& quad_clip,
    const gfx::Transform& target_transform) {
  absl::optional<gfx::Rect> out_clip;
  if (surface_clip)
    out_clip = surface_clip;

  if (quad_clip) {
    // TODO(jamesr): This only works if target_transform maps integer
    // rects to integer rects.
    gfx::Rect final_clip =
        cc::MathUtil::MapEnclosingClippedRect(target_transform, *quad_clip);
    if (out_clip)
      out_clip->Intersect(final_clip);
    else
      out_clip = final_clip;
  }

  return out_clip;
}

// Creates a new SharedQuadState in |dest_render_pass| based on |source_sqs|
// plus additional modified values.
// - |source_sqs| is the SharedQuadState to copy from.
// - |quad_to_target_transform| replaces the equivalent |source_sqs| value.
// - |target_transform| is an additional transform to add. Used when merging the
//    root render pass of a surface into the embedding render pass.
// - |quad_layer_rect| replaces the equivalent |source_sqs| value.
// - |visible_quad_layer_rect| replaces the equivalent |source_sqs| value.
// - |mask_filter_info_ext| replaces the equivalent |source_sqs| values.
// - |added_clip_rect| is an additional clip rect added to the quad clip rect.
// - |dest_render_pass| is where the new SharedQuadState will be created.
SharedQuadState* CopyAndScaleSharedQuadState(
    const SharedQuadState* source_sqs,
    const gfx::Transform& quad_to_target_transform,
    const gfx::Transform& target_transform,
    const gfx::Rect& quad_layer_rect,
    const gfx::Rect& visible_quad_layer_rect,
    const absl::optional<gfx::Rect>& added_clip_rect,
    const MaskFilterInfoExt& mask_filter_info_ext,
    AggregatedRenderPass* dest_render_pass) {
  auto* shared_quad_state = dest_render_pass->CreateAndAppendSharedQuadState();
  auto new_clip_rect = CalculateClipRect(added_clip_rect, source_sqs->clip_rect,
                                         target_transform);

  // target_transform contains any transformation that may exist
  // between the context that these quads are being copied from (i.e. the
  // surface's draw transform when aggregated from within a surface) to the
  // target space of the pass. This will be identity except when copying the
  // root draw pass from a surface into a pass when the surface draw quad's
  // transform is not identity.
  gfx::Transform new_transform = quad_to_target_transform;
  new_transform.ConcatTransform(target_transform);

  shared_quad_state->SetAll(
      new_transform, quad_layer_rect, visible_quad_layer_rect,
      mask_filter_info_ext.mask_filter_info, new_clip_rect,
      source_sqs->are_contents_opaque, source_sqs->opacity,
      source_sqs->blend_mode, source_sqs->sorting_context_id);
  shared_quad_state->is_fast_rounded_corner =
      mask_filter_info_ext.is_fast_rounded_corner,
  shared_quad_state->de_jelly_delta_y = source_sqs->de_jelly_delta_y;
  return shared_quad_state;
}

// Creates a new SharedQuadState in |dest_render_pass| and copies |source_sqs|
// into it. See CopyAndScaleSharedQuadState() for full documentation.
SharedQuadState* CopySharedQuadState(
    const SharedQuadState* source_sqs,
    const gfx::Transform& target_transform,
    const absl::optional<gfx::Rect>& added_clip_rect,
    const MaskFilterInfoExt& mask_filter_info,
    AggregatedRenderPass* dest_render_pass) {
  return CopyAndScaleSharedQuadState(
      source_sqs, source_sqs->quad_to_target_transform, target_transform,
      source_sqs->quad_layer_rect, source_sqs->visible_quad_layer_rect,
      added_clip_rect, mask_filter_info, dest_render_pass);
}

// Returns true if |resolved_pass| needs full damage. This is because:
// 1. The render pass pixels will be saved, either by a copy request or into a
//    cached render pass. This avoids a partially drawn render pass being saved.
// 2. The render pass pixels will have a pixel moving foreground filter applied
//    to them. In this case pixels outside the damage_rect can be moved inside
//    the damage_rect by the filter.
bool RenderPassNeedsFullDamage(const ResolvedPassData& resolved_pass) {
  auto& aggregation = resolved_pass.aggregation();
  return aggregation.in_cached_render_pass ||
         aggregation.in_copy_request_pass ||
         aggregation.in_pixel_moving_filter_pass;
}

}  // namespace

constexpr base::TimeDelta SurfaceAggregator::kHistogramMinTime;
constexpr base::TimeDelta SurfaceAggregator::kHistogramMaxTime;

struct SurfaceAggregator::PrewalkResult {
  // This is the set of Surfaces that were referenced by another Surface, but
  // not included in a SurfaceDrawQuad.
  base::flat_set<SurfaceId> undrawn_surfaces;
  bool video_capture_enabled = false;
  bool may_contain_video = false;
  bool frame_sinks_changed = false;
  gfx::ContentColorUsage content_color_usage = gfx::ContentColorUsage::kSRGB;
};

SurfaceAggregator::SurfaceAggregator(SurfaceManager* manager,
                                     DisplayResourceProvider* provider,
                                     bool aggregate_only_damaged,
                                     bool needs_surface_damage_rect_list)
    : manager_(manager),
      provider_(provider),
      aggregate_only_damaged_(aggregate_only_damaged),
      needs_surface_damage_rect_list_(needs_surface_damage_rect_list),
      de_jelly_enabled_(DeJellyEnabled()) {
  DCHECK(manager_);
  DCHECK(provider_);
}

SurfaceAggregator::~SurfaceAggregator() {
  // Notify client of all surfaces being removed.
  contained_surfaces_.clear();
  contained_frame_sinks_.clear();
  ProcessAddedAndRemovedSurfaces();
}

int SurfaceAggregator::ChildIdForSurface(Surface* surface) {
  auto it = surface_id_to_resource_child_id_.find(surface->surface_id());
  if (it == surface_id_to_resource_child_id_.end()) {
    int child_id = provider_->CreateChild(
        base::BindRepeating(&SurfaceClient::UnrefResources, surface->client()),
        surface->surface_id());
    surface_id_to_resource_child_id_[surface->surface_id()] = child_id;
    return child_id;
  } else {
    return it->second;
  }
}

gfx::Rect SurfaceAggregator::DamageRectForSurface(
    const ResolvedFrameData& resolved_frame,
    bool include_per_quad_damage) const {
  const Surface* surface = resolved_frame.surface();

  // The |damage_rect| set in |SurfaceAnimationManager| is the |output_rect|.
  // However, we dont use |damage_rect| because when we transition from
  // interpolated frame we would end up using the |damage_rect| from the
  // original non interpolated frame.
  // TODO(vmpstr): This damage may be too large, but I think it's hard to figure
  // out a small bounds on the damage given an animation that happens in
  // SurfaceAnimationManager.
  if (surface->HasSurfaceAnimationDamage())
    return resolved_frame.GetOutputRect();

  // TODO(crbug.com/1194082): Cache the previous frame_index in
  // ResolvedFrameData to avoid multiple maps lookups here.
  auto it = previous_contained_surfaces_.find(surface->surface_id());
  if (it != previous_contained_surfaces_.end()) {
    uint64_t previous_index = it->second;
    // The resolved frame index is unchanged from last aggregation so there is
    // no damage.
    if (previous_index == surface->GetActiveFrameIndex())
      return gfx::Rect();
  }

  const SurfaceId& previous_surface_id = surface->previous_frame_surface_id();
  if (surface->surface_id() != previous_surface_id) {
    it = previous_contained_surfaces_.find(previous_surface_id);
  }

  if (it != previous_contained_surfaces_.end()) {
    uint64_t previous_index = it->second;
    if (previous_index == surface->GetActiveFrameIndex() - 1) {
      return resolved_frame.GetDamageRect(include_per_quad_damage);
    }
  }

  return resolved_frame.GetOutputRect();
}

// This function is called at each render pass - CopyQuadsToPass().
void SurfaceAggregator::AddRenderPassFilterDamageToDamageList(
    const gfx::Transform& parent_target_transform,
    const CompositorRenderPass* source_pass,
    AggregatedRenderPass* dest_pass) {
  // Add damages from render passes with pixel-moving foreground filters or
  // backdrop filters to the surface damage list.
  if (!source_pass->filters.HasFilterThatMovesPixels() &&
      !source_pass->backdrop_filters.HasFilterThatMovesPixels()) {
    return;
  }

  gfx::Transform parent_to_root_target_transform = gfx::Transform(
      dest_pass->transform_to_root_target, parent_target_transform);

  gfx::Rect damage_rect = source_pass->output_rect;
  if (source_pass->filters.HasFilterThatMovesPixels()) {
    float max_pixel_movement = source_pass->filters.MaximumPixelMovement();
    gfx::RectF damage_rect_f(damage_rect);
    damage_rect_f.Inset(-max_pixel_movement, -max_pixel_movement);
    damage_rect = gfx::ToEnclosingRect(damage_rect_f);
  }

  gfx::Rect damage_rect_in_root_target_space =
      cc::MathUtil::MapEnclosingClippedRect(parent_to_root_target_transform,
                                            damage_rect);

  // The whole render pass rect with pixel-moving foreground filters or
  // backdrop filters is considered damaged if it intersects with the other
  // damages.
  if (damage_rect_in_root_target_space.Intersects(root_damage_rect_)) {
    // Transform will be performed again in AddSurfaceDamageToDamageList()
    // Just pass in damage_rect instead of damage_rect_in_root_target_space.
    AddSurfaceDamageToDamageList(damage_rect, gfx::Transform(), {}, dest_pass,
                                 /*resolved_frame=*/nullptr);
  }
}
void SurfaceAggregator::AddSurfaceDamageToDamageList(
    const gfx::Rect& default_damage_rect,
    const gfx::Transform& parent_target_transform,
    const absl::optional<gfx::Rect>& clip_rect,
    AggregatedRenderPass* dest_pass,
    const ResolvedFrameData* resolved_frame) {
  gfx::Rect damage_rect;
  if (!resolved_frame) {
    // When the surface is null, it's either the surface is lost or it comes
    // from a render pass with filters.
    damage_rect = default_damage_rect;
  } else {
    if (RenderPassNeedsFullDamage(resolved_frame->GetRootRenderPassData())) {
      damage_rect = resolved_frame->GetOutputRect();
    } else {
      damage_rect = DamageRectForSurface(*resolved_frame, false);
    }
  }

  if (damage_rect.IsEmpty()) {
    current_zero_damage_rect_is_not_recorded_ = true;
    return;
  }
  current_zero_damage_rect_is_not_recorded_ = false;

  gfx::Transform parent_to_root_target_transform = gfx::Transform(
      dest_pass->transform_to_root_target, parent_target_transform);

  gfx::Rect damage_rect_in_root_target_space =
      cc::MathUtil::MapEnclosingClippedRect(parent_to_root_target_transform,
                                            damage_rect);

  if (clip_rect) {
    gfx::Rect root_clip_rect = cc::MathUtil::MapEnclosingClippedRect(
        dest_pass->transform_to_root_target, *clip_rect);
    damage_rect_in_root_target_space.Intersect(root_clip_rect);
  }

  surface_damage_rect_list_->push_back(damage_rect_in_root_target_space);
}

// This function returns the overlay candidate quad ptr which has an
// overlay_damage_index pointing to the its damage rect in
// surface_damage_rect_list_. |overlay_damage_index| will be saved in the shared
// quad state later.
// This function is called at CopyQuadsToPass().
const DrawQuad* SurfaceAggregator::FindQuadWithOverlayDamage(
    const CompositorRenderPass& source_pass,
    AggregatedRenderPass* dest_pass,
    const gfx::Transform& parent_target_transform,
    const Surface* surface,
    const absl::optional<gfx::Rect>& clip_rect,
    size_t* overlay_damage_index) {
  // If we have damage from a surface animation, then we shouldn't have an
  // overlay candidate from the root render pass, since that's an interpolated
  // pass with "artificial" damage.
  if (surface->HasSurfaceAnimationDamage())
    return nullptr;

  // Only process the damage rect at the root render pass, once per surface.
  const CompositorFrame& frame = surface->GetActiveFrame();
  bool is_last_pass_on_src_surface =
      &source_pass == frame.render_pass_list.back().get();
  if (!is_last_pass_on_src_surface)
    return nullptr;

  // The occluding damage optimization currently relies on two things - there
  // can't be any damage above the quad within the surface, and the quad needs
  // its own SQS for the occluding_damage_rect metadata.
  const DrawQuad* target_quad = nullptr;
  for (auto* quad : source_pass.quad_list) {
    // Quads with |per_quad_damage| do not contribute to the |damage_rect| in
    // the |source_pass|. These quads are also assumed to have unique SQS
    // objects.
    if (source_pass.has_per_quad_damage) {
      auto optional_damage = GetOptionalDamageRectFromQuad(quad);
      if (optional_damage.has_value()) {
        continue;
      }
    }

    if (target_quad == nullptr) {
      target_quad = quad;
    } else {
      // More that one quad without per_quad_damage.
      target_quad = nullptr;
      break;
    }
  }

  // No overlay candidate is found.
  if (!target_quad)
    return nullptr;

  // Zero damage is not recorded in the surface_damage_rect_list_.
  // In this case, add an empty damage rect to the list so
  // |overlay_damage_index| can save this index.
  if (current_zero_damage_rect_is_not_recorded_) {
    current_zero_damage_rect_is_not_recorded_ = false;
    surface_damage_rect_list_->push_back(gfx::Rect());
  }

  // The latest surface damage rect.
  *overlay_damage_index = surface_damage_rect_list_->size() - 1;

  return target_quad;
}

bool SurfaceAggregator::CanPotentiallyMergePass(
    const SurfaceDrawQuad& surface_quad) {
  const SharedQuadState* sqs = surface_quad.shared_quad_state;
  return surface_quad.allow_merge &&
         base::IsApproximatelyEqual(sqs->opacity, 1.f, kOpacityEpsilon) &&
         sqs->de_jelly_delta_y == 0;
}

const ResolvedFrameData* SurfaceAggregator::GetLatestFrameData(
    const SurfaceId& surface_id) {
  auto* surface = manager_->GetSurfaceForId(surface_id);
  return GetResolvedFrame(surface, /*inside_aggregation=*/false);
}

ResolvedFrameData* SurfaceAggregator::GetResolvedFrame(
    const SurfaceRange& range) {
  // Find latest in flight surface and cache that result for the duration of
  // this aggregation, then find ResolvedFrameData for that surface.
  auto iter = resolved_surface_ranges_.find(range);
  if (iter == resolved_surface_ranges_.end()) {
    iter = resolved_surface_ranges_
               .emplace(range, manager_->GetLatestInFlightSurface(range))
               .first;
  }

  return GetResolvedFrame(iter->second, /*inside_aggregation=*/true);
}

ResolvedFrameData* SurfaceAggregator::GetResolvedFrame(
    const SurfaceId& surface_id) {
  return GetResolvedFrame(manager_->GetSurfaceForId(surface_id),
                          /*inside_aggregation=*/true);
}

ResolvedFrameData* SurfaceAggregator::GetResolvedFrame(
    Surface* surface,
    bool inside_aggregation) {
  if (!surface || !surface->HasActiveFrame()) {
    // If there is no resolved surface or the surface has no active frame there
    // is no resolved frame data to return.
    return nullptr;
  }

  auto iter = resolved_frames_.find(surface);
  if (iter == resolved_frames_.end()) {
    iter =
        resolved_frames_
            .emplace(std::piecewise_construct, std::forward_as_tuple(surface),
                     std::forward_as_tuple(surface->surface_id(), surface))
            .first;
  }

  ResolvedFrameData& resolved_frame = iter->second;
  DCHECK_EQ(resolved_frame.surface(), surface);

  // Verify that a new surface wasn't created at the same address as a deleted
  // surface with a different SurfaceId.
  // TODO(kylechar): Invalidate cached resolved frame data when
  // SurfaceObserver signals the surface has been destroyed instead.
  if (resolved_frame.surface_id() != surface->surface_id()) {
    resolved_frames_.erase(iter);
    return GetResolvedFrame(surface, inside_aggregation);
  }

  // Mark the frame as used this aggregation so it persists.
  bool first_use = inside_aggregation ? resolved_frame.MarkAsUsed() : true;

  if (first_use) {
    // If there is a new CompositorFrame for `surface` compute resolved frame
    // data for the new resolved CompositorFrame.
    if (resolved_frame.frame_index() != surface->GetActiveFrameIndex() ||
        surface->HasSurfaceAnimationDamage()) {
      DCHECK(inside_aggregation);
      base::ElapsedTimer timer;
      ProcessResolvedFrame(resolved_frame);
      stats_->declare_resources_time += timer.Elapsed();
    }
  }

  return &resolved_frame;
}

void SurfaceAggregator::HandleSurfaceQuad(
    const SurfaceDrawQuad* surface_quad,
    float parent_device_scale_factor,
    const gfx::Transform& target_transform,
    const absl::optional<gfx::Rect>& clip_rect,
    AggregatedRenderPass* dest_pass,
    bool ignore_undamaged,
    gfx::Rect* damage_rect_in_quad_space,
    bool* damage_rect_in_quad_space_valid,
    const MaskFilterInfoExt& mask_filter_info) {
  SurfaceId primary_surface_id = surface_quad->surface_range.end();
  const ResolvedFrameData* resolved_frame =
      GetResolvedFrame(surface_quad->surface_range);

  // If a new surface is going to be emitted, add the surface_quad rect to
  // |surface_damage_rect_list_| for overlays. The whole quad is considered
  // damaged.
  if (needs_surface_damage_rect_list_ &&
      (!resolved_frame ||
       (resolved_frame->surface_id() != primary_surface_id))) {
    const SharedQuadState* surface_quad_sqs = surface_quad->shared_quad_state;

    // TODO(crbug.com/1261917, crbug.com/1257765 and crbug.com/1263146):
    // Integer overflows caused by huge rect sizes in surface_quad->visible_rect
    // occur in some crash dumps. Use sqs->visible_quad_layer_rect instead
    // because it seems more accurate in those crash dumps. Revisit this comment
    // after the huge rect size bug is fixed.
    gfx::Rect default_damage_rect = cc::MathUtil::MapEnclosingClippedRect(
        surface_quad_sqs->quad_to_target_transform,
        surface_quad_sqs->visible_quad_layer_rect);
    if (surface_quad_sqs->clip_rect) {
      default_damage_rect.Intersect(surface_quad_sqs->clip_rect.value());
    }

    AddSurfaceDamageToDamageList(default_damage_rect, target_transform,
                                 clip_rect, dest_pass,
                                 /*resolved_frame=*/nullptr);
  }

  // If there's no fallback surface ID available, then simply emit a
  // SolidColorDrawQuad with the provided default background color. This
  // can happen after a Viz process crash.
  if (!resolved_frame) {
    EmitDefaultBackgroundColorQuad(surface_quad, target_transform, clip_rect,
                                   dest_pass, mask_filter_info);
    return;
  }

  if (resolved_frame->surface_id() != primary_surface_id &&
      !surface_quad->stretch_content_to_fill_bounds) {
    const CompositorFrame& fallback_frame =
        resolved_frame->surface()->GetActiveOrInterpolatedFrame();

    gfx::Rect fallback_rect(fallback_frame.size_in_pixels());

    float scale_ratio =
        parent_device_scale_factor / fallback_frame.device_scale_factor();
    fallback_rect =
        gfx::ScaleToEnclosingRect(fallback_rect, scale_ratio, scale_ratio);
    fallback_rect = gfx::IntersectRects(fallback_rect, surface_quad->rect);

    EmitGutterQuadsIfNecessary(surface_quad->rect, fallback_rect,
                               surface_quad->shared_quad_state,
                               target_transform, clip_rect,
                               fallback_frame.metadata.root_background_color,
                               dest_pass, mask_filter_info);
  }

  EmitSurfaceContent(*resolved_frame, parent_device_scale_factor, surface_quad,
                     target_transform, clip_rect, dest_pass, ignore_undamaged,
                     damage_rect_in_quad_space, damage_rect_in_quad_space_valid,
                     mask_filter_info);
}

void SurfaceAggregator::EmitSurfaceContent(
    const ResolvedFrameData& resolved_frame,
    float parent_device_scale_factor,
    const SurfaceDrawQuad* surface_quad,
    const gfx::Transform& target_transform,
    const absl::optional<gfx::Rect>& clip_rect,
    AggregatedRenderPass* dest_pass,
    bool ignore_undamaged,
    gfx::Rect* damage_rect_in_quad_space,
    bool* damage_rect_in_quad_space_valid,
    const MaskFilterInfoExt& mask_filter_info) {
  Surface* surface = resolved_frame.surface();

  // If this surface's id is already in our referenced set then it creates
  // a cycle in the graph and should be dropped.
  SurfaceId surface_id = surface->surface_id();
  if (referenced_surfaces_.count(surface_id))
    return;

  ++stats_->copied_surface_count;

  const CompositorFrame& frame = surface->GetActiveOrInterpolatedFrame();

  // If we are stretching content to fill the SurfaceDrawQuad, or if the device
  // scale factor mismatches between content and SurfaceDrawQuad, we appply an
  // additional scale.
  float extra_content_scale_x, extra_content_scale_y;
  if (surface_quad->stretch_content_to_fill_bounds) {
    const gfx::Rect& surface_quad_rect = surface_quad->rect;
    // Stretches the surface contents to exactly fill the layer bounds,
    // regardless of scale or aspect ratio differences.
    extra_content_scale_x = surface_quad_rect.width() /
                            static_cast<float>(frame.size_in_pixels().width());
    extra_content_scale_y = surface_quad_rect.height() /
                            static_cast<float>(frame.size_in_pixels().height());
  } else {
    extra_content_scale_x = extra_content_scale_y =
        parent_device_scale_factor / frame.device_scale_factor();
  }
  float inverse_extra_content_scale_x = SK_Scalar1 / extra_content_scale_x;
  float inverse_extra_content_scale_y = SK_Scalar1 / extra_content_scale_y;

  const SharedQuadState* surface_quad_sqs = surface_quad->shared_quad_state;
  gfx::Transform scaled_quad_to_target_transform(
      surface_quad_sqs->quad_to_target_transform);
  scaled_quad_to_target_transform.Scale(extra_content_scale_x,
                                        extra_content_scale_y);

  TRACE_EVENT_WITH_FLOW2(
      "viz,benchmark", "Graphics.Pipeline",
      TRACE_ID_GLOBAL(frame.metadata.begin_frame_ack.trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SurfaceAggregation", "display_trace", display_trace_id_);

  const gfx::Rect& surface_quad_visible_rect = surface_quad->visible_rect;
  if (ignore_undamaged) {
    gfx::Transform quad_to_target_transform(
        target_transform, surface_quad_sqs->quad_to_target_transform);
    *damage_rect_in_quad_space_valid = CalculateQuadSpaceDamageRect(
        quad_to_target_transform, dest_pass->transform_to_root_target,
        root_damage_rect_, damage_rect_in_quad_space);
    if (*damage_rect_in_quad_space_valid &&
        !damage_rect_in_quad_space->Intersects(surface_quad_visible_rect)) {
      return;
    }
  }

  // A map keyed by RenderPass id.
  Surface::CopyRequestsMap copy_requests;
  surface->TakeCopyOutputRequests(&copy_requests);

  const CompositorRenderPassList& render_pass_list = frame.render_pass_list;
  if (!resolved_frame.is_valid()) {
    // As |copy_requests| goes out-of-scope, all copy requests in that container
    // will auto-send an empty result upon destruction.
    return;
  }

  referenced_surfaces_.insert(surface_id);

  gfx::Transform combined_transform = scaled_quad_to_target_transform;
  combined_transform.ConcatTransform(target_transform);

  // If the SurfaceDrawQuad is marked as being reflected and surface contents
  // are going to be scaled then keep the RenderPass. This allows the reflected
  // surface to be drawn with AA enabled for smooth scaling and preserves the
  // original reflector scaling behaviour which scaled a TextureLayer.
  bool reflected_and_scaled =
      surface_quad->is_reflection &&
      !scaled_quad_to_target_transform.IsIdentityOrTranslation();

  // We cannot merge passes if de-jelly is being applied, as we must have a
  // renderpass to skew.
  bool merge_pass =
      CanPotentiallyMergePass(*surface_quad) && !reflected_and_scaled &&
      copy_requests.empty() && combined_transform.Preserves2dAxisAlignment() &&
      mask_filter_info.CanMergeMaskFilterInfo(*render_pass_list.back());

  absl::optional<gfx::Rect> quads_clip;
  if (merge_pass || needs_surface_damage_rect_list_) {
    // Intersect the transformed surface visible rect and the clip rect to
    // create a smaller cliprect for the quad.
    gfx::Rect surface_quad_clip_rect = cc::MathUtil::MapEnclosingClippedRect(
        surface_quad_sqs->quad_to_target_transform, surface_quad_visible_rect);
    if (surface_quad_sqs->clip_rect) {
      surface_quad_clip_rect.Intersect(*surface_quad_sqs->clip_rect);
    }

    quads_clip =
        CalculateClipRect(clip_rect, surface_quad_clip_rect, target_transform);
  }

  if (needs_surface_damage_rect_list_) {
    AddSurfaceDamageToDamageList(/*default_damage_rect=*/gfx::Rect(),
                                 combined_transform, quads_clip, dest_pass,
                                 &resolved_frame);
  }

  if (frame.metadata.delegated_ink_metadata) {
    // The metadata must be taken off of the surface, rather than a copy being
    // made, in order to ensure that the delegated ink metadata is used for
    // exactly one frame. Otherwise, it could potentially end up being used to
    // draw the same trail on multiple frames if a new CompositorFrame wasn't
    // generated.
    TransformAndStoreDelegatedInkMetadata(
        gfx::Transform(dest_pass->transform_to_root_target, combined_transform),
        surface->TakeDelegatedInkMetadata());
  }

  // TODO(fsamuel): Move this to a separate helper function.
  const auto& resolved_passes = resolved_frame.GetResolvedPasses();
  size_t num_render_passes = resolved_passes.size();
  size_t passes_to_copy =
      merge_pass ? num_render_passes - 1 : num_render_passes;
  for (size_t j = 0; j < passes_to_copy; ++j) {
    const ResolvedPassData& resolved_pass = resolved_passes[j];
    const CompositorRenderPass& source = resolved_pass.render_pass();

    size_t sqs_size = source.shared_quad_state_list.size();
    size_t dq_size = source.quad_list.size();
    auto copy_pass = std::make_unique<AggregatedRenderPass>(sqs_size, dq_size);

    gfx::Rect output_rect = source.output_rect;
    if (max_render_target_size_ > 0) {
      output_rect.set_width(
          std::min(output_rect.width(), max_render_target_size_));
      output_rect.set_height(
          std::min(output_rect.height(), max_render_target_size_));
    }
    copy_pass->SetAll(
        resolved_pass.remapped_id(), output_rect, output_rect,
        source.transform_to_root_target, source.filters,
        source.backdrop_filters, source.backdrop_filter_bounds,
        root_content_color_usage_, source.has_transparent_background,
        source.cache_render_pass,
        resolved_pass.aggregation().has_damage_from_contributing_content,
        source.generate_mipmap);

    MoveMatchingRequests(source.id, &copy_requests, &copy_pass->copy_requests);

    // Contributing passes aggregated in to the pass list need to take the
    // transform of the surface quad into account to update their transform to
    // the root surface.
    copy_pass->transform_to_root_target.ConcatTransform(combined_transform);
    copy_pass->transform_to_root_target.ConcatTransform(
        dest_pass->transform_to_root_target);

    CopyQuadsToPass(resolved_frame, resolved_pass, copy_pass.get(),
                    frame.device_scale_factor(), gfx::Transform(), {}, surface,
                    MaskFilterInfoExt());

    // If the render pass has copy requests, or should be cached, or has
    // moving-pixel filters, or in a moving-pixel surface, we should damage the
    // whole output rect so that we always drawn the full content. Otherwise, we
    // might have incompleted copy request, or cached patially drawn render
    // pass.
    if (!RenderPassNeedsFullDamage(resolved_pass)) {
      gfx::Transform inverse_transform(gfx::Transform::kSkipInitialization);
      if (copy_pass->transform_to_root_target.GetInverse(&inverse_transform)) {
        gfx::Rect damage_rect_in_render_pass_space =
            cc::MathUtil::ProjectEnclosingClippedRect(inverse_transform,
                                                      root_damage_rect_);
        copy_pass->damage_rect.Intersect(damage_rect_in_render_pass_space);
      }
    }

    dest_pass_list_->push_back(std::move(copy_pass));
  }

  const auto& last_pass = *render_pass_list.back();
  const auto& resolved_root_pass = resolved_frame.GetRootRenderPassData();

  if (merge_pass) {
    CopyQuadsToPass(resolved_frame, resolved_root_pass, dest_pass,
                    frame.device_scale_factor(), combined_transform, quads_clip,
                    surface, mask_filter_info);
  } else {
    auto* shared_quad_state = CopyAndScaleSharedQuadState(
        surface_quad_sqs, scaled_quad_to_target_transform, target_transform,
        gfx::ScaleToEnclosingRect(surface_quad_sqs->quad_layer_rect,
                                  inverse_extra_content_scale_x,
                                  inverse_extra_content_scale_y),
        gfx::ScaleToEnclosingRect(surface_quad_sqs->visible_quad_layer_rect,
                                  inverse_extra_content_scale_x,
                                  inverse_extra_content_scale_y),
        clip_rect, mask_filter_info, dest_pass);

    // At this point, we need to calculate three values in order to construct
    // the CompositorRenderPassDrawQuad:

    // |quad_rect| - A rectangle representing the RenderPass's output area in
    //   content space. This is equal to the root render pass (|last_pass|)
    //   output rect.
    gfx::Rect quad_rect = last_pass.output_rect;

    // |quad_visible_rect| - A rectangle representing the visible portion of
    //   the RenderPass, in content space. As the SurfaceDrawQuad being
    //   embedded may be clipped further than its root render pass, we use the
    //   surface quad's value - |source_visible_rect|.
    //
    //   There may be an |extra_content_scale_x| applied when going from this
    //   render pass's content space to the surface's content space, we remove
    //   this so that |quad_visible_rect| is in the render pass's content
    //   space.
    gfx::Rect quad_visible_rect(gfx::ScaleToEnclosingRect(
        surface_quad_visible_rect, inverse_extra_content_scale_x,
        inverse_extra_content_scale_y));

    // |tex_coord_rect| - A rectangle representing the bounds of the texture
    //   in the RenderPass's |quad_rect|. Not in content space, instead as an
    //   offset within |quad_rect|.
    gfx::RectF tex_coord_rect = gfx::RectF(gfx::SizeF(quad_rect.size()));

    // We can't produce content outside of |quad_rect|, so clip the visible
    // rect if necessary.
    quad_visible_rect.Intersect(quad_rect);
    auto remapped_pass_id = resolved_root_pass.remapped_id();
    if (quad_visible_rect.IsEmpty()) {
      base::EraseIf(*dest_pass_list_,
                    [&remapped_pass_id](
                        const std::unique_ptr<AggregatedRenderPass>& pass) {
                      return pass->id == remapped_pass_id;
                    });
    } else {
      auto* quad =
          dest_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
      quad->SetNew(shared_quad_state, quad_rect, quad_visible_rect,
                   remapped_pass_id, kInvalidResourceId, gfx::RectF(),
                   gfx::Size(), gfx::Vector2dF(), gfx::PointF(), tex_coord_rect,
                   /*force_anti_aliasing_off=*/false,
                   /* backdrop_filter_quality*/ 1.0f);
    }
  }

  referenced_surfaces_.erase(surface_id);
  surface->DidAggregate();
}

void SurfaceAggregator::EmitDefaultBackgroundColorQuad(
    const SurfaceDrawQuad* surface_quad,
    const gfx::Transform& target_transform,
    const absl::optional<gfx::Rect>& clip_rect,
    AggregatedRenderPass* dest_pass,
    const MaskFilterInfoExt& mask_filter_info) {
  TRACE_EVENT1("viz", "SurfaceAggregator::EmitDefaultBackgroundColorQuad",
               "surface_range", surface_quad->surface_range.ToString());

  // No matching surface was found so create a SolidColorDrawQuad with the
  // SurfaceDrawQuad default background color.
  SkColor background_color = surface_quad->default_background_color;
  auto* shared_quad_state =
      CopySharedQuadState(surface_quad->shared_quad_state, target_transform,
                          clip_rect, mask_filter_info, dest_pass);

  auto* solid_color_quad =
      dest_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_color_quad->SetNew(shared_quad_state, surface_quad->rect,
                           surface_quad->visible_rect, background_color, false);
}

void SurfaceAggregator::EmitGutterQuadsIfNecessary(
    const gfx::Rect& primary_rect,
    const gfx::Rect& fallback_rect,
    const SharedQuadState* primary_shared_quad_state,
    const gfx::Transform& target_transform,
    const absl::optional<gfx::Rect>& clip_rect,
    SkColor background_color,
    AggregatedRenderPass* dest_pass,
    const MaskFilterInfoExt& mask_filter_info) {
  bool has_transparent_background = background_color == SK_ColorTRANSPARENT;

  // If the fallback Surface's active CompositorFrame has a non-transparent
  // background then compute gutter.
  if (has_transparent_background)
    return;

  if (fallback_rect.width() < primary_rect.width()) {
    // The right gutter also includes the bottom-right corner, if necessary.
    gfx::Rect right_gutter_rect(fallback_rect.right(), primary_rect.y(),
                                primary_rect.width() - fallback_rect.width(),
                                primary_rect.height());

    SharedQuadState* shared_quad_state = CopyAndScaleSharedQuadState(
        primary_shared_quad_state,
        primary_shared_quad_state->quad_to_target_transform, target_transform,
        right_gutter_rect, right_gutter_rect, clip_rect, mask_filter_info,
        dest_pass);

    auto* right_gutter =
        dest_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    right_gutter->SetNew(shared_quad_state, right_gutter_rect,
                         right_gutter_rect, background_color, false);
  }

  if (fallback_rect.height() < primary_rect.height()) {
    gfx::Rect bottom_gutter_rect(
        primary_rect.x(), fallback_rect.bottom(), fallback_rect.width(),
        primary_rect.height() - fallback_rect.height());

    SharedQuadState* shared_quad_state = CopyAndScaleSharedQuadState(
        primary_shared_quad_state,
        primary_shared_quad_state->quad_to_target_transform, target_transform,
        bottom_gutter_rect, bottom_gutter_rect, clip_rect, mask_filter_info,
        dest_pass);

    auto* bottom_gutter =
        dest_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bottom_gutter->SetNew(shared_quad_state, bottom_gutter_rect,
                          bottom_gutter_rect, background_color, false);
  }
}

void SurfaceAggregator::AddColorConversionPass() {
  auto* root_render_pass = dest_pass_list_->back().get();
  gfx::Rect output_rect = root_render_pass->output_rect;

  // An extra color conversion pass is only done if the display's color
  // space is unsuitable as a blending color space.
  bool needs_color_conversion_pass =
      !display_color_spaces_
           .GetOutputColorSpace(root_render_pass->content_color_usage,
                                root_render_pass->has_transparent_background)
           .IsSuitableForBlending();

  // If we added or removed the color conversion pass, we need to add full
  // damage to the current-root renderpass (and also the new-root renderpass,
  // if the current-root renderpass becomes and intermediate renderpass).
  if (needs_color_conversion_pass != last_frame_had_color_conversion_pass_)
    root_render_pass->damage_rect = output_rect;

  last_frame_had_color_conversion_pass_ = needs_color_conversion_pass;
  if (!needs_color_conversion_pass)
    return;
  CHECK(root_render_pass->transform_to_root_target == gfx::Transform());

  if (!color_conversion_render_pass_id_)
    color_conversion_render_pass_id_ =
        render_pass_id_generator_.GenerateNextId();

  auto color_conversion_pass = std::make_unique<AggregatedRenderPass>(1, 1);
  color_conversion_pass->SetNew(color_conversion_render_pass_id_, output_rect,
                                root_render_pass->damage_rect,
                                root_render_pass->transform_to_root_target);
  color_conversion_pass->has_transparent_background =
      root_render_pass->has_transparent_background;
  color_conversion_pass->content_color_usage = root_content_color_usage_;
  color_conversion_pass->is_color_conversion_pass = true;

  auto* shared_quad_state =
      color_conversion_pass->CreateAndAppendSharedQuadState();
  // Do NOT set blend mode here to SkBlendMode::kSrcOver, which will cause
  // blending with empty (black) root pass when child pass has alpha.
  shared_quad_state->SetAll(
      /*quad_to_target_transform=*/gfx::Transform(),
      /*quad_layer_rect=*/output_rect,
      /*visible_layer_rect=*/output_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/absl::nullopt, /*are_contents_opaque=*/false,
      /*opacity=*/1.f,
      /*blend_mode=*/SkBlendMode::kSrc, /*sorting_context_id=*/0);

  auto* quad = color_conversion_pass
                   ->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(shared_quad_state, output_rect, output_rect,
               root_render_pass->id, kInvalidResourceId, gfx::RectF(),
               gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
               gfx::RectF(output_rect),
               /*force_anti_aliasing_off=*/false,
               /*backdrop_filter_quality*/ 1.0f);
  dest_pass_list_->push_back(std::move(color_conversion_pass));
}

void SurfaceAggregator::AddDisplayTransformPass() {
  if (dest_pass_list_->empty())
    return;

  auto* root_render_pass = dest_pass_list_->back().get();
  gfx::Rect output_rect = root_render_pass->output_rect;
  DCHECK(root_render_pass->transform_to_root_target == root_surface_transform_);

  if (!display_transform_render_pass_id_)
    display_transform_render_pass_id_ =
        render_pass_id_generator_.GenerateNextId();

  auto display_transform_pass = std::make_unique<AggregatedRenderPass>(1, 1);
  display_transform_pass->SetAll(
      display_transform_render_pass_id_,
      cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          root_surface_transform_, root_render_pass->output_rect),
      cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          root_surface_transform_, root_render_pass->damage_rect),
      gfx::Transform(),
      /*filters=*/cc::FilterOperations(),
      /*backdrop_filters=*/cc::FilterOperations(),
      /*backdrop_filter_bounds=*/gfx::RRectF(),
      root_render_pass->content_color_usage,
      root_render_pass->has_transparent_background,
      /*cache_render_pass=*/false,
      /*has_damage_from_contributing_content=*/false,
      /*generate_mipmap=*/false);

  bool are_contents_opaque = true;
  for (const auto* sqs : root_render_pass->shared_quad_state_list) {
    if (!sqs->are_contents_opaque) {
      are_contents_opaque = false;
      break;
    }
  }

  auto* shared_quad_state =
      display_transform_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(
      /*quad_to_target_transform=*/root_surface_transform_,
      /*quad_layer_rect=*/output_rect,
      /*visible_layer_rect=*/output_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/absl::nullopt, are_contents_opaque, /*opacity=*/1.f,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  auto* quad = display_transform_pass
                   ->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(shared_quad_state, output_rect, output_rect,
               root_render_pass->id, kInvalidResourceId, gfx::RectF(),
               gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
               gfx::RectF(output_rect),
               /*force_anti_aliasing_off=*/false,
               /*backdrop_filter_quality*/ 1.0f);
  dest_pass_list_->push_back(std::move(display_transform_pass));
}

void SurfaceAggregator::CopyQuadsToPass(
    const ResolvedFrameData& resolved_frame,
    const ResolvedPassData& resolved_pass,
    AggregatedRenderPass* dest_pass,
    float parent_device_scale_factor,
    const gfx::Transform& target_transform,
    const absl::optional<gfx::Rect>& clip_rect,
    const Surface* surface,
    const MaskFilterInfoExt& parent_mask_filter_info_ext) {
  const CompositorRenderPass& source_pass = resolved_pass.render_pass();
  const QuadList& source_quad_list = source_pass.quad_list;
  const SharedQuadState* last_copied_source_shared_quad_state = nullptr;

  // If the current frame has copy requests or cached render passes, then
  // aggregate the entire thing, as otherwise parts of the copy requests may be
  // ignored and we could cache partially drawn render pass.
  // If there are pixel-moving backdrop filters then the damage rect might be
  // expanded later, so we can't drop quads that are outside the current damage
  // rect safely.
  // If overlay/underlay is enabled then the underlay rect might be added to the
  // damage rect later. We are not able to predict right here which draw quad
  // candidate will be promoted to overlay/underlay. Also, we might drop quads
  // which are on top of an underlay and cause the overlay processor to
  // present the quad as an overlay instead of an underlay.
  const bool ignore_undamaged =
      aggregate_only_damaged_ && !has_copy_requests_ &&
      !has_pixel_moving_backdrop_filter_ &&
      !resolved_pass.aggregation().in_cached_render_pass &&
      !resolved_pass.aggregation().in_pixel_moving_filter_pass;
  // TODO(kylechar): For copy render passes we only need to draw all quads if
  // those attributes are set on the current render pass' aggregation data. The
  // complication is if a SurfaceDrawQuad is dropped and that surface has a copy
  // request on it then we still need to draw the surface.

  // Damage rect in the quad space of the current shared quad state.
  // TODO(jbauman): This rect may contain unnecessary area if
  // transform isn't axis-aligned.
  gfx::Rect damage_rect_in_quad_space;
  bool damage_rect_in_quad_space_valid = false;

#if DCHECK_IS_ON()
  const SharedQuadStateList& source_shared_quad_state_list =
      source_pass.shared_quad_state_list;
  // If quads have come in with SharedQuadState out of order, or when quads have
  // invalid SharedQuadState pointer, it should DCHECK.
  auto sqs_iter = source_shared_quad_state_list.cbegin();
  for (auto* quad : source_quad_list) {
    while (sqs_iter != source_shared_quad_state_list.cend() &&
           quad->shared_quad_state != *sqs_iter) {
      ++sqs_iter;
    }
    DCHECK(sqs_iter != source_shared_quad_state_list.cend());
  }
#endif

  size_t overlay_damage_index = 0;
  const DrawQuad* quad_with_overlay_damage_index = nullptr;
  if (needs_surface_damage_rect_list_) {
    AddRenderPassFilterDamageToDamageList(target_transform, &source_pass,
                                          dest_pass);
    quad_with_overlay_damage_index =
        FindQuadWithOverlayDamage(source_pass, dest_pass, target_transform,
                                  surface, clip_rect, &overlay_damage_index);
  }

  MaskFilterInfoExt new_mask_filter_info_ext = parent_mask_filter_info_ext;

  size_t quad_index = 0;
  auto& resolved_draw_quads = resolved_pass.draw_quads();
  for (auto* quad : source_quad_list) {
    const ResolvedQuadData& quad_data = resolved_draw_quads[quad_index++];

    // Both cannot be set at once. If this happens then a surface is being
    // merged when it should not.
    DCHECK(quad->shared_quad_state->mask_filter_info.IsEmpty() ||
           parent_mask_filter_info_ext.mask_filter_info.IsEmpty());

    if (quad->material == DrawQuad::Material::kSurfaceContent) {
      const auto* surface_quad = SurfaceDrawQuad::MaterialCast(quad);
      // HandleSurfaceQuad may add other shared quad state, so reset the
      // current data.
      last_copied_source_shared_quad_state = nullptr;

      if (!surface_quad->surface_range.end().is_valid())
        continue;

      if (parent_mask_filter_info_ext.mask_filter_info.IsEmpty()) {
        new_mask_filter_info_ext = MaskFilterInfoExt(
            quad->shared_quad_state->mask_filter_info,
            quad->shared_quad_state->is_fast_rounded_corner, target_transform);
      }

      HandleSurfaceQuad(
          surface_quad, parent_device_scale_factor, target_transform, clip_rect,
          dest_pass, ignore_undamaged, &damage_rect_in_quad_space,
          &damage_rect_in_quad_space_valid, new_mask_filter_info_ext);
    } else {
      if (quad->shared_quad_state != last_copied_source_shared_quad_state) {
        if (parent_mask_filter_info_ext.mask_filter_info.IsEmpty()) {
          new_mask_filter_info_ext =
              MaskFilterInfoExt(quad->shared_quad_state->mask_filter_info,
                                quad->shared_quad_state->is_fast_rounded_corner,
                                target_transform);
        }
        SharedQuadState* dest_shared_quad_state =
            CopySharedQuadState(quad->shared_quad_state, target_transform,
                                clip_rect, new_mask_filter_info_ext, dest_pass);
        // Here we output the optional quad's |per_quad_damage| to the
        // |surface_damage_rect_list_|. Any non per quad damage associated with
        // this |source_pass| will have been added to the
        // |surface_damage_rect_list_| before this phase.
        if (source_pass.has_per_quad_damage &&
            GetOptionalDamageRectFromQuad(quad).has_value()) {
          auto damage_rect_in_target_space =
              GetOptionalDamageRectFromQuad(quad);
          dest_shared_quad_state->overlay_damage_index =
              surface_damage_rect_list_->size();
          AddSurfaceDamageToDamageList(damage_rect_in_target_space.value(),
                                       target_transform, clip_rect, dest_pass,
                                       /*resolved_frame=*/nullptr);
        } else if (quad == quad_with_overlay_damage_index) {
          dest_shared_quad_state->overlay_damage_index = overlay_damage_index;
        }

        if (de_jelly_enabled_) {
          // If a surface is being drawn for a second time, clear our
          // |de_jelly_delta_y|, as de-jelly is only needed the first time
          // a surface draws.
          if (!new_surfaces_.count(surface->surface_id()))
            dest_shared_quad_state->de_jelly_delta_y = 0.0f;
        }

        last_copied_source_shared_quad_state = quad->shared_quad_state;
        if (ignore_undamaged) {
          damage_rect_in_quad_space_valid = CalculateQuadSpaceDamageRect(
              dest_shared_quad_state->quad_to_target_transform,
              dest_pass->transform_to_root_target, root_damage_rect_,
              &damage_rect_in_quad_space);
        }
      }

      if (ignore_undamaged) {
        if (damage_rect_in_quad_space_valid &&
            !damage_rect_in_quad_space.Intersects(quad->visible_rect))
          continue;
      }

      DrawQuad* dest_quad;
      if (quad->material == DrawQuad::Material::kCompositorRenderPass) {
        const auto* pass_quad =
            CompositorRenderPassDrawQuad::MaterialCast(quad);
        CompositorRenderPassId original_pass_id = pass_quad->render_pass_id;
        AggregatedRenderPassId remapped_pass_id =
            resolved_frame.GetRenderPassDataById(original_pass_id)
                .remapped_id();

        dest_quad = dest_pass->CopyFromAndAppendRenderPassDrawQuad(
            pass_quad, remapped_pass_id);
      } else if (quad->material == DrawQuad::Material::kTextureContent) {
        const auto* texture_quad = TextureDrawQuad::MaterialCast(quad);
        if (texture_quad->secure_output_only &&
            (!output_is_secure_ ||
             resolved_pass.aggregation().in_copy_request_pass)) {
          auto* solid_color_quad =
              dest_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
          solid_color_quad->SetNew(dest_pass->shared_quad_state_list.back(),
                                   quad->rect, quad->visible_rect,
                                   SK_ColorBLACK, false);
          dest_quad = solid_color_quad;
        } else {
          dest_quad = dest_pass->CopyFromAndAppendDrawQuad(quad);
        }
      } else {
        dest_quad = dest_pass->CopyFromAndAppendDrawQuad(quad);
      }
      dest_quad->resources = quad_data.remapped_resources;
    }
  }
}

void SurfaceAggregator::CopyPasses(const ResolvedFrameData& resolved_frame) {
  Surface* surface = resolved_frame.surface();
  const CompositorFrame& frame = surface->GetActiveOrInterpolatedFrame();

  // The root surface is allowed to have copy output requests, so grab them
  // off its render passes. This map contains a set of CopyOutputRequests
  // keyed by each RenderPass id.
  Surface::CopyRequestsMap copy_requests;
  surface->TakeCopyOutputRequests(&copy_requests);

  const auto& source_pass_list = frame.render_pass_list;
  if (!resolved_frame.is_valid())
    return;

  ++stats_->copied_surface_count;

  const gfx::Transform surface_transform =
      IsRootSurface(surface) ? root_surface_transform_ : gfx::Transform();

  if (frame.metadata.delegated_ink_metadata) {
    DCHECK(surface->GetActiveFrameMetadata().delegated_ink_metadata ==
           frame.metadata.delegated_ink_metadata);
    // The metadata must be taken off of the surface, rather than a copy being
    // made, in order to ensure that the delegated ink metadata is used for
    // exactly one frame. Otherwise, it could potentially end up being used to
    // draw the same trail on multiple frames if a new CompositorFrame wasn't
    // generated.
    TransformAndStoreDelegatedInkMetadata(
        gfx::Transform(source_pass_list.back()->transform_to_root_target,
                       surface_transform),
        surface->TakeDelegatedInkMetadata());
  }

  bool apply_surface_transform_to_root_pass = true;
  for (auto& resolved_pass : resolved_frame.GetResolvedPasses()) {
    const auto& source = resolved_pass.render_pass();

    size_t sqs_size = source.shared_quad_state_list.size();
    size_t dq_size = source.quad_list.size();
    auto copy_pass = std::make_unique<AggregatedRenderPass>(sqs_size, dq_size);

    MoveMatchingRequests(source.id, &copy_requests, &copy_pass->copy_requests);

    // We add an additional render pass for the transform if the root render
    // pass has any copy requests.
    apply_surface_transform_to_root_pass =
        resolved_pass.is_root() &&
        (copy_pass->copy_requests.empty() || surface_transform.IsIdentity());

    gfx::Rect output_rect = source.output_rect;
    gfx::Transform transform_to_root_target = source.transform_to_root_target;
    if (apply_surface_transform_to_root_pass) {
      // If we don't need an additional render pass to apply the surface
      // transform, adjust the root pass's rects to account for it.
      output_rect = cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          surface_transform, output_rect);
    } else {
      // For the non-root render passes, the transform to root target needs to
      // be adjusted to include the root surface transform. This is also true if
      // we will be adding another render pass for the surface transform, in
      // which this will no longer be the root.
      transform_to_root_target =
          gfx::Transform(surface_transform, source.transform_to_root_target);
    }

    copy_pass->SetAll(
        resolved_pass.remapped_id(), output_rect, output_rect,
        transform_to_root_target, source.filters, source.backdrop_filters,
        source.backdrop_filter_bounds, root_content_color_usage_,
        source.has_transparent_background, source.cache_render_pass,
        resolved_pass.aggregation().has_damage_from_contributing_content,
        source.generate_mipmap);

    if (needs_surface_damage_rect_list_ && resolved_pass.is_root()) {
      AddSurfaceDamageToDamageList(
          /*default_damage_rect=*/gfx::Rect(), surface_transform,
          /*clip_rect=*/{}, copy_pass.get(), &resolved_frame);
    }

    CopyQuadsToPass(resolved_frame, resolved_pass, copy_pass.get(),
                    frame.device_scale_factor(),
                    apply_surface_transform_to_root_pass ? surface_transform
                                                         : gfx::Transform(),
                    {}, surface, MaskFilterInfoExt());

    // If the render pass has copy requests, or should be cached, or has
    // moving-pixel filters, or in a moving-pixel surface, we should damage the
    // whole output rect so that we always drawn the full content. Otherwise, we
    // might have incompleted copy request, or cached patially drawn render
    // pass.
    if (!RenderPassNeedsFullDamage(resolved_pass)) {
      gfx::Transform inverse_transform(gfx::Transform::kSkipInitialization);
      if (copy_pass->transform_to_root_target.GetInverse(&inverse_transform)) {
        gfx::Rect damage_rect_in_render_pass_space =
            cc::MathUtil::ProjectEnclosingClippedRect(inverse_transform,
                                                      root_damage_rect_);
        copy_pass->damage_rect.Intersect(damage_rect_in_render_pass_space);
      }
    }
    dest_pass_list_->push_back(std::move(copy_pass));
  }

  if (!apply_surface_transform_to_root_pass)
    AddDisplayTransformPass();
}

void SurfaceAggregator::ProcessAddedAndRemovedSurfaces() {
  for (const auto& surface : previous_contained_surfaces_) {
    if (!contained_surfaces_.count(surface.first))
      // Release resources of removed surface.
      ReleaseResources(surface.first);
  }
}

gfx::Rect SurfaceAggregator::PrewalkRenderPass(
    ResolvedFrameData& resolved_frame,
    ResolvedPassData& resolved_pass,
    bool will_draw,
    const gfx::Rect& damage_from_parent,
    const gfx::Transform& target_to_root_transform,
    const ResolvedPassData* parent_pass,
    PrewalkResult& result) {
  const CompositorRenderPass& render_pass = resolved_pass.render_pass();

  if (render_pass.backdrop_filters.HasFilterThatMovesPixels()) {
    has_pixel_moving_backdrop_filter_ = true;
  }

  // Populate state for about cached render passes and pixel moving filters.
  // These attributes apply transitively to all child render passes embedded by
  // the CompositorRenderPass with the attribute.
  if (render_pass.cache_render_pass ||
      (parent_pass && parent_pass->aggregation().in_cached_render_pass)) {
    resolved_pass.aggregation().in_cached_render_pass = true;
  }

  if (render_pass.filters.HasFilterThatMovesPixels() ||
      (parent_pass && parent_pass->aggregation().in_pixel_moving_filter_pass)) {
    resolved_pass.aggregation().in_pixel_moving_filter_pass = true;
  }

  if (render_pass.has_damage_from_contributing_content) {
    resolved_pass.aggregation().has_damage_from_contributing_content = true;
  }

  // The damage on the root render pass of the surface comes from damage
  // accumulated from all quads in the surface, and needs to be expanded by any
  // pixel-moving backdrop filter in the render pass if intersecting. Transform
  // this damage into the local space of the render pass for this purpose.
  gfx::Rect surface_root_rp_damage = DamageRectForSurface(resolved_frame, true);
  if (!surface_root_rp_damage.IsEmpty()) {
    gfx::Transform root_to_target_transform(
        gfx::Transform::kSkipInitialization);
    if (target_to_root_transform.GetInverse(&root_to_target_transform)) {
      surface_root_rp_damage = cc::MathUtil::ProjectEnclosingClippedRect(
          root_to_target_transform, surface_root_rp_damage);
    }
  }

  gfx::Rect damage_rect;
  // Iterate through the quad list back-to-front and accumulate damage from
  // all quads (only SurfaceDrawQuads and RenderPassDrawQuads can have damage
  // at this point). |damage_rect| has damage from all quads below the current
  // iterated quad, and can be used to determine if there's any intersection
  // with the current quad when needed.
  for (const DrawQuad* quad : base::Reversed(resolved_pass.prewalk_quads())) {
    gfx::Rect quad_damage_rect;
    if (quad->material == DrawQuad::Material::kSurfaceContent) {
      const auto* surface_quad = SurfaceDrawQuad::MaterialCast(quad);
      ResolvedFrameData* child_resolved_frame =
          GetResolvedFrame(surface_quad->surface_range);

      // If the primary surface is not available then we assume the damage is
      // the full size of the SurfaceDrawQuad because we might need to introduce
      // gutter.
      if (!child_resolved_frame || child_resolved_frame->surface_id() !=
                                       surface_quad->surface_range.end()) {
        quad_damage_rect = quad->rect;
      }

      if (child_resolved_frame) {
        float x_scale = SK_Scalar1;
        float y_scale = SK_Scalar1;
        if (surface_quad->stretch_content_to_fill_bounds) {
          const gfx::Size& child_size =
              child_resolved_frame->surface()->size_in_pixels();
          if (!child_size.IsEmpty()) {
            x_scale = static_cast<float>(surface_quad->rect.width()) /
                      child_size.width();
            y_scale = static_cast<float>(surface_quad->rect.height()) /
                      child_size.height();
          }
        } else {
          // If not stretching to fit bounds then scale to adjust to device
          // scale factor differences between child and parent surface. This
          // scale factor is later applied to quads in the aggregated frame.
          x_scale = y_scale =
              resolved_frame.surface()->device_scale_factor() /
              child_resolved_frame->surface()->device_scale_factor();
        }
        // If the surface quad is to be merged potentially, the current
        // effective accumulated damage needs to be taken into account. This
        // includes the damage from quads under the surface quad, i.e.
        // |damage_rect|, |surface_root_rp_damage|, which can contain damage
        // contributed by quads under the surface quad in the previous stage
        // (cc), and |damage_from_parent|. The damage is first transformed into
        // the local space of the surface quad and then passed to the embedding
        // surface. The condition for deciding if the surface quad will merge is
        // loose here, so for those quads passed this condition but eventually
        // don't merge, there is over-contribution of the damage passed from
        // parent, but this shouldn't affect correctness.
        gfx::Rect accumulated_damage_in_child_space;

        if (CanPotentiallyMergePass(*surface_quad)) {
          accumulated_damage_in_child_space.Union(damage_rect);
          accumulated_damage_in_child_space.Union(damage_from_parent);
          accumulated_damage_in_child_space.Union(surface_root_rp_damage);
          if (!accumulated_damage_in_child_space.IsEmpty()) {
            gfx::Transform inverse(gfx::Transform::kSkipInitialization);
            bool inverted =
                quad->shared_quad_state->quad_to_target_transform.GetInverse(
                    &inverse);
            DCHECK(inverted);
            inverse.PostScale(SK_Scalar1 / x_scale, SK_Scalar1 / y_scale);
            accumulated_damage_in_child_space =
                cc::MathUtil::ProjectEnclosingClippedRect(
                    inverse, accumulated_damage_in_child_space);
          }
        }
        gfx::Rect child_rect =
            PrewalkSurface(*child_resolved_frame, &resolved_pass, will_draw,
                           accumulated_damage_in_child_space, result);
        child_rect = gfx::ScaleToEnclosingRect(child_rect, x_scale, y_scale);
        quad_damage_rect.Union(child_rect);
      }

      if (!quad_damage_rect.IsEmpty()) {
        resolved_pass.aggregation().has_damage_from_contributing_content = true;
      }
    } else if (quad->material == DrawQuad::Material::kCompositorRenderPass) {
      auto* render_pass_quad = CompositorRenderPassDrawQuad::MaterialCast(quad);

      CompositorRenderPassId child_pass_id = render_pass_quad->render_pass_id;

      ResolvedPassData& child_resolved_pass =
          resolved_frame.GetRenderPassDataById(child_pass_id);
      const CompositorRenderPass& child_render_pass =
          child_resolved_pass.render_pass();

      gfx::Rect rect_in_target_space = cc::MathUtil::MapEnclosingClippedRect(
          quad->shared_quad_state->quad_to_target_transform, quad->rect);

      // |damage_rect|, |damage_from_parent| and |surface_root_rp_damage|
      // either are or can possible contain damage from under the quad, so if
      // they intersect the quad render pass output rect, we have to invalidate
      // the |intersects_damage_under| flag. Note the intersection test can be
      // done against backdrop filter bounds as an improvement.
      bool intersects_current_damage =
          rect_in_target_space.Intersects(damage_rect);
      bool intersects_damage_from_parent =
          rect_in_target_space.Intersects(damage_from_parent);
      // The |intersects_damage_under| flag hints if the current quad intersects
      // any damage from any quads below in the same surface. If the flag is
      // false, it means the intersecting damage is from quads above it or from
      // itself.
      bool intersects_damage_from_surface =
          rect_in_target_space.Intersects(surface_root_rp_damage);
      if (intersects_current_damage || intersects_damage_from_parent ||
          intersects_damage_from_surface) {
        render_pass_quad->intersects_damage_under = true;

        if (child_render_pass.backdrop_filters.HasFilterThatMovesPixels()) {
          // The damage from under the quad intersects quad render pass output
          // rect and it has to be expanded because of the pixel-moving
          // backdrop filters. We expand the |damage_rect| to include quad
          // render pass output rect (which can be optimized to be backdrop
          // filter bounds). |damage_from_parent| and |surface_root_rp_damage|
          // only have to be included when they also have intersection with the
          // quad.
          damage_rect.Union(rect_in_target_space);
          if (intersects_damage_from_parent) {
            damage_rect.Union(damage_from_parent);
          }
          if (intersects_damage_from_surface) {
            damage_rect.Union(surface_root_rp_damage);
          }
        }
      }
      // For the pixel-moving backdrop filters, all effects are limited to the
      // size of the RenderPassDrawQuad rect. Therefore when we find the damage
      // under the quad intersects quad render pass output rect, we extend the
      // damage rect to include the rpdq->rect.

      // For the pixel-moving foreground filters, all effects can be expanded
      // outside the RenderPassDrawQuad rect to the size of rect +
      // filters.MaximumPixelMovement(). Therefore, we have to check if
      // (rpdq->rect + MaximumPixelMovement()) intersects the damage under it.
      // Then we extend the damage rect to include the (rpdq->rect +
      // MaximumPixelMovement()).

      // Expand the damage to cover entire |output_rect| if the |render_pass|
      // has pixel-moving foreground filter.
      if (child_render_pass.filters.HasFilterThatMovesPixels()) {
        gfx::Rect expanded_rect_in_target_space =
            GetExpandedRectWithPixelMovingForegroundFilter(render_pass_quad,
                                                           child_render_pass);

        if (expanded_rect_in_target_space.Intersects(damage_rect) ||
            expanded_rect_in_target_space.Intersects(damage_from_parent) ||
            expanded_rect_in_target_space.Intersects(surface_root_rp_damage)) {
          damage_rect.Union(expanded_rect_in_target_space);
        }
      }

      resolved_pass.aggregation().embedded_passes.insert(&child_resolved_pass);

      const gfx::Transform child_to_root_transform(
          target_to_root_transform,
          quad->shared_quad_state->quad_to_target_transform);
      quad_damage_rect = PrewalkRenderPass(
          resolved_frame, child_resolved_pass, will_draw, gfx::Rect(),
          child_to_root_transform, &resolved_pass, result);

      if (child_resolved_pass.aggregation()
              .has_damage_from_contributing_content) {
        resolved_pass.aggregation().has_damage_from_contributing_content = true;
      }
    }

    if (!quad_damage_rect.IsEmpty()) {
      // Convert the quad damage rect into its target space and clip it if
      // needed. Ignore tiny errors to avoid artificially inflating the
      // damage due to floating point math.
      constexpr float kEpsilon = 0.001f;
      gfx::Rect rect_in_target_space =
          cc::MathUtil::MapEnclosingClippedRectIgnoringError(
              quad->shared_quad_state->quad_to_target_transform,
              quad_damage_rect, kEpsilon);
      if (quad->shared_quad_state->clip_rect) {
        rect_in_target_space.Intersect(*quad->shared_quad_state->clip_rect);
      }
      damage_rect.Union(rect_in_target_space);
    }
  }

  // Expand the damage to cover entire |output_rect| if the |render_pass| has
  // pixel-moving foreground filter.
  if (!damage_rect.IsEmpty() && render_pass.filters.HasFilterThatMovesPixels())
    damage_rect.Union(render_pass.output_rect);
  return damage_rect;
}

void SurfaceAggregator::ProcessResolvedFrame(
    ResolvedFrameData& resolved_frame) {
  Surface* surface = resolved_frame.surface();
  const CompositorFrame& compositor_frame =
      surface->GetActiveOrInterpolatedFrame();
  auto& resource_list = compositor_frame.resource_list;

  int child_id = ChildIdForSurface(surface);

  // Ref the resources in the surface, and let the provider know we've received
  // new resources from the compositor frame.
  if (surface->client())
    surface->client()->RefResources(resource_list);
  provider_->ReceiveFromChild(child_id, resource_list);

  stats_->declare_resources_count += resource_list.size();

  ResourceIdSet resource_set = resolved_frame.UpdateForActiveFrame(
      provider_->GetChildToParentMap(child_id), render_pass_id_generator_);

  // Declare the used resources to the provider. This will cause all resources
  // that were received but not used in the render passes to be unreferenced in
  // the surface, and returned to the child in the resource provider.
  provider_->DeclareUsedResourcesFromChild(child_id, resource_set);
}

bool SurfaceAggregator::CheckFrameSinksChanged(const Surface* surface) {
  contained_surfaces_[surface->surface_id()] = surface->GetActiveFrameIndex();
  LocalSurfaceId& local_surface_id =
      contained_frame_sinks_[surface->surface_id().frame_sink_id()];
  bool frame_sinks_changed = (!previous_contained_frame_sinks_.contains(
      surface->surface_id().frame_sink_id()));
  local_surface_id =
      std::max(surface->surface_id().local_surface_id(), local_surface_id);
  return frame_sinks_changed;
}

gfx::Rect SurfaceAggregator::PrewalkSurface(ResolvedFrameData& resolved_frame,
                                            ResolvedPassData* parent_pass,
                                            bool will_draw,
                                            const gfx::Rect& damage_from_parent,
                                            PrewalkResult& result) {
  Surface* surface = resolved_frame.surface();
  DCHECK(surface->HasActiveFrame());
  DebugLogSurface(surface, will_draw);

  if (referenced_surfaces_.count(surface->surface_id()))
    return gfx::Rect();

  result.frame_sinks_changed |= CheckFrameSinksChanged(surface);

  if (!resolved_frame.is_valid())
    return gfx::Rect();
  ++stats_->prewalked_surface_count;

  auto& root_resolved_pass = resolved_frame.GetRootRenderPassData();
  if (parent_pass) {
    parent_pass->aggregation().embedded_passes.insert(&root_resolved_pass);
  }

  gfx::Rect damage_rect = DamageRectForSurface(resolved_frame, true);

  // Avoid infinite recursion by adding current surface to
  // |referenced_surfaces_|.
  referenced_surfaces_.insert(surface->surface_id());

  damage_rect.Union(PrewalkRenderPass(resolved_frame, root_resolved_pass,
                                      will_draw, damage_from_parent,
                                      gfx::Transform(), parent_pass, result));

  // If this surface has damage from contributing content, then the render pass
  // embedding this surface does as well.
  if (parent_pass &&
      root_resolved_pass.aggregation().has_damage_from_contributing_content) {
    parent_pass->aggregation().has_damage_from_contributing_content = true;
  }

  if (!damage_rect.IsEmpty()) {
    auto damage_rect_surface_space = damage_rect;
    if (IsRootSurface(surface)) {
      // The damage reported to the surface is in pre-display transform space
      // since it is used by clients which are not aware of the display
      // transform.
      damage_rect = cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          root_surface_transform_, damage_rect);
      gfx::Transform inverse(gfx::Transform::kSkipInitialization);
      bool inverted = root_surface_transform_.GetInverse(&inverse);
      DCHECK(inverted);
      damage_rect_surface_space =
          cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(inverse,
                                                                  damage_rect);
    }

    // The following call can cause one or more copy requests to be added to the
    // Surface. Therefore, no code before this point should have assumed
    // anything about the presence or absence of copy requests after this point.
    surface->NotifyAggregatedDamage(damage_rect_surface_space,
                                    expected_display_time_);
  }

  // If any CopyOutputRequests were made at FrameSink level, make sure we grab
  // them too.
  surface->TakeCopyOutputRequestsFromClient();
  if (surface->IsVideoCaptureOnFromClient())
    result.video_capture_enabled = true;

  if (de_jelly_enabled_ && surface->HasUndrawnActiveFrame())
    new_surfaces_.insert(surface->surface_id());

  if (will_draw)
    surface->OnWillBeDrawn();

  const CompositorFrame& frame = surface->GetActiveOrInterpolatedFrame();
  for (const SurfaceRange& surface_range : frame.metadata.referenced_surfaces) {
    damage_ranges_[surface_range.end().frame_sink_id()].push_back(
        surface_range);
    if (surface_range.HasDifferentFrameSinkIds()) {
      damage_ranges_[surface_range.start()->frame_sink_id()].push_back(
          surface_range);
    }
  }

  for (const SurfaceId& surface_id : surface->active_referenced_surfaces()) {
    if (!contained_surfaces_.count(surface_id)) {
      result.undrawn_surfaces.insert(surface_id);
      ResolvedFrameData* undrawn_surface = GetResolvedFrame(surface_id);
      if (undrawn_surface) {
        PrewalkSurface(*undrawn_surface, /*parent_pass=*/nullptr,
                       /*will_draw=*/false, gfx::Rect(), result);
      }
    }
  }

  for (auto& resolved_pass : resolved_frame.GetResolvedPasses()) {
    auto& render_pass = resolved_pass.render_pass();

    // Checking for copy requests need to be done after the prewalk because
    // copy requests can get added after damage is computed.
    if (!render_pass.copy_requests.empty()) {
      has_copy_requests_ = true;
      MarkAndPropagateCopyRequestPasses(resolved_pass);
    }
  }

  referenced_surfaces_.erase(surface->surface_id());
  if (!damage_rect.IsEmpty() && frame.metadata.may_contain_video)
    result.may_contain_video = true;
  result.content_color_usage =
      std::max(result.content_color_usage, frame.metadata.content_color_usage);

  return damage_rect;
}

void SurfaceAggregator::CopyUndrawnSurfaces(PrewalkResult* prewalk_result) {
  // undrawn_surfaces are Surfaces that were identified by prewalk as being
  // referenced by a drawn Surface, but aren't contained in a SurfaceDrawQuad.
  // They need to be iterated over to ensure that any copy requests on them
  // (or on Surfaces they reference) are executed.
  std::vector<SurfaceId> surfaces_to_copy(
      prewalk_result->undrawn_surfaces.begin(),
      prewalk_result->undrawn_surfaces.end());
  DCHECK(referenced_surfaces_.empty());

  for (size_t i = 0; i < surfaces_to_copy.size(); i++) {
    SurfaceId surface_id = surfaces_to_copy[i];
    const ResolvedFrameData* resolved_frame = GetResolvedFrame(surface_id);
    if (!resolved_frame)
      continue;

    Surface* surface = resolved_frame->surface();
    if (!surface->HasCopyOutputRequests()) {
      // Children are not necessarily included in undrawn_surfaces (because
      // they weren't referenced directly from a drawn surface), but may have
      // copy requests, so make sure to check them as well.
      for (const SurfaceId& child_id : surface->active_referenced_surfaces()) {
        // Don't iterate over the child Surface if it was already listed as a
        // child of a different Surface, or in the case where there's infinite
        // recursion.
        if (!prewalk_result->undrawn_surfaces.count(child_id)) {
          surfaces_to_copy.push_back(child_id);
          prewalk_result->undrawn_surfaces.insert(child_id);
        }
      }
    } else {
      prewalk_result->undrawn_surfaces.erase(surface_id);
      referenced_surfaces_.insert(surface_id);
      CopyPasses(*resolved_frame);
      referenced_surfaces_.erase(surface_id);
    }
  }
}

void SurfaceAggregator::MarkAndPropagateCopyRequestPasses(
    ResolvedPassData& resolved_pass) {
  if (resolved_pass.aggregation().in_copy_request_pass)
    return;

  resolved_pass.aggregation().in_copy_request_pass = true;
  for (auto* child_pass : resolved_pass.aggregation().embedded_passes) {
    MarkAndPropagateCopyRequestPasses(*child_pass);
  }
}

AggregatedFrame SurfaceAggregator::Aggregate(
    const SurfaceId& surface_id,
    base::TimeTicks expected_display_time,
    gfx::OverlayTransform display_transform,
    const gfx::Rect& target_damage,
    int64_t display_trace_id) {
  DCHECK(!expected_display_time.is_null());

  root_surface_id_ = surface_id;
  Surface* surface = manager_->GetSurfaceForId(surface_id);
  DCHECK(surface);
  DCHECK(contained_surfaces_.empty());

  CheckFrameSinksChanged(surface);

  if (!surface->HasActiveFrame())
    return {};

  // Start recording new stats for this aggregation.
  stats_.emplace();

  display_trace_id_ = display_trace_id;
  expected_display_time_ = expected_display_time;

  const CompositorFrame& root_surface_frame =
      surface->GetActiveOrInterpolatedFrame();
  TRACE_EVENT_WITH_FLOW2(
      "viz,benchmark", "Graphics.Pipeline",
      TRACE_ID_GLOBAL(root_surface_frame.metadata.begin_frame_ack.trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SurfaceAggregation", "display_trace", display_trace_id_);

  AggregatedFrame frame;
  frame.top_controls_visible_height =
      root_surface_frame.metadata.top_controls_visible_height;

  dest_pass_list_ = &frame.render_pass_list;
  surface_damage_rect_list_ = &frame.surface_damage_rect_list_;

  auto& root_render_pass = root_surface_frame.render_pass_list.back();

  // The root render pass on the root surface can not have backdrop filters.
  DCHECK(!root_render_pass->backdrop_filters.HasFilterThatMovesPixels());

  const gfx::Size viewport_bounds = root_render_pass->output_rect.size();
  root_surface_transform_ = gfx::OverlayTransformToTransform(
      display_transform, gfx::SizeF(viewport_bounds));

  // Reset state that couldn't be reset in ResetAfterAggregate().
  damage_ranges_.clear();

  DCHECK(referenced_surfaces_.empty());

  base::ElapsedTimer prewalk_timer;
  PrewalkResult prewalk_result;
  // Get the resolved frame for the root surface after `prewalk_timer` is
  // started so the work is counted in the same histograms as before.
  ResolvedFrameData& resolved_frame =
      *GetResolvedFrame(surface, /*inside_aggregation=*/true);
  gfx::Rect prewalk_damage_rect = PrewalkSurface(
      resolved_frame,
      /*parent_pass=*/nullptr,
      /*will_draw=*/true, /*damage_from_parent=*/gfx::Rect(), prewalk_result);
  stats_->prewalk_time = prewalk_timer.Elapsed();

  root_damage_rect_ = prewalk_damage_rect;
  // |root_damage_rect_| is used to restrict aggregating quads only if they
  // intersect this area.
  root_damage_rect_.Union(target_damage);

  // Changing color usage will cause the renderer to reshape the output surface,
  // therefore the renderer might expand the damage to the whole frame. The
  // following makes sure SA will produce all the quads to cover the full frame.
  bool color_usage_changed =
      root_content_color_usage_ != prewalk_result.content_color_usage;
  if (color_usage_changed) {
    root_damage_rect_ = cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
        root_surface_transform_,
        gfx::Rect(root_surface_frame.size_in_pixels()));
    root_content_color_usage_ = prewalk_result.content_color_usage;
  }

  if (prewalk_result.frame_sinks_changed)
    manager_->AggregatedFrameSinksChanged();

  frame.has_copy_requests = has_copy_requests_;
  frame.video_capture_enabled = prewalk_result.video_capture_enabled;
  frame.may_contain_video = prewalk_result.may_contain_video;
  frame.content_color_usage = prewalk_result.content_color_usage;

  base::ElapsedTimer copy_timer;
  CopyUndrawnSurfaces(&prewalk_result);
  referenced_surfaces_.insert(surface_id);
  CopyPasses(resolved_frame);
  referenced_surfaces_.erase(surface_id);
  DCHECK(referenced_surfaces_.empty());
  stats_->copy_time = copy_timer.Elapsed();

  RecordStatHistograms();

  if (dest_pass_list_->empty()) {
    ResetAfterAggregate();
    return {};
  }

  // The root render pass damage might have been expanded by target_damage (the
  // area that might need to be recomposited on the target surface). We restrict
  // the damage_rect of the root render pass to the one caused by the source
  // surfaces, except when drawing delegated ink trails.
  // The damage on the root render pass should not include the expanded area
  // since Renderer and OverlayProcessor expect the non expanded damage. The
  // only exception is when delegated ink trails are being drawn, in which case
  // the root render pass needs to contain the expanded area, as |target_damage|
  // also reflects the delegated ink trail damage rect.
  auto* last_pass = dest_pass_list_->back().get();

  if (!color_usage_changed && !last_frame_had_delegated_ink_ &&
      !RenderPassNeedsFullDamage(resolved_frame.GetRootRenderPassData())) {
    last_pass->damage_rect.Intersect(prewalk_damage_rect);
  }

  // Now that we've handled our main surface aggregation, apply de-jelly effect
  // if enabled.
  if (de_jelly_enabled_)
    HandleDeJelly(surface);

  AddColorConversionPass();

  ProcessAddedAndRemovedSurfaces();
  contained_surfaces_.swap(previous_contained_surfaces_);
  contained_frame_sinks_.swap(previous_contained_frame_sinks_);

  ResetAfterAggregate();

  for (auto it : previous_contained_surfaces_) {
    surface = manager_->GetSurfaceForId(it.first);
    if (surface) {
      surface->allocation_group()->TakeAggregatedLatencyInfoUpTo(
          surface, &frame.latency_info);
    }
    if (!ui::LatencyInfo::Verify(frame.latency_info,
                                 "SurfaceAggregator::Aggregate")) {
      break;
    }
  }

  if (delegated_ink_metadata_) {
    frame.delegated_ink_metadata = std::move(delegated_ink_metadata_);
    last_frame_had_delegated_ink_ = true;
  } else {
    last_frame_had_delegated_ink_ = false;
  }

  if (frame_annotator_)
    frame_annotator_->AnnotateAggregatedFrame(&frame);

  return frame;
}

void SurfaceAggregator::RecordStatHistograms() {
  UMA_HISTOGRAM_COUNTS_100(
      "Compositing.SurfaceAggregator.PrewalkedSurfaceCount",
      stats_->prewalked_surface_count);
  UMA_HISTOGRAM_COUNTS_100("Compositing.SurfaceAggregator.CopiedSurfaceCount",
                           stats_->copied_surface_count);
  UMA_HISTOGRAM_COUNTS_1000(
      "Compositing.SurfaceAggregator.DeclareResourceCount",
      stats_->declare_resources_count);

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Compositing.SurfaceAggregator.PrewalkUs", stats_->prewalk_time,
      kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Compositing.SurfaceAggregator.CopyUs", stats_->copy_time,
      kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Compositing.SurfaceAggregator.DeclareResourcesUs",
      stats_->declare_resources_time, kHistogramMinTime, kHistogramMaxTime,
      kHistogramTimeBuckets);

  stats_.reset();
}

void SurfaceAggregator::ResetAfterAggregate() {
  dest_pass_list_ = nullptr;
  surface_damage_rect_list_ = nullptr;
  current_zero_damage_rect_is_not_recorded_ = false;
  expected_display_time_ = base::TimeTicks();
  display_trace_id_ = -1;
  has_pixel_moving_backdrop_filter_ = false;
  has_copy_requests_ = false;
  new_surfaces_.clear();
  resolved_surface_ranges_.clear();
  contained_surfaces_.clear();
  contained_frame_sinks_.clear();

  // Delete resolved frame data that wasn't used this frame.
  base::EraseIf(resolved_frames_, [](auto& entry) {
    return !entry.second.CheckIfUsedAndReset();
  });
}

void SurfaceAggregator::ReleaseResources(const SurfaceId& surface_id) {
  auto it = surface_id_to_resource_child_id_.find(surface_id);
  if (it != surface_id_to_resource_child_id_.end()) {
    provider_->DestroyChild(it->second);
    surface_id_to_resource_child_id_.erase(it);
  }
}

void SurfaceAggregator::SetFullDamageForSurface(const SurfaceId& surface_id) {
  auto it = previous_contained_surfaces_.find(surface_id);
  if (it == previous_contained_surfaces_.end())
    return;
  // Set the last drawn index as 0 to ensure full damage next time it's drawn.
  it->second = 0;
}

void SurfaceAggregator::SetDisplayColorSpaces(
    const gfx::DisplayColorSpaces& display_color_spaces) {
  display_color_spaces_ = display_color_spaces;
}

void SurfaceAggregator::SetMaxRenderTargetSize(int max_size) {
  DCHECK_GE(max_size, 0);
  max_render_target_size_ = max_size;
}

bool SurfaceAggregator::NotifySurfaceDamageAndCheckForDisplayDamage(
    const SurfaceId& surface_id) {
  if (previous_contained_surfaces_.count(surface_id)) {
    Surface* surface = manager_->GetSurfaceForId(surface_id);
    if (surface) {
      DCHECK(surface->HasActiveFrame());
      if (surface->GetActiveOrInterpolatedFrame().resource_list.empty())
        ReleaseResources(surface_id);
    }
    return true;
  }

  auto it = damage_ranges_.find(surface_id.frame_sink_id());
  if (it == damage_ranges_.end())
    return false;

  for (const SurfaceRange& surface_range : it->second) {
    if (surface_range.IsInRangeInclusive(surface_id))
      return true;
  }

  return false;
}

bool SurfaceAggregator::HasFrameAnnotator() const {
  return !!frame_annotator_;
}

void SurfaceAggregator::SetFrameAnnotator(
    std::unique_ptr<FrameAnnotator> frame_annotator) {
  DCHECK(!frame_annotator_);
  frame_annotator_ = std::move(frame_annotator);
}

void SurfaceAggregator::DestroyFrameAnnotator() {
  DCHECK(frame_annotator_);
  frame_annotator_.reset();
}

bool SurfaceAggregator::IsRootSurface(const Surface* surface) const {
  return surface->surface_id() == root_surface_id_;
}

// Transform the point and presentation area of the metadata to be in the root
// target space. They need to be in the root target space because they will
// eventually be drawn directly onto the buffer just before being swapped onto
// the screen, so root target space is required so that they are positioned
// correctly. After transforming, they are stored in the
// |delegated_ink_metadata_| member in order to be placed on the final
// aggregated frame, after which the member is then cleared.
void SurfaceAggregator::TransformAndStoreDelegatedInkMetadata(
    const gfx::Transform& parent_quad_to_root_target_transform,
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  if (delegated_ink_metadata_) {
    // This member could already be populated in two scenarios:
    //   1. The delegated ink metadata was committed to a frame's metadata that
    //      wasn't ultimately used to produce a frame, but is now being used.
    //   2. There are two or more ink strokes requesting a delegated ink trail
    //      simultaneously.
    // In both cases, we want to default to using a "last write wins" strategy
    // to determine the metadata to put on the final aggregated frame. This
    // avoids potential issues of using stale ink metadata in the first scenario
    // by always using the newest one. For the second scenario, it would be a
    // very niche use case to have more than one at a time, so the explainer
    // specifies using last write wins to decide.
    base::TimeTicks stored_time = delegated_ink_metadata_->timestamp();
    base::TimeTicks new_time = metadata->timestamp();
    if (new_time < stored_time)
      return;
  }

  gfx::PointF point(metadata->point());
  gfx::RectF area(metadata->presentation_area());
  parent_quad_to_root_target_transform.TransformPoint(&point);
  parent_quad_to_root_target_transform.TransformRect(&area);
  delegated_ink_metadata_ = std::make_unique<gfx::DelegatedInkMetadata>(
      point, metadata->diameter(), metadata->color(), metadata->timestamp(),
      area, metadata->frame_time(), metadata->is_hovering());

  TRACE_EVENT_INSTANT2(
      "viz", "SurfaceAggregator::TransformAndStoreDelegatedInkMetadata",
      TRACE_EVENT_SCOPE_THREAD, "original metadata", metadata->ToString(),
      "transformed metadata", delegated_ink_metadata_->ToString());
}

void SurfaceAggregator::HandleDeJelly(Surface* surface) {
  TRACE_EVENT0("viz", "SurfaceAggregator::HandleDeJelly");

  if (!DeJellyActive()) {
    SetLastFrameHadJelly(false);
    return;
  }

  // |jelly_clip| is the rect that contains all de-jelly'd quads. It is used as
  // an approximation for the containing non-skewed clip rect.
  gfx::Rect jelly_clip;
  // |max_skew| represents the maximum skew applied to an element. To prevent
  // tearing due to slight inaccuracies, we apply the max skew to all skewed
  // elements.
  float max_skew = 0.0f;

  // Iterate over each SharedQuadState in the root render pass and compute
  // |max_skew| and |jelly_clip|.
  auto* root_render_pass = dest_pass_list_->back().get();
  float screen_width = DeJellyScreenWidth();
  for (SharedQuadState* state : root_render_pass->shared_quad_state_list) {
    float delta_y = state->de_jelly_delta_y;
    if (delta_y == 0.0f)
      continue;

    // We are going to de-jelly this SharedQuadState. Expand the max clip.
    if (state->clip_rect) {
      jelly_clip.Union(*state->clip_rect);
    }

    // Compute the skew angle and update |max_skew|.
    float de_jelly_angle = gfx::RadToDeg(atan2(delta_y, screen_width));
    float sign = de_jelly_angle / std::abs(de_jelly_angle);
    max_skew = std::max(std::abs(de_jelly_angle), std::abs(max_skew)) * sign;
  }

  // Exit if nothing was skewed.
  if (max_skew == 0.0f) {
    SetLastFrameHadJelly(false);
    return;
  }

  SetLastFrameHadJelly(true);

  // Remove the existing root render pass and create a new one which we will
  // re-copy skewed quads / render-passes to.
  // TODO(ericrk): Handle backdrop filters?
  // TODO(ericrk): This will end up skewing copy requests. Address if
  // necessary.
  auto old_root = std::move(dest_pass_list_->back());
  dest_pass_list_->pop_back();
  auto new_root = root_render_pass->Copy(root_render_pass->id);
  new_root->copy_requests = std::move(old_root->copy_requests);

  // Data tracking the current sub RenderPass (if any) which is being appended
  // to. We can keep re-using a sub RenderPass if the skew has not changed and
  // if we are in the typical kSrcOver blend mode.
  std::unique_ptr<AggregatedRenderPass> sub_render_pass;
  SkBlendMode sub_render_pass_blend_mode;
  float sub_render_pass_opacity;

  // Apply de-jelly to all quads, promoting quads into render passes as
  // necessary.
  for (auto it = root_render_pass->quad_list.begin();
       it != root_render_pass->quad_list.end();) {
    auto* state = it->shared_quad_state;
    bool has_skew = state->de_jelly_delta_y != 0.0f;

    // If we have a sub RenderPass which is not compatible with our current
    // quad, we must flush and clear it.
    if (sub_render_pass) {
      if (!has_skew || sub_render_pass_blend_mode != state->blend_mode ||
          state->blend_mode != SkBlendMode::kSrcOver) {
        AppendDeJellyRenderPass(max_skew, jelly_clip, sub_render_pass_opacity,
                                sub_render_pass_blend_mode, new_root.get(),
                                std::move(sub_render_pass));
        sub_render_pass.reset();
      }
    }

    // Create a new render pass if we have a skewed quad which is clipped more
    // than jelly_clip.
    bool create_render_pass =
        has_skew && state->clip_rect && state->clip_rect != jelly_clip;
    if (!sub_render_pass && create_render_pass) {
      sub_render_pass = std::make_unique<AggregatedRenderPass>(1, 1);
      gfx::Transform skew_transform;
      skew_transform.Skew(0.0f, max_skew);
      // Ignore rectangles for now, these are updated in
      // CreateDeJellyRenderPassQuads.
      sub_render_pass->SetNew(render_pass_id_generator_.GenerateNextId(),
                              gfx::Rect(), gfx::Rect(), skew_transform);
      // If blend mode is not kSrcOver, we apply it in the render pass.
      if (state->blend_mode != SkBlendMode::kSrcOver) {
        sub_render_pass_opacity = state->opacity;
        sub_render_pass_blend_mode = state->blend_mode;
      } else {
        sub_render_pass_opacity = 1.0f;
        sub_render_pass_blend_mode = SkBlendMode::kSrcOver;
      }
    }

    if (sub_render_pass) {
      CreateDeJellyRenderPassQuads(&it, root_render_pass->quad_list.end(),
                                   jelly_clip, max_skew, sub_render_pass.get());
    } else {
      float skew = has_skew ? max_skew : 0.0f;
      CreateDeJellyNormalQuads(&it, root_render_pass->quad_list.end(),
                               new_root.get(), skew);
    }
  }
  if (sub_render_pass) {
    AppendDeJellyRenderPass(max_skew, jelly_clip, sub_render_pass_opacity,
                            sub_render_pass_blend_mode, new_root.get(),
                            std::move(sub_render_pass));
  }

  dest_pass_list_->push_back(std::move(new_root));
}

void SurfaceAggregator::CreateDeJellyRenderPassQuads(
    cc::ListContainer<DrawQuad>::Iterator* quad_iterator,
    const cc::ListContainer<DrawQuad>::Iterator& end,
    const gfx::Rect& jelly_clip,
    float skew,
    AggregatedRenderPass* render_pass) {
  auto* quad = **quad_iterator;
  const auto* state = quad->shared_quad_state;

  // Heuristic - we may have over-clipped a quad. If a quad is clipped by the
  // |jelly_clip|, but contains content beyond |jelly_clip|, un-clip the quad by
  // MaxDeJellyHeight().
  int un_clip_top = 0;
  int un_clip_bottom = 0;
  DCHECK(state->clip_rect);
  if (state->clip_rect->y() <= jelly_clip.y()) {
    un_clip_top = MaxDeJellyHeight();
  }
  if (state->clip_rect->bottom() >= jelly_clip.bottom()) {
    un_clip_bottom = MaxDeJellyHeight();
  }

  // Compute the required renderpass rect in target space.
  // First, find the un-transformed visible rect.
  gfx::RectF render_pass_visible_rect_f(state->visible_quad_layer_rect);
  // Next, if this is a RenderPass quad, find any filters and expand the
  // visible rect.
  if (quad->material == DrawQuad::Material::kCompositorRenderPass) {
    auto target_id = AggregatedRenderPassId(uint64_t{
        CompositorRenderPassDrawQuad::MaterialCast(quad)->render_pass_id});
    for (auto& rp : *dest_pass_list_) {
      if (rp->id == target_id) {
        render_pass_visible_rect_f = gfx::RectF(
            rp->filters.MapRect(state->visible_quad_layer_rect, SkMatrix()));
        break;
      }
    }
  }
  // Next, find the enclosing Rect for the transformed target space RectF.
  state->quad_to_target_transform.TransformRect(&render_pass_visible_rect_f);
  gfx::Rect render_pass_visible_rect =
      gfx::ToEnclosingRect(render_pass_visible_rect_f);
  // Finally, expand by our un_clip amounts.
  render_pass_visible_rect.Inset(0, -un_clip_top, 0, -un_clip_bottom);

  // Expand the |render_pass|'s rects.
  render_pass->output_rect =
      gfx::UnionRects(render_pass->output_rect, render_pass_visible_rect);
  render_pass->damage_rect = render_pass->output_rect;

  // Create a new SharedQuadState based on |state|.
  {
    auto* new_state = render_pass->CreateAndAppendSharedQuadState();
    *new_state = *state;
    // If blend mode is not kSrcOver, we apply it in the RenderPass.
    if (state->blend_mode != SkBlendMode::kSrcOver) {
      new_state->opacity = 1.0f;
      new_state->blend_mode = SkBlendMode::kSrcOver;
    }

    // Expand our clip by un clip amounts.
    new_state->clip_rect->Inset(0, -un_clip_top, 0, -un_clip_bottom);
  }

  // Append all quads sharing |new_state|.
  AppendDeJellyQuadsForSharedQuadState(quad_iterator, end, render_pass, state);
}

void SurfaceAggregator::CreateDeJellyNormalQuads(
    cc::ListContainer<DrawQuad>::Iterator* quad_iterator,
    const cc::ListContainer<DrawQuad>::Iterator& end,
    AggregatedRenderPass* root_pass,
    float skew) {
  auto* quad = **quad_iterator;
  const auto* state = quad->shared_quad_state;

  // Crearte a new SharedQuadState on |root_pass| and apply skew if any.
  SharedQuadState* new_state = root_pass->CreateAndAppendSharedQuadState();
  *new_state = *state;
  if (skew != 0.0f) {
    gfx::Transform skew_transform;
    skew_transform.Skew(0.0f, skew);
    new_state->quad_to_target_transform =
        skew_transform * new_state->quad_to_target_transform;
  }

  // Append all quads sharing |new_state|.
  AppendDeJellyQuadsForSharedQuadState(quad_iterator, end, root_pass, state);
}

void SurfaceAggregator::AppendDeJellyRenderPass(
    float skew,
    const gfx::Rect& jelly_clip,
    float opacity,
    SkBlendMode blend_mode,
    AggregatedRenderPass* root_pass,
    std::unique_ptr<AggregatedRenderPass> render_pass) {
  // Create a new quad for this renderpass and append it to the pass list.
  auto* new_state = root_pass->CreateAndAppendSharedQuadState();
  gfx::Transform transform;
  new_state->SetAll(transform, render_pass->output_rect,
                    render_pass->output_rect, gfx::MaskFilterInfo(), jelly_clip,
                    false, opacity, blend_mode, 0);
  auto* quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(new_state, render_pass->output_rect, render_pass->output_rect,
               render_pass->id, kInvalidResourceId, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(), gfx::PointF(),
               gfx::RectF(gfx::SizeF(render_pass->output_rect.size())), false,
               1.0f);
  gfx::Transform skew_transform;
  skew_transform.Skew(0.0f, skew);
  new_state->quad_to_target_transform =
      skew_transform * new_state->quad_to_target_transform;
  dest_pass_list_->push_back(std::move(render_pass));
}

void SurfaceAggregator::AppendDeJellyQuadsForSharedQuadState(
    cc::ListContainer<DrawQuad>::Iterator* quad_iterator,
    const cc::ListContainer<DrawQuad>::Iterator& end,
    AggregatedRenderPass* render_pass,
    const SharedQuadState* state) {
  auto* quad = **quad_iterator;
  while (quad->shared_quad_state == state) {
    // Since we're dealing with post-aggregated passes, we should not have any
    // RenderPassDrawQuads.
    DCHECK_NE(quad->material, DrawQuad::Material::kCompositorRenderPass);
    if (quad->material == DrawQuad::Material::kAggregatedRenderPass) {
      const auto* pass_quad = AggregatedRenderPassDrawQuad::MaterialCast(quad);
      render_pass->CopyFromAndAppendRenderPassDrawQuad(pass_quad);
    } else {
      render_pass->CopyFromAndAppendDrawQuad(quad);
    }

    ++(*quad_iterator);
    if (*quad_iterator == end)
      break;
    quad = **quad_iterator;
  }
}

void SurfaceAggregator::SetLastFrameHadJelly(bool had_jelly) {
  // If we've just rendererd a jelly-free frame after one with jelly, we must
  // damage the entire surface, as we may have removed jelly from an otherwise
  // unchanged quad.
  if (last_frame_had_jelly_ && !had_jelly) {
    auto* root_pass = dest_pass_list_->back().get();
    root_pass->damage_rect = root_pass->output_rect;
  }
  last_frame_had_jelly_ = had_jelly;
}

void SurfaceAggregator::DebugLogSurface(const Surface* surface,
                                        bool will_draw) {
  DBG_LOG("aggregator.surface.log", "D%d - %s, %s draws=%s",
          static_cast<int>(referenced_surfaces_.size()),
          surface->surface_id().ToString().c_str(),
          surface->size_in_pixels().ToString().c_str(),
          will_draw ? "true" : "false");
}

}  // namespace viz
