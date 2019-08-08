// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/surface_aggregator.h"

#include <stddef.h>

#include <map>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/renderer_utils.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_allocation_group.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "ui/gfx/overlay_transform_utils.h"

namespace viz {
namespace {

// Used for determine when to treat opacity close to 1.f as opaque. The value is
// chosen to be smaller than 1/255.
constexpr float kOpacityEpsilon = 0.001f;

void MoveMatchingRequests(
    RenderPassId render_pass_id,
    std::multimap<RenderPassId, std::unique_ptr<CopyOutputRequest>>*
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

}  // namespace

std::string SurfaceAggregator::ClipData::ToString() const {
  return is_clipped ? "clip " + rect.ToString() : "no clip";
}

SurfaceAggregator::SurfaceAggregator(SurfaceManager* manager,
                                     DisplayResourceProvider* provider,
                                     bool aggregate_only_damaged,
                                     bool needs_surface_occluding_damage_rect)
    : manager_(manager),
      provider_(provider),
      next_render_pass_id_(1),
      aggregate_only_damaged_(aggregate_only_damaged),
      needs_surface_occluding_damage_rect_(needs_surface_occluding_damage_rect),
      weak_factory_(this) {
  DCHECK(manager_);
}

SurfaceAggregator::~SurfaceAggregator() {
  // Notify client of all surfaces being removed.
  contained_surfaces_.clear();
  contained_frame_sinks_.clear();
  ProcessAddedAndRemovedSurfaces();
}

SurfaceAggregator::PrewalkResult::PrewalkResult() {}

SurfaceAggregator::PrewalkResult::~PrewalkResult() {}

// Create a clip rect for an aggregated quad from the original clip rect and
// the clip rect from the surface it's on.
SurfaceAggregator::ClipData SurfaceAggregator::CalculateClipRect(
    const ClipData& surface_clip,
    const ClipData& quad_clip,
    const gfx::Transform& target_transform) {
  ClipData out_clip;
  if (surface_clip.is_clipped)
    out_clip = surface_clip;

  if (quad_clip.is_clipped) {
    // TODO(jamesr): This only works if target_transform maps integer
    // rects to integer rects.
    gfx::Rect final_clip =
        cc::MathUtil::MapEnclosingClippedRect(target_transform, quad_clip.rect);
    if (out_clip.is_clipped)
      out_clip.rect.Intersect(final_clip);
    else
      out_clip.rect = final_clip;
    out_clip.is_clipped = true;
  }

  return out_clip;
}

RenderPassId SurfaceAggregator::RemapPassId(RenderPassId surface_local_pass_id,
                                            const SurfaceId& surface_id) {
  auto key = std::make_pair(surface_id, surface_local_pass_id);
  auto it = render_pass_allocator_map_.find(key);
  if (it != render_pass_allocator_map_.end()) {
    it->second.in_use = true;
    return it->second.id;
  }

  RenderPassInfo render_pass_info;
  render_pass_info.id = next_render_pass_id_++;
  render_pass_allocator_map_[key] = render_pass_info;
  return render_pass_info.id;
}

int SurfaceAggregator::ChildIdForSurface(Surface* surface) {
  auto it = surface_id_to_resource_child_id_.find(surface->surface_id());
  if (it == surface_id_to_resource_child_id_.end()) {
    int child_id = provider_->CreateChild(
        base::BindRepeating(&SurfaceAggregator::UnrefResources,
                            surface->client()),
        surface->needs_sync_tokens());
    surface_id_to_resource_child_id_[surface->surface_id()] = child_id;
    return child_id;
  } else {
    return it->second;
  }
}

bool SurfaceAggregator::IsSurfaceFrameIndexSameAsPrevious(
    const Surface* surface) const {
  auto it = previous_contained_surfaces_.find(surface->surface_id());
  if (it != previous_contained_surfaces_.end()) {
    uint64_t previous_index = it->second;
    if (previous_index == surface->GetActiveFrameIndex())
      return true;
  }
  return false;
}

gfx::Rect SurfaceAggregator::DamageRectForSurface(
    const Surface* surface,
    const RenderPass& source,
    const gfx::Rect& full_rect) const {
  if (IsSurfaceFrameIndexSameAsPrevious(surface))
    return gfx::Rect();

  auto it = previous_contained_surfaces_.find(surface->surface_id());
  const SurfaceId& previous_surface_id = surface->previous_frame_surface_id();

  if (surface->surface_id() != previous_surface_id) {
    it = previous_contained_surfaces_.find(previous_surface_id);
  }
  if (it != previous_contained_surfaces_.end()) {
    uint64_t previous_index = it->second;
    if (previous_index == surface->GetActiveFrameIndex() - 1)
      return source.damage_rect;
  }

  return full_rect;
}

gfx::Rect SurfaceAggregator::CalculateOccludingSurfaceDamageRect(
    const DrawQuad* quad,
    const gfx::Transform& parent_quad_to_root_target_transform) {
  if (damage_rects_union_of_surfaces_on_top_.IsEmpty())
    return gfx::Rect();

  // Transform the quad to the parent root target space
  // Note: this quad is on the child root render pass.
  gfx::Transform transform(parent_quad_to_root_target_transform,
                           quad->shared_quad_state->quad_to_target_transform);
  gfx::Rect surface_in_root_target_space =
      cc::MathUtil::MapEnclosingClippedRect(transform, quad->visible_rect);

  // damage_rects_union_of_surfaces_on_top_ is already in the parent root target
  // space.
  gfx::Rect occluding_damage_rect = damage_rects_union_of_surfaces_on_top_;
  occluding_damage_rect.Intersect(surface_in_root_target_space);

  return occluding_damage_rect;
}

// In CopyPasses(), surfaces are processed from top to bottom. Therefore, all
// surfaces on top has been added to damage_rects_union_of_surfaces_on_top_
// before this.
void SurfaceAggregator::UnionSurfaceDamageRectsOnTop(
    const gfx::Rect& surface_rect,
    const gfx::Transform& quad_to_root_target_transform,
    const RenderPass* render_pass) {
  DCHECK(!surface_rect.IsEmpty());

  gfx::Rect damage_rect_in_root_target_space =
      cc::MathUtil::MapEnclosingClippedRect(quad_to_root_target_transform,
                                            surface_rect);
  damage_rects_union_of_surfaces_on_top_.Union(
      damage_rect_in_root_target_space);
}

// This is for underlay video power optimization.
// The purpose of this function is to calculate the occluding damage rect if
// there are elements on top of underlay. This damage rect is later saved in
// shared_quad_state->occluding_damage_rect and used by the overlay
// processor for damage rect optimization. This function is called once
// for each surface. It adds the damage rects of all surfaces to
// damage_rects_union_of_surfaces_on_top_. The occluding damage rect
// is then calculated based on this rect.
bool SurfaceAggregator::ProcessSurfaceOccludingDamage(
    const Surface* surface,
    const RenderPassList& render_pass_list,
    const gfx::Transform& parent_target_transform,
    const RenderPass* dest_pass,
    gfx::Rect* occluding_damage_rect) {
  if (!needs_surface_occluding_damage_rect_)
    return false;

  bool occluding_damage_rect_valid = false;
  RenderPass* last_render_pass = render_pass_list.back().get();
  gfx::Transform quad_to_root_target_transform = gfx::Transform(
      dest_pass->transform_to_root_target, parent_target_transform);

  // This occluding damage detection only works when there is only one quad
  // in the current surface.
  if (render_pass_list.size() == 1 && last_render_pass->quad_list.size() == 1) {
    auto* quad = last_render_pass->quad_list.back();
    *occluding_damage_rect = CalculateOccludingSurfaceDamageRect(
        quad, quad_to_root_target_transform);
    occluding_damage_rect_valid = true;
  }

  gfx::Rect surface_damage_rect;
  if (RenderPassNeedsFullDamage(dest_pass)) {
    surface_damage_rect = last_render_pass->output_rect;
  } else {
    surface_damage_rect = DamageRectForSurface(surface, *last_render_pass,
                                               last_render_pass->output_rect);
  }

  // Add the current surface to the damage rect union if there is any damage.
  // This should be done AFTER checking the occluding damage because the surface
  // on top should not include its own surface.
  if (!surface_damage_rect.IsEmpty()) {
    UnionSurfaceDamageRectsOnTop(
        surface_damage_rect, quad_to_root_target_transform, last_render_pass);
  }
  return occluding_damage_rect_valid;
}

bool SurfaceAggregator::RenderPassNeedsFullDamage(
    const RenderPass* pass) const {
  if (copy_request_passes_.count(pass->id) || pass->cache_render_pass ||
      moved_pixel_passes_.count(pass->id)) {
    return true;
  } else {
    return false;
  }
}

// static
void SurfaceAggregator::UnrefResources(
    base::WeakPtr<SurfaceClient> surface_client,
    const std::vector<ReturnedResource>& resources) {
  if (surface_client)
    surface_client->UnrefResources(resources);
}

void SurfaceAggregator::HandleSurfaceQuad(
    const SurfaceDrawQuad* surface_quad,
    float parent_device_scale_factor,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    RenderPass* dest_pass,
    bool ignore_undamaged,
    gfx::Rect* damage_rect_in_quad_space,
    bool* damage_rect_in_quad_space_valid,
    const RoundedCornerInfo& rounded_corner_info) {
  SurfaceId primary_surface_id = surface_quad->surface_range.end();

  // If there's no fallback surface ID available, then simply emit a
  // SolidColorDrawQuad with the provided default background color. This
  // can happen after a Viz process crash.
  Surface* latest_surface =
      manager_->GetLatestInFlightSurface(surface_quad->surface_range);
  if (!latest_surface || !latest_surface->HasActiveFrame()) {
    EmitDefaultBackgroundColorQuad(surface_quad, target_transform, clip_rect,
                                   dest_pass, rounded_corner_info);
    return;
  }

  if (latest_surface->surface_id() != primary_surface_id &&
      !surface_quad->stretch_content_to_fill_bounds) {
    const CompositorFrame& fallback_frame = latest_surface->GetActiveFrame();

    gfx::Rect fallback_rect(latest_surface->GetActiveFrame().size_in_pixels());

    float scale_ratio =
        parent_device_scale_factor / fallback_frame.device_scale_factor();
    fallback_rect =
        gfx::ScaleToEnclosingRect(fallback_rect, scale_ratio, scale_ratio);
    fallback_rect = gfx::IntersectRects(fallback_rect, surface_quad->rect);

    EmitGutterQuadsIfNecessary(surface_quad->rect, fallback_rect,
                               surface_quad->shared_quad_state,
                               target_transform, clip_rect,
                               fallback_frame.metadata.root_background_color,
                               dest_pass, rounded_corner_info);
  }

  EmitSurfaceContent(latest_surface, parent_device_scale_factor,
                     surface_quad->shared_quad_state, surface_quad->rect,
                     surface_quad->visible_rect, target_transform, clip_rect,
                     surface_quad->stretch_content_to_fill_bounds, dest_pass,
                     ignore_undamaged, damage_rect_in_quad_space,
                     damage_rect_in_quad_space_valid, rounded_corner_info,
                     surface_quad->is_reflection);
}

void SurfaceAggregator::EmitSurfaceContent(
    Surface* surface,
    float parent_device_scale_factor,
    const SharedQuadState* source_sqs,
    const gfx::Rect& source_rect,
    const gfx::Rect& source_visible_rect,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    bool stretch_content_to_fill_bounds,
    RenderPass* dest_pass,
    bool ignore_undamaged,
    gfx::Rect* damage_rect_in_quad_space,
    bool* damage_rect_in_quad_space_valid,
    const RoundedCornerInfo& rounded_corner_info,
    bool is_reflection) {
  // If this surface's id is already in our referenced set then it creates
  // a cycle in the graph and should be dropped.
  SurfaceId surface_id = surface->surface_id();
  if (referenced_surfaces_.count(surface_id))
    return;

  float layer_to_content_scale_x, layer_to_content_scale_y;
  if (stretch_content_to_fill_bounds) {
    // Stretches the surface contents to exactly fill the layer bounds,
    // regardless of scale or aspect ratio differences.
    layer_to_content_scale_x =
        static_cast<float>(surface->GetActiveFrame().size_in_pixels().width()) /
        source_rect.width();
    layer_to_content_scale_y =
        static_cast<float>(
            surface->GetActiveFrame().size_in_pixels().height()) /
        source_rect.height();
  } else {
    layer_to_content_scale_x = layer_to_content_scale_y =
        surface->GetActiveFrame().device_scale_factor() /
        parent_device_scale_factor;
  }

  gfx::Transform scaled_quad_to_target_transform(
      source_sqs->quad_to_target_transform);
  scaled_quad_to_target_transform.Scale(SK_MScalar1 / layer_to_content_scale_x,
                                        SK_MScalar1 / layer_to_content_scale_y);

  const CompositorFrame& frame = surface->GetActiveFrame();
  TRACE_EVENT_WITH_FLOW2(
      "viz,benchmark", "Graphics.Pipeline",
      TRACE_ID_GLOBAL(frame.metadata.begin_frame_ack.trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SurfaceAggregation", "display_trace", display_trace_id_);

  if (ignore_undamaged) {
    gfx::Transform quad_to_target_transform(
        target_transform, source_sqs->quad_to_target_transform);
    *damage_rect_in_quad_space_valid = CalculateQuadSpaceDamageRect(
        quad_to_target_transform, dest_pass->transform_to_root_target,
        root_damage_rect_, damage_rect_in_quad_space);
    if (*damage_rect_in_quad_space_valid &&
        !damage_rect_in_quad_space->Intersects(source_visible_rect)) {
      return;
    }
  }

  // A map keyed by RenderPass id.
  Surface::CopyRequestsMap copy_requests;
  surface->TakeCopyOutputRequests(&copy_requests);

  const RenderPassList& render_pass_list = frame.render_pass_list;
  if (!valid_surfaces_.count(surface_id)) {
    // As |copy_requests| goes out-of-scope, all copy requests in that container
    // will auto-send an empty result upon destruction.
    return;
  }

  referenced_surfaces_.insert(surface_id);
  // TODO(vmpstr): provider check is a hack for unittests that don't set up a
  // resource provider.
  std::unordered_map<ResourceId, ResourceId> empty_map;
  const auto& child_to_parent_map =
      provider_ ? provider_->GetChildToParentMap(ChildIdForSurface(surface))
                : empty_map;
  gfx::Transform combined_transform = scaled_quad_to_target_transform;
  combined_transform.ConcatTransform(target_transform);

  // If the SurfaceDrawQuad is marked as being reflected and surface contents
  // are going to be scaled then keep the RenderPass. This allows the reflected
  // surface to be drawn with AA enabled for smooth scaling and preserves the
  // original reflector scaling behaviour which scaled a TextureLayer.
  bool reflected_and_scaled =
      is_reflection &&
      !scaled_quad_to_target_transform.IsIdentityOrTranslation();

  bool merge_pass =
      !reflected_and_scaled &&
      base::IsApproximatelyEqual(source_sqs->opacity, 1.f, kOpacityEpsilon) &&
      copy_requests.empty() && combined_transform.Preserves2dAxisAlignment() &&
      CanMergeRoundedCorner(rounded_corner_info, *render_pass_list.back());

  gfx::Rect occluding_damage_rect;
  bool occluding_damage_rect_valid = ProcessSurfaceOccludingDamage(
      surface, render_pass_list, combined_transform, dest_pass,
      &occluding_damage_rect);

  const RenderPassList& referenced_passes = render_pass_list;
  // TODO(fsamuel): Move this to a separate helper function.
  size_t passes_to_copy =
      merge_pass ? referenced_passes.size() - 1 : referenced_passes.size();
  for (size_t j = 0; j < passes_to_copy; ++j) {
    const RenderPass& source = *referenced_passes[j];

    size_t sqs_size = source.shared_quad_state_list.size();
    size_t dq_size = source.quad_list.size();
    std::unique_ptr<RenderPass> copy_pass(
        RenderPass::Create(sqs_size, dq_size));

    RenderPassId remapped_pass_id = RemapPassId(source.id, surface_id);

    copy_pass->SetAll(remapped_pass_id, source.output_rect, source.output_rect,
                      source.transform_to_root_target, source.filters,
                      source.backdrop_filters, source.backdrop_filter_bounds,
                      blending_color_space_, source.has_transparent_background,
                      source.cache_render_pass,
                      source.has_damage_from_contributing_content,
                      source.generate_mipmap);

    MoveMatchingRequests(source.id, &copy_requests, &copy_pass->copy_requests);

    // Contributing passes aggregated in to the pass list need to take the
    // transform of the surface quad into account to update their transform to
    // the root surface.
    copy_pass->transform_to_root_target.ConcatTransform(
        scaled_quad_to_target_transform);
    copy_pass->transform_to_root_target.ConcatTransform(target_transform);
    copy_pass->transform_to_root_target.ConcatTransform(
        dest_pass->transform_to_root_target);

    CopyQuadsToPass(source.quad_list, source.shared_quad_state_list,
                    surface->GetActiveFrame().device_scale_factor(),
                    child_to_parent_map, gfx::Transform(), ClipData(),
                    copy_pass.get(), surface_id, RoundedCornerInfo(),
                    occluding_damage_rect, occluding_damage_rect_valid);

    // If the render pass has copy requests, or should be cached, or has
    // moving-pixel filters, or in a moving-pixel surface, we should damage the
    // whole output rect so that we always drawn the full content. Otherwise, we
    // might have incompleted copy request, or cached patially drawn render
    // pass.
    if (!RenderPassNeedsFullDamage(copy_pass.get())) {
      gfx::Transform inverse_transform(gfx::Transform::kSkipInitialization);
      if (copy_pass->transform_to_root_target.GetInverse(&inverse_transform)) {
        gfx::Rect damage_rect_in_render_pass_space =
            cc::MathUtil::ProjectEnclosingClippedRect(inverse_transform,
                                                      root_damage_rect_);
        copy_pass->damage_rect.Intersect(damage_rect_in_render_pass_space);
      }
    }

    if (copy_pass->has_damage_from_contributing_content)
      contributing_content_damaged_passes_.insert(copy_pass->id);
    dest_pass_list_->push_back(std::move(copy_pass));
  }

  gfx::Transform surface_transform = scaled_quad_to_target_transform;
  surface_transform.ConcatTransform(target_transform);

  const auto& last_pass = *render_pass_list.back();
  // This will check if all the surface_quads (including child surfaces) has
  // damage because HandleSurfaceQuad is a recursive call by calling
  // CopyQuadsToPass in it.
  dest_pass->has_damage_from_contributing_content |=
      !DamageRectForSurface(surface, last_pass, last_pass.output_rect)
           .IsEmpty();

  if (merge_pass) {
    // TODO(jamesr): Clean up last pass special casing.
    const QuadList& quads = last_pass.quad_list;

    // Intersect the transformed visible rect and the clip rect to create a
    // smaller cliprect for the quad.
    ClipData surface_quad_clip_rect(
        true, cc::MathUtil::MapEnclosingClippedRect(
                  source_sqs->quad_to_target_transform, source_visible_rect));
    if (source_sqs->is_clipped) {
      surface_quad_clip_rect.rect.Intersect(source_sqs->clip_rect);
    }

    ClipData quads_clip =
        CalculateClipRect(clip_rect, surface_quad_clip_rect, target_transform);

    CopyQuadsToPass(quads, last_pass.shared_quad_state_list,
                    surface->GetActiveFrame().device_scale_factor(),
                    child_to_parent_map, surface_transform, quads_clip,
                    dest_pass, surface_id, rounded_corner_info,
                    occluding_damage_rect, occluding_damage_rect_valid);
  } else {
    auto* shared_quad_state = CopyAndScaleSharedQuadState(
        source_sqs, scaled_quad_to_target_transform, target_transform,
        gfx::ScaleToEnclosingRect(source_sqs->quad_layer_rect,
                                  layer_to_content_scale_x,
                                  layer_to_content_scale_y),
        gfx::ScaleToEnclosingRect(source_sqs->visible_quad_layer_rect,
                                  layer_to_content_scale_x,
                                  layer_to_content_scale_y),
        clip_rect, dest_pass, rounded_corner_info, occluding_damage_rect,
        occluding_damage_rect_valid);

    gfx::Rect scaled_rect(gfx::ScaleToEnclosingRect(
        source_rect, layer_to_content_scale_x, layer_to_content_scale_y));

    gfx::Rect scaled_visible_rect(
        gfx::ScaleToEnclosingRect(source_visible_rect, layer_to_content_scale_x,
                                  layer_to_content_scale_y));

    auto* quad = dest_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
    RenderPassId remapped_pass_id = RemapPassId(last_pass.id, surface_id);
    quad->SetNew(shared_quad_state, scaled_rect, scaled_visible_rect,
                 remapped_pass_id, 0, gfx::RectF(), gfx::Size(),
                 gfx::Vector2dF(), gfx::PointF(), gfx::RectF(scaled_rect),
                 /*force_anti_aliasing_off=*/false,
                 /* backdrop_filter_quality*/ 1.0f);
  }

  // Need to re-query since referenced_surfaces_ iterators are not stable.
  referenced_surfaces_.erase(referenced_surfaces_.find(surface_id));
}

void SurfaceAggregator::EmitDefaultBackgroundColorQuad(
    const SurfaceDrawQuad* surface_quad,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    RenderPass* dest_pass,
    const RoundedCornerInfo& rounded_corner_info) {
  // The primary surface is unavailable and there is no fallback
  // surface specified so create a SolidColorDrawQuad with the default
  // background color.
  SkColor background_color = surface_quad->default_background_color;
  auto* shared_quad_state =
      CopySharedQuadState(surface_quad->shared_quad_state, target_transform,
                          clip_rect, dest_pass, rounded_corner_info,
                          /*occluding_damage_rect*/ gfx::Rect(),
                          /*occluding_damage_rect_valid*/ false);

  auto* solid_color_quad =
      dest_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_color_quad->SetNew(shared_quad_state, surface_quad->rect,
                           surface_quad->visible_rect, background_color, false);

  if (needs_surface_occluding_damage_rect_) {
    gfx::Transform transform(
        target_transform,
        surface_quad->shared_quad_state->quad_to_target_transform);
    transform.ConcatTransform(dest_pass->transform_to_root_target);
    UnionSurfaceDamageRectsOnTop(surface_quad->rect, transform, dest_pass);
  }
}

void SurfaceAggregator::EmitGutterQuadsIfNecessary(
    const gfx::Rect& primary_rect,
    const gfx::Rect& fallback_rect,
    const SharedQuadState* primary_shared_quad_state,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    SkColor background_color,
    RenderPass* dest_pass,
    const RoundedCornerInfo& rounded_corner_info) {
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
        right_gutter_rect, right_gutter_rect, clip_rect, dest_pass,
        rounded_corner_info,
        /*occluding_damage_rect*/ gfx::Rect(),
        /*occluding_damage_rect_valid*/ false);

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
        bottom_gutter_rect, bottom_gutter_rect, clip_rect, dest_pass,
        rounded_corner_info,
        /*occluding_damage_rect*/ gfx::Rect(),
        /*occluding_damage_rect_valid*/ false);

    auto* bottom_gutter =
        dest_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bottom_gutter->SetNew(shared_quad_state, bottom_gutter_rect,
                          bottom_gutter_rect, background_color, false);
  }
}

void SurfaceAggregator::AddColorConversionPass() {
  if (dest_pass_list_->empty())
    return;

  auto* root_render_pass = dest_pass_list_->back().get();
  if (root_render_pass->color_space == output_color_space_)
    return;

  gfx::Rect output_rect = root_render_pass->output_rect;
  CHECK(root_render_pass->transform_to_root_target == gfx::Transform());

  if (!color_conversion_render_pass_id_)
    color_conversion_render_pass_id_ = next_render_pass_id_++;

  auto color_conversion_pass = RenderPass::Create(1, 1);
  color_conversion_pass->SetNew(color_conversion_render_pass_id_, output_rect,
                                root_render_pass->damage_rect,
                                root_render_pass->transform_to_root_target);
  color_conversion_pass->has_transparent_background =
      root_render_pass->has_transparent_background;
  color_conversion_pass->color_space = output_color_space_;

  auto* shared_quad_state =
      color_conversion_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(
      /*quad_to_target_transform=*/gfx::Transform(),
      /*quad_layer_rect=*/output_rect,
      /*visible_quad_layer_rect=*/output_rect,
      /*rounded_corner_bounds=*/gfx::RRectF(),
      /*clip_rect=*/gfx::Rect(),
      /*is_clipped=*/false, /*are_contents_opaque=*/false, /*opacity=*/1.f,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  auto* quad =
      color_conversion_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  quad->SetNew(shared_quad_state, output_rect, output_rect,
               root_render_pass->id, 0, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(), gfx::PointF(), gfx::RectF(output_rect),
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
    display_transform_render_pass_id_ = next_render_pass_id_++;

  auto display_transform_pass = RenderPass::Create(1, 1);
  display_transform_pass->SetNew(
      display_transform_render_pass_id_,
      cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          root_surface_transform_, root_render_pass->output_rect),
      cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          root_surface_transform_, root_render_pass->damage_rect),
      gfx::Transform());
  display_transform_pass->color_space = root_render_pass->color_space;

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
      /*visible_quad_layer_rect=*/output_rect,
      /*rounded_corner_bounds=*/gfx::RRectF(),
      /*clip_rect=*/gfx::Rect(),
      /*is_clipped=*/false, are_contents_opaque, /*opacity=*/1.f,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  auto* quad =
      display_transform_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  quad->SetNew(shared_quad_state, output_rect, output_rect,
               root_render_pass->id, 0, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(), gfx::PointF(), gfx::RectF(output_rect),
               /*force_anti_aliasing_off=*/false,
               /*backdrop_filter_quality*/ 1.0f);
  dest_pass_list_->push_back(std::move(display_transform_pass));
}

SharedQuadState* SurfaceAggregator::CopySharedQuadState(
    const SharedQuadState* source_sqs,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    RenderPass* dest_render_pass,
    const RoundedCornerInfo& rounded_corner_info,
    const gfx::Rect& occluding_damage_rect,
    bool occluding_damage_rect_valid) {
  return CopyAndScaleSharedQuadState(
      source_sqs, source_sqs->quad_to_target_transform, target_transform,
      source_sqs->quad_layer_rect, source_sqs->visible_quad_layer_rect,
      clip_rect, dest_render_pass, rounded_corner_info, occluding_damage_rect,
      occluding_damage_rect_valid);
}

SurfaceAggregator::RoundedCornerInfo::RoundedCornerInfo(
    const gfx::RRectF& bounds_arg,
    bool is_fast_rounded_corner_arg,
    const gfx::Transform target_transform) {
  is_fast_rounded_corner = is_fast_rounded_corner_arg;
  if (bounds_arg.IsEmpty())
    return;
  DCHECK(target_transform.Preserves2dAxisAlignment());
  bounds = bounds_arg;
  SkMatrix matrix = target_transform.matrix();
  bounds.Scale(matrix.getScaleX(), matrix.getScaleY());
  bounds.Offset(target_transform.To2dTranslation());
}

SharedQuadState* SurfaceAggregator::CopyAndScaleSharedQuadState(
    const SharedQuadState* source_sqs,
    const gfx::Transform& scaled_quad_to_target_transform,
    const gfx::Transform& target_transform,
    const gfx::Rect& quad_layer_rect,
    const gfx::Rect& visible_quad_layer_rect,
    const ClipData& clip_rect,
    RenderPass* dest_render_pass,
    const RoundedCornerInfo& rounded_corner_info,
    const gfx::Rect& occluding_damage_rect,
    bool occluding_damage_rect_valid) {
  auto* shared_quad_state = dest_render_pass->CreateAndAppendSharedQuadState();
  ClipData new_clip_rect = CalculateClipRect(
      clip_rect, ClipData(source_sqs->is_clipped, source_sqs->clip_rect),
      target_transform);

  // target_transform contains any transformation that may exist
  // between the context that these quads are being copied from (i.e. the
  // surface's draw transform when aggregated from within a surface) to the
  // target space of the pass. This will be identity except when copying the
  // root draw pass from a surface into a pass when the surface draw quad's
  // transform is not identity.
  gfx::Transform new_transform = scaled_quad_to_target_transform;
  new_transform.ConcatTransform(target_transform);

  shared_quad_state->SetAll(
      new_transform, quad_layer_rect, visible_quad_layer_rect,
      rounded_corner_info.bounds, new_clip_rect.rect, new_clip_rect.is_clipped,
      source_sqs->are_contents_opaque, source_sqs->opacity,
      source_sqs->blend_mode, source_sqs->sorting_context_id);
  shared_quad_state->is_fast_rounded_corner =
      rounded_corner_info.is_fast_rounded_corner;
  if (occluding_damage_rect_valid) {
    shared_quad_state->occluding_damage_rect = occluding_damage_rect;
  }

  return shared_quad_state;
}

void SurfaceAggregator::CopyQuadsToPass(
    const QuadList& source_quad_list,
    const SharedQuadStateList& source_shared_quad_state_list,
    float parent_device_scale_factor,
    const std::unordered_map<ResourceId, ResourceId>& child_to_parent_map,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    RenderPass* dest_pass,
    const SurfaceId& surface_id,
    const RoundedCornerInfo& parent_rounded_corner_info,
    const gfx::Rect& occluding_damage_rect,
    bool occluding_damage_rect_valid) {
  const SharedQuadState* last_copied_source_shared_quad_state = nullptr;
  // If the current frame has copy requests or cached render passes, then
  // aggregate the entire thing, as otherwise parts of the copy requests may be
  // ignored and we could cache partially drawn render pass.
  const bool ignore_undamaged =
      aggregate_only_damaged_ && !has_copy_requests_ &&
      !has_cached_render_passes_ && !moved_pixel_passes_.count(dest_pass->id);
  // Damage rect in the quad space of the current shared quad state.
  // TODO(jbauman): This rect may contain unnecessary area if
  // transform isn't axis-aligned.
  gfx::Rect damage_rect_in_quad_space;
  bool damage_rect_in_quad_space_valid = false;

#if DCHECK_IS_ON()
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

  RoundedCornerInfo new_rounded_corner_info = parent_rounded_corner_info;
  for (auto* quad : source_quad_list) {
    // Both cannot be set at once. If this happens then a surface is being
    // merged when it should not.
    DCHECK(quad->shared_quad_state->rounded_corner_bounds.IsEmpty() ||
           parent_rounded_corner_info.IsEmpty());

    if (quad->material == DrawQuad::Material::kSurfaceContent) {
      const auto* surface_quad = SurfaceDrawQuad::MaterialCast(quad);
      // HandleSurfaceQuad may add other shared quad state, so reset the
      // current data.
      last_copied_source_shared_quad_state = nullptr;

      if (!surface_quad->surface_range.end().is_valid())
        continue;

      if (parent_rounded_corner_info.IsEmpty()) {
        new_rounded_corner_info = RoundedCornerInfo(
            quad->shared_quad_state->rounded_corner_bounds,
            quad->shared_quad_state->is_fast_rounded_corner, target_transform);
      }

      HandleSurfaceQuad(
          surface_quad, parent_device_scale_factor, target_transform, clip_rect,
          dest_pass, ignore_undamaged, &damage_rect_in_quad_space,
          &damage_rect_in_quad_space_valid, new_rounded_corner_info);
    } else {
      if (quad->shared_quad_state != last_copied_source_shared_quad_state) {
        if (parent_rounded_corner_info.IsEmpty()) {
          new_rounded_corner_info =
              RoundedCornerInfo(quad->shared_quad_state->rounded_corner_bounds,
                                quad->shared_quad_state->is_fast_rounded_corner,
                                target_transform);
        }
        const SharedQuadState* dest_shared_quad_state = CopySharedQuadState(
            quad->shared_quad_state, target_transform, clip_rect, dest_pass,
            new_rounded_corner_info, occluding_damage_rect,
            occluding_damage_rect_valid);

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
      if (quad->material == DrawQuad::Material::kRenderPass) {
        const auto* pass_quad = RenderPassDrawQuad::MaterialCast(quad);
        RenderPassId original_pass_id = pass_quad->render_pass_id;
        RenderPassId remapped_pass_id =
            RemapPassId(original_pass_id, surface_id);

        // If the RenderPassDrawQuad is referring to other render pass with the
        // |has_damage_from_contributing_content| set on it, then the dest_pass
        // should have the flag set on it as well.
        if (contributing_content_damaged_passes_.count(remapped_pass_id))
          dest_pass->has_damage_from_contributing_content = true;

        dest_quad = dest_pass->CopyFromAndAppendRenderPassDrawQuad(
            pass_quad, remapped_pass_id);
      } else if (quad->material == DrawQuad::Material::kTextureContent) {
        const auto* texture_quad = TextureDrawQuad::MaterialCast(quad);
        if (texture_quad->secure_output_only &&
            (!output_is_secure_ || copy_request_passes_.count(dest_pass->id))) {
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
      if (!child_to_parent_map.empty()) {
        for (ResourceId& resource_id : dest_quad->resources) {
          auto it = child_to_parent_map.find(resource_id);
          DCHECK(it != child_to_parent_map.end());

          DCHECK_EQ(it->first, resource_id);
          ResourceId remapped_id = it->second;
          resource_id = remapped_id;
        }
      }
    }
  }
}

void SurfaceAggregator::CopyPasses(const CompositorFrame& frame,
                                   Surface* surface) {
  // The root surface is allowed to have copy output requests, so grab them
  // off its render passes. This map contains a set of CopyOutputRequests
  // keyed by each RenderPass id.
  Surface::CopyRequestsMap copy_requests;
  surface->TakeCopyOutputRequests(&copy_requests);

  const auto& source_pass_list = frame.render_pass_list;
  DCHECK(valid_surfaces_.count(surface->surface_id()));
  if (!valid_surfaces_.count(surface->surface_id()))
    return;

  // TODO(vmpstr): provider check is a hack for unittests that don't set up a
  // resource provider.
  std::unordered_map<ResourceId, ResourceId> empty_map;
  const auto& child_to_parent_map =
      provider_ ? provider_->GetChildToParentMap(ChildIdForSurface(surface))
                : empty_map;

  const gfx::Transform surface_transform =
      IsRootSurface(surface) ? root_surface_transform_ : gfx::Transform();

  gfx::Rect occluding_damage_rect;
  bool occluding_damage_rect_valid = ProcessSurfaceOccludingDamage(
      surface, source_pass_list, surface_transform,
      source_pass_list.back().get(), &occluding_damage_rect);

  bool apply_surface_transform_to_root_pass = true;
  for (size_t i = 0; i < source_pass_list.size(); ++i) {
    const auto& source = *source_pass_list[i];
    const bool is_root_pass = (i == source_pass_list.size() - 1);

    size_t sqs_size = source.shared_quad_state_list.size();
    size_t dq_size = source.quad_list.size();
    auto copy_pass = RenderPass::Create(sqs_size, dq_size);

    MoveMatchingRequests(source.id, &copy_requests, &copy_pass->copy_requests);

    // We add an additional render pass for the transform if the root render
    // pass has any copy requests.
    apply_surface_transform_to_root_pass =
        is_root_pass &&
        (copy_pass->copy_requests.empty() || surface_transform.IsIdentity());

    RenderPassId remapped_pass_id =
        RemapPassId(source.id, surface->surface_id());

    gfx::Rect output_rect = source.output_rect;
    gfx::Rect damage_rect = source.output_rect;
    gfx::Transform transform_to_root_target = source.transform_to_root_target;
    if (apply_surface_transform_to_root_pass) {
      // If we don't need an additional render pass to apply the surface
      // transform, adjust the root pass's rects to account for it.
      output_rect = cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          surface_transform, output_rect);
      damage_rect = cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          surface_transform, damage_rect);
    } else {
      // For the non-root render passes, the transform to root target needs to
      // be adjusted to include the root surface transform. This is also true if
      // we will be adding another render pass for the surface transform, in
      // which this will no longer be the root.
      transform_to_root_target =
          gfx::Transform(surface_transform, source.transform_to_root_target);
    }

    copy_pass->SetAll(
        remapped_pass_id, output_rect, damage_rect, transform_to_root_target,
        source.filters, source.backdrop_filters, source.backdrop_filter_bounds,
        blending_color_space_, source.has_transparent_background,
        source.cache_render_pass, source.has_damage_from_contributing_content,
        source.generate_mipmap);

    CopyQuadsToPass(source.quad_list, source.shared_quad_state_list,
                    frame.device_scale_factor(), child_to_parent_map,
                    apply_surface_transform_to_root_pass ? surface_transform
                                                         : gfx::Transform(),
                    ClipData(), copy_pass.get(), surface->surface_id(),
                    RoundedCornerInfo(), occluding_damage_rect,
                    occluding_damage_rect_valid);

    // If the render pass has copy requests, or should be cached, or has
    // moving-pixel filters, or in a moving-pixel surface, we should damage the
    // whole output rect so that we always drawn the full content. Otherwise, we
    // might have incompleted copy request, or cached patially drawn render
    // pass.
    if (!RenderPassNeedsFullDamage(copy_pass.get())) {
      gfx::Transform inverse_transform(gfx::Transform::kSkipInitialization);
      if (copy_pass->transform_to_root_target.GetInverse(&inverse_transform)) {
        gfx::Rect damage_rect_in_render_pass_space =
            cc::MathUtil::ProjectEnclosingClippedRect(inverse_transform,
                                                      root_damage_rect_);
        copy_pass->damage_rect.Intersect(damage_rect_in_render_pass_space);
      }
    }

    if (copy_pass->has_damage_from_contributing_content)
      contributing_content_damaged_passes_.insert(copy_pass->id);
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

// Walk the Surface tree from surface_id. Validate the resources of the current
// surface and its descendants, check if there are any copy requests, and
// return the combined damage rect.
gfx::Rect SurfaceAggregator::PrewalkTree(Surface* surface,
                                         bool in_moved_pixel_surface,
                                         int parent_pass_id,
                                         bool will_draw,
                                         PrewalkResult* result) {
  if (referenced_surfaces_.count(surface->surface_id()))
    return gfx::Rect();

  contained_surfaces_[surface->surface_id()] = surface->GetActiveFrameIndex();
  LocalSurfaceId& local_surface_id =
      contained_frame_sinks_[surface->surface_id().frame_sink_id()];
  local_surface_id =
      std::max(surface->surface_id().local_surface_id(), local_surface_id);

  if (!surface->HasActiveFrame())
    return gfx::Rect();

  const CompositorFrame& frame = surface->GetActiveFrame();
  int child_id = 0;
  // TODO(jbauman): hack for unit tests that don't set up rp
  if (provider_) {
    child_id = ChildIdForSurface(surface);
    surface->RefResources(frame.resource_list);
    provider_->ReceiveFromChild(child_id, frame.resource_list);
  }

  std::vector<ResourceId> referenced_resources;
  size_t reserve_size = frame.resource_list.size();
  referenced_resources.reserve(reserve_size);

  bool invalid_frame = false;
  std::unordered_map<ResourceId, ResourceId> empty_map;
  const auto& child_to_parent_map =
      provider_ ? provider_->GetChildToParentMap(child_id) : empty_map;

  RenderPassId remapped_pass_id =
      RemapPassId(frame.render_pass_list.back()->id, surface->surface_id());
  if (in_moved_pixel_surface)
    moved_pixel_passes_.insert(remapped_pass_id);
  if (parent_pass_id)
    render_pass_dependencies_[parent_pass_id].insert(remapped_pass_id);

  const gfx::Transform& root_pass_transform =
      IsRootSurface(surface) ? root_surface_transform_ : gfx::Transform();
  struct SurfaceInfo {
    SurfaceInfo(const SurfaceRange& surface_range,
                bool has_moved_pixels,
                RenderPassId parent_pass_id,
                const gfx::Transform& target_to_surface_transform,
                const gfx::Rect& quad_rect,
                bool stretch_content_to_fill_bounds,
                bool is_clipped,
                const gfx::Rect& clip_rect_in_root_target_space)
        : surface_range(surface_range),
          has_moved_pixels(has_moved_pixels),
          parent_pass_id(parent_pass_id),
          target_to_surface_transform(target_to_surface_transform),
          quad_rect(quad_rect),
          stretch_content_to_fill_bounds(stretch_content_to_fill_bounds),
          is_clipped(is_clipped),
          clip_rect_in_root_target_space(clip_rect_in_root_target_space) {}
    SurfaceRange surface_range;
    bool has_moved_pixels;
    RenderPassId parent_pass_id;
    gfx::Transform target_to_surface_transform;
    gfx::Rect quad_rect;
    bool stretch_content_to_fill_bounds;
    bool is_clipped;
    gfx::Rect clip_rect_in_root_target_space;
  };
  std::vector<SurfaceInfo> child_surfaces;

  gfx::Rect pixel_moving_backdrop_filters_rect;
  // This data is created once and typically small or empty. Collect all items
  // and pass to a flat_vector to sort once.
  std::vector<RenderPassId> pixel_moving_background_filter_passes_data;
  const auto* root_pass_in_root_surface =
      IsRootSurface(surface) ? frame.render_pass_list.back().get() : nullptr;
  for (const auto& render_pass : frame.render_pass_list) {
    if (render_pass->backdrop_filters.HasFilterThatMovesPixels()) {
      DCHECK_NE(render_pass.get(), root_pass_in_root_surface)
          << "The root render pass on the root surface can not have backdrop "
             "affecting filters";

      pixel_moving_background_filter_passes_data.push_back(
          RemapPassId(render_pass->id, surface->surface_id()));

      gfx::Transform transform_to_root_target(
          root_pass_transform, render_pass->transform_to_root_target);
      pixel_moving_backdrop_filters_rect.Union(
          cc::MathUtil::MapEnclosingClippedRect(transform_to_root_target,
                                                render_pass->output_rect));
    }
  }
  base::flat_set<RenderPassId> pixel_moving_background_filter_passes(
      std::move(pixel_moving_background_filter_passes_data),
      base::KEEP_FIRST_OF_DUPES);

  for (const auto& render_pass : base::Reversed(frame.render_pass_list)) {
    RenderPassId remapped_pass_id =
        RemapPassId(render_pass->id, surface->surface_id());
    bool has_pixel_moving_filter =
        render_pass->filters.HasFilterThatMovesPixels();
    if (has_pixel_moving_filter)
      moved_pixel_passes_.insert(remapped_pass_id);
    bool in_moved_pixel_pass =
        has_pixel_moving_filter ||
        base::ContainsKey(moved_pixel_passes_, remapped_pass_id);

    for (auto* quad : render_pass->quad_list) {
      if (quad->material == DrawQuad::Material::kSurfaceContent) {
        const auto* surface_quad = SurfaceDrawQuad::MaterialCast(quad);
        gfx::Transform transform_to_root_target(
            root_pass_transform, render_pass->transform_to_root_target);
        gfx::Transform target_to_surface_transform(
            transform_to_root_target,
            surface_quad->shared_quad_state->quad_to_target_transform);

        gfx::Rect clip_rect_in_root_target_space;
        if (surface_quad->shared_quad_state->is_clipped) {
          // clip_rect is already in quad target space so only
          // transform_to_root_target needs to be applied
          clip_rect_in_root_target_space =
              cc::MathUtil::MapEnclosingClippedRect(
                  transform_to_root_target,
                  surface_quad->shared_quad_state->clip_rect);
        }
        child_surfaces.emplace_back(
            surface_quad->surface_range, in_moved_pixel_pass, remapped_pass_id,
            target_to_surface_transform, surface_quad->rect,
            surface_quad->stretch_content_to_fill_bounds,
            surface_quad->shared_quad_state->is_clipped,
            clip_rect_in_root_target_space);
      } else if (quad->material == DrawQuad::Material::kRenderPass) {
        const auto* render_pass_quad = RenderPassDrawQuad::MaterialCast(quad);
        if (in_moved_pixel_pass) {
          moved_pixel_passes_.insert(RemapPassId(
              render_pass_quad->render_pass_id, surface->surface_id()));
        }
        if (pixel_moving_background_filter_passes.count(
                render_pass_quad->render_pass_id)) {
          in_moved_pixel_pass = true;
        }
        render_pass_dependencies_[remapped_pass_id].insert(RemapPassId(
            render_pass_quad->render_pass_id, surface->surface_id()));
      }

      if (!provider_)
        continue;
      for (ResourceId resource_id : quad->resources) {
        if (!child_to_parent_map.count(resource_id)) {
          invalid_frame = true;
          break;
        }
        referenced_resources.push_back(resource_id);
      }
    }
  }

  if (invalid_frame)
    return gfx::Rect();
  valid_surfaces_.insert(surface->surface_id());

  ResourceIdSet resource_set(std::move(referenced_resources),
                             base::KEEP_FIRST_OF_DUPES);
  if (provider_)
    provider_->DeclareUsedResourcesFromChild(child_id, resource_set);

  gfx::Rect damage_rect;
  gfx::Rect full_damage;
  RenderPass* last_pass = frame.render_pass_list.back().get();
  full_damage = last_pass->output_rect;
  damage_rect =
      DamageRectForSurface(surface, *last_pass, last_pass->output_rect);
  damage_rect = cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
      root_pass_transform, damage_rect);

  // Avoid infinite recursion by adding current surface to
  // referenced_surfaces_.
  referenced_surfaces_.insert(surface->surface_id());
  for (const auto& surface_info : child_surfaces) {
    // TODO(fsamuel): Consider caching this value somewhere so that
    // HandleSurfaceQuad doesn't need to call it again.
    Surface* child_surface =
        manager_->GetLatestInFlightSurface(surface_info.surface_range);

    // If the primary surface is not available then we assume the damage is
    // the full size of the SurfaceDrawQuad because we might need to introduce
    // gutter.
    gfx::Rect surface_damage;
    if (!child_surface ||
        child_surface->surface_id() != surface_info.surface_range.end()) {
      surface_damage = surface_info.quad_rect;
    }

    if (child_surface) {
      if (surface_info.stretch_content_to_fill_bounds) {
        // Scale up the damage_quad generated by the child_surface to fit
        // the containing quad_rect.
        gfx::Rect child_rect =
            PrewalkTree(child_surface, surface_info.has_moved_pixels,
                        surface_info.parent_pass_id, will_draw, result);
        if (child_surface->size_in_pixels().GetCheckedArea().ValueOrDefault(0) >
            0) {
          float y_scale = static_cast<float>(surface_info.quad_rect.height()) /
                          child_surface->size_in_pixels().height();
          float x_scale = static_cast<float>(surface_info.quad_rect.width()) /
                          child_surface->size_in_pixels().width();
          surface_damage.Union(
              gfx::ScaleToEnclosingRect(child_rect, x_scale, y_scale));
        }
      } else {
        surface_damage.Union(
            PrewalkTree(child_surface, surface_info.has_moved_pixels,
                        surface_info.parent_pass_id, will_draw, result));
      }
    }

    if (surface_damage.IsEmpty())
      continue;

    if (surface_info.has_moved_pixels) {
      // Areas outside the rect hit by target_to_surface_transform may be
      // modified if there is a filter that moves pixels.
      damage_rect = full_damage;
      continue;
    }

    gfx::Rect surface_damage_in_root_target_space =
        cc::MathUtil::MapEnclosingClippedRect(
            surface_info.target_to_surface_transform, surface_damage);
    if (surface_info.is_clipped) {
      surface_damage_in_root_target_space.Intersect(
          surface_info.clip_rect_in_root_target_space);
    }
    damage_rect.Union(surface_damage_in_root_target_space);
  }

  if (!damage_rect.IsEmpty()) {
    // The following call can cause one or more copy requests to be added to the
    // Surface. Therefore, no code before this point should have assumed
    // anything about the presence or absence of copy requests after this point.

    // The damage reported to the surface is in pre-display transform space
    // since it is used by clients which are not aware of the display transform.
    gfx::Transform inverse(gfx::Transform::kSkipInitialization);
    bool inverted = root_pass_transform.GetInverse(&inverse);
    DCHECK(inverted);
    surface->NotifyAggregatedDamage(
        cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(inverse,
                                                                damage_rect),
        expected_display_time_);
  }

  // If any CopyOutputRequests were made at FrameSink level, make sure we grab
  // them too.
  surface->TakeCopyOutputRequestsFromClient();

  if (will_draw)
    surface->OnWillBeDrawn();

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
      result->undrawn_surfaces.insert(surface_id);
      Surface* undrawn_surface = manager_->GetSurfaceForId(surface_id);
      if (undrawn_surface)
        PrewalkTree(undrawn_surface, false, 0, false /* will_draw */, result);
    }
  }

  for (const auto& render_pass : frame.render_pass_list) {
    if (!render_pass->copy_requests.empty()) {
      RenderPassId remapped_pass_id =
          RemapPassId(render_pass->id, surface->surface_id());
      copy_request_passes_.insert(remapped_pass_id);
    }
    if (render_pass->cache_render_pass)
      has_cached_render_passes_ = true;
  }

  auto it = referenced_surfaces_.find(surface->surface_id());
  referenced_surfaces_.erase(it);
  if (!damage_rect.IsEmpty() && frame.metadata.may_contain_video)
    result->may_contain_video = true;

  if (damage_rect.Intersects(pixel_moving_backdrop_filters_rect))
    damage_rect.Union(pixel_moving_backdrop_filters_rect);

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
    Surface* surface = manager_->GetSurfaceForId(surface_id);
    if (!surface)
      continue;
    if (!surface->HasActiveFrame())
      continue;
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
      CopyPasses(surface->GetActiveFrame(), surface);
      // CopyPasses may have mutated container, need to re-query to erase.
      referenced_surfaces_.erase(referenced_surfaces_.find(surface_id));
    }
  }
}

void SurfaceAggregator::PropagateCopyRequestPasses() {
  std::vector<RenderPassId> copy_requests_to_iterate(
      copy_request_passes_.begin(), copy_request_passes_.end());
  while (!copy_requests_to_iterate.empty()) {
    RenderPassId first = copy_requests_to_iterate.back();
    copy_requests_to_iterate.pop_back();
    auto it = render_pass_dependencies_.find(first);
    if (it == render_pass_dependencies_.end())
      continue;
    for (auto pass : it->second) {
      if (copy_request_passes_.insert(pass).second) {
        copy_requests_to_iterate.push_back(pass);
      }
    }
  }
}

bool SurfaceAggregator::CanMergeRoundedCorner(
    const RoundedCornerInfo& rounded_corner_info,
    const RenderPass& root_render_pass) {
  // If the quad has no rounded corner, then we do not have to block merging.
  if (rounded_corner_info.IsEmpty())
    return true;

  // If the quad has rounded corner and it is not a fast rounded corner, we
  // cannot merge.
  if (!rounded_corner_info.is_fast_rounded_corner)
    return false;

  // If any of the quads in the root render pass has a rounded corner of its
  // own, then we cannot merge.
  const SharedQuadStateList& sqs_list = root_render_pass.shared_quad_state_list;
  for (const auto* sqs : sqs_list) {
    if (!sqs->rounded_corner_bounds.IsEmpty())
      return false;
  }
  return true;
}

CompositorFrame SurfaceAggregator::Aggregate(
    const SurfaceId& surface_id,
    base::TimeTicks expected_display_time,
    gfx::OverlayTransform display_transform,
    int64_t display_trace_id) {
  DCHECK(!expected_display_time.is_null());

  root_surface_id_ = surface_id;
  Surface* surface = manager_->GetSurfaceForId(surface_id);
  DCHECK(surface);
  DCHECK(contained_surfaces_.empty());
  contained_surfaces_[surface_id] = surface->GetActiveFrameIndex();

  LocalSurfaceId& local_surface_id =
      contained_frame_sinks_[surface_id.frame_sink_id()];
  local_surface_id =
      std::max(surface->surface_id().local_surface_id(), local_surface_id);

  if (!surface->HasActiveFrame())
    return {};

  base::AutoReset<int64_t> reset_display_trace_id(&display_trace_id_,
                                                  display_trace_id);
  const CompositorFrame& root_surface_frame = surface->GetActiveFrame();
  TRACE_EVENT_WITH_FLOW2(
      "viz,benchmark", "Graphics.Pipeline",
      TRACE_ID_GLOBAL(root_surface_frame.metadata.begin_frame_ack.trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SurfaceAggregation", "display_trace", display_trace_id_);

  CompositorFrame frame;

  dest_pass_list_ = &frame.render_pass_list;
  expected_display_time_ = expected_display_time;

  const gfx::Size viewport_bounds =
      root_surface_frame.render_pass_list.back()->output_rect.size();
  root_surface_transform_ =
      gfx::OverlayTransformToTransform(display_transform, viewport_bounds);

  valid_surfaces_.clear();
  has_cached_render_passes_ = false;
  damage_ranges_.clear();
  damage_rects_union_of_surfaces_on_top_ = gfx::Rect();
  DCHECK(referenced_surfaces_.empty());
  PrewalkResult prewalk_result;
  root_damage_rect_ =
      PrewalkTree(surface, false, 0, true /* will_draw */, &prewalk_result);

  PropagateCopyRequestPasses();
  has_copy_requests_ = !copy_request_passes_.empty();
  frame.metadata.may_contain_video = prewalk_result.may_contain_video;

  CopyUndrawnSurfaces(&prewalk_result);
  referenced_surfaces_.insert(surface_id);
  CopyPasses(root_surface_frame, surface);
  // CopyPasses may have mutated container, need to re-query to erase.
  referenced_surfaces_.erase(referenced_surfaces_.find(surface_id));
  AddColorConversionPass();

  moved_pixel_passes_.clear();
  copy_request_passes_.clear();
  contributing_content_damaged_passes_.clear();
  render_pass_dependencies_.clear();

  // Remove all render pass mappings that weren't used in the current frame.
  for (auto it = render_pass_allocator_map_.begin();
       it != render_pass_allocator_map_.end();) {
    if (it->second.in_use) {
      it->second.in_use = false;
      it++;
    } else {
      it = render_pass_allocator_map_.erase(it);
    }
  }

  DCHECK(referenced_surfaces_.empty());

  if (dest_pass_list_->empty())
    return {};

  dest_pass_list_ = nullptr;
  expected_display_time_ = base::TimeTicks();
  ProcessAddedAndRemovedSurfaces();
  contained_surfaces_.swap(previous_contained_surfaces_);
  contained_surfaces_.clear();
  contained_frame_sinks_.swap(previous_contained_frame_sinks_);
  contained_frame_sinks_.clear();

  for (auto it : previous_contained_surfaces_) {
    Surface* surface = manager_->GetSurfaceForId(it.first);
    if (surface) {
      surface->allocation_group()->TakeAggregatedLatencyInfoUpTo(
          surface, &frame.metadata.latency_info);
    }
    if (!ui::LatencyInfo::Verify(frame.metadata.latency_info,
                                 "SurfaceAggregator::Aggregate")) {
      break;
    }
  }
  return frame;
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

void SurfaceAggregator::SetOutputColorSpace(
    const gfx::ColorSpace& blending_color_space,
    const gfx::ColorSpace& output_color_space) {
  blending_color_space_ = blending_color_space.IsValid()
                              ? blending_color_space
                              : gfx::ColorSpace::CreateSRGB();
  output_color_space_ = output_color_space.IsValid()
                            ? output_color_space
                            : gfx::ColorSpace::CreateSRGB();
}

bool SurfaceAggregator::NotifySurfaceDamageAndCheckForDisplayDamage(
    const SurfaceId& surface_id) {
  if (previous_contained_surfaces_.count(surface_id)) {
    Surface* surface = manager_->GetSurfaceForId(surface_id);
    if (surface) {
      DCHECK(surface->HasActiveFrame());
      if (surface->GetActiveFrame().resource_list.empty())
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

bool SurfaceAggregator::IsRootSurface(const Surface* surface) const {
  return surface->surface_id() == root_surface_id_;
}

}  // namespace viz
