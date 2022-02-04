// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_using_strategy.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/quad_list.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"

#include "components/viz/common/quads/texture_draw_quad.h"

namespace viz {
namespace {

// Gets the minimum scaling amount used by either dimension for the src relative
// to the dst.
float GetMinScaleFactor(const OverlayCandidate& candidate) {
  if (candidate.resource_size_in_pixels.IsEmpty() ||
      candidate.uv_rect.IsEmpty()) {
    return 1.0f;
  }
  return std::min(candidate.display_rect.width() /
                      (candidate.uv_rect.width() *
                       candidate.resource_size_in_pixels.width()),
                  candidate.display_rect.height() /
                      (candidate.uv_rect.height() *
                       candidate.resource_size_in_pixels.height()));
}

// Modifies an OverlayCandidate so that the |org_src_rect| (which should
// correspond to the src rect before any modifications were made) is scaled by
// |scale_factor| and then clipped and aligned on integral subsampling
// boundaries. This is used for dealing with required overlays and scaling
// limitations.
void ScaleCandidateSrcRect(const gfx::RectF& org_src_rect,
                           float scale_factor,
                           OverlayCandidate* candidate) {
  gfx::RectF src_rect(org_src_rect);
  src_rect.set_width(org_src_rect.width() / scale_factor);
  src_rect.set_height(org_src_rect.height() / scale_factor);

  // Make it an integral multiple of the subsampling factor.
  constexpr int kSubsamplingFactor = 2;
  src_rect.set_x(kSubsamplingFactor *
                 (std::lround(src_rect.x()) / kSubsamplingFactor));
  src_rect.set_y(kSubsamplingFactor *
                 (std::lround(src_rect.y()) / kSubsamplingFactor));
  src_rect.set_width(kSubsamplingFactor *
                     (std::lround(src_rect.width()) / kSubsamplingFactor));
  src_rect.set_height(kSubsamplingFactor *
                      (std::lround(src_rect.height()) / kSubsamplingFactor));
  // Scale it back into UV space and set it in the candidate.
  candidate->uv_rect = gfx::ScaleRect(
      src_rect, 1.0f / candidate->resource_size_in_pixels.width(),
      1.0f / candidate->resource_size_in_pixels.height());
}
}  // namespace

static void LogStrategyEnumUMA(OverlayStrategy strategy) {
  UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayStrategy", strategy);
}

OverlayProcessorUsingStrategy::ProposedCandidateKey
OverlayProcessorUsingStrategy::ToProposeKey(
    const OverlayProcessorUsingStrategy::Strategy::OverlayProposedCandidate&
        proposed) {
  return {proposed.candidate.tracking_id, proposed.strategy->GetUMAEnum()};
}

// Default implementation of whether a strategy would remove the output surface
// as overlay plane.
bool OverlayProcessorUsingStrategy::Strategy::RemoveOutputSurfaceAsOverlay() {
  return false;
}

OverlayStrategy OverlayProcessorUsingStrategy::Strategy::GetUMAEnum() const {
  return OverlayStrategy::kUnknown;
}

gfx::RectF OverlayProcessorUsingStrategy::Strategy::GetPrimaryPlaneDisplayRect(
    const PrimaryPlane* primary_plane) {
  return primary_plane ? primary_plane->display_rect : gfx::RectF();
}

OverlayProcessorUsingStrategy::OverlayProcessorUsingStrategy()
    : max_overlays_considered_(features::MaxOverlaysConsidered()) {}

OverlayProcessorUsingStrategy::~OverlayProcessorUsingStrategy() = default;

gfx::Rect OverlayProcessorUsingStrategy::GetPreviousFrameOverlaysBoundingRect()
    const {
  gfx::Rect result = overlay_damage_rect_;
  result.Union(previous_frame_overlay_rect_);
  return result;
}

gfx::Rect OverlayProcessorUsingStrategy::GetAndResetOverlayDamage() {
  gfx::Rect result = overlay_damage_rect_;
  overlay_damage_rect_ = gfx::Rect();
  return result;
}

void OverlayProcessorUsingStrategy::NotifyOverlayPromotion(
    DisplayResourceProvider* display_resource_provider,
    const CandidateList& candidates,
    const QuadList& quad_list) {}

void OverlayProcessorUsingStrategy::SetFrameSequenceNumber(
    uint64_t frame_sequence_number) {
  frame_sequence_number_ = frame_sequence_number;
}

void OverlayProcessorUsingStrategy::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_passes,
    const skia::Matrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    SurfaceDamageRectList surface_damage_rect_list,
    OutputSurfaceOverlayPlane* output_surface_plane,
    CandidateList* candidates,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  TRACE_EVENT0("viz", "OverlayProcessorUsingStrategy::ProcessForOverlays");
  DCHECK(candidates->empty());
  auto* render_pass = render_passes->back().get();
  bool success = false;

  DBG_DRAW_RECT("overlay.incoming.damage", (*damage_rect));
  for (auto&& each : surface_damage_rect_list) {
    DBG_DRAW_RECT("overlay.surface.damage", each);
  }

  // If we have any copy requests, we can't remove any quads for overlays or
  // CALayers because the framebuffer would be missing the removed quads'
  // contents.
  if (render_pass->copy_requests.empty()) {
    if (features::IsOverlayPrioritizationEnabled()) {
      success = AttemptWithStrategiesPrioritized(
          output_color_matrix, render_pass_backdrop_filters, resource_provider,
          render_passes, &surface_damage_rect_list, output_surface_plane,
          candidates, content_bounds, damage_rect);
    } else {
      success = AttemptWithStrategies(
          output_color_matrix, render_pass_backdrop_filters, resource_provider,
          render_passes, &surface_damage_rect_list, output_surface_plane,
          candidates, content_bounds);
    }
  }
  LogCheckOverlaySupportMetrics();

  DCHECK(candidates->empty() || success);

  UpdateDamageRect(candidates, &surface_damage_rect_list,
                   &render_pass->quad_list, damage_rect);

  NotifyOverlayPromotion(resource_provider, *candidates,
                         render_pass->quad_list);

  if (!candidates->empty()) {
    DBG_DRAW_RECT("overlay.selected.rect", (*candidates)[0].display_rect);
  }
  DBG_DRAW_RECT("overlay.outgoing.damage", (*damage_rect));

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                 "Scheduled overlay planes", candidates->size());
}

void OverlayProcessorUsingStrategy::CheckOverlaySupport(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidate_list) {
  base::ElapsedTimer timer;

  CheckOverlaySupportImpl(primary_plane, candidate_list);
  check_overlay_support_call_count_++;

  base::TimeDelta time = timer.Elapsed();

  static constexpr base::TimeDelta kMinTime = base::Microseconds(1);
  static constexpr base::TimeDelta kMaxTime = base::Milliseconds(10);
  static constexpr int kTimeBuckets = 50;
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Compositing.Display.OverlayProcessorUsingStrategy.CheckOverlaySupportUs",
      time, kMinTime, kMaxTime, kTimeBuckets);
}

// This local function simply recomputes the root damage from
// |surface_damage_rect_list| while excluding the damage contribution from a
// specific overlay.
// TODO(petermcneeley): Eventually this code should be commonized in the same
// location as the definition of |SurfaceDamageRectList|
namespace {
gfx::Rect ComputeDamageExcludingIndex(
    uint32_t overlay_damage_index,
    SurfaceDamageRectList* surface_damage_rect_list,
    const gfx::Rect& existing_damage,
    const gfx::Rect& display_rect,
    bool is_opaque,
    bool is_underlay) {
  gfx::Rect root_damage_rect;

  if (overlay_damage_index == OverlayCandidate::kInvalidDamageIndex) {
    // An opaque overlay that is on top will hide any damage underneath.
    // TODO(petermcneeley): This is a special case optimization which could be
    // removed if we had more reliable damage.
    if (is_opaque && !is_underlay) {
      return gfx::SubtractRects(existing_damage, display_rect);
    }
    return existing_damage;
  }

  gfx::Rect occluding_rect;
  for (size_t i = 0; i < surface_damage_rect_list->size(); i++) {
    if (overlay_damage_index != i) {
      gfx::Rect curr_surface_damage = (*surface_damage_rect_list)[i];

      // The |surface_damage_rect_list| can include damage rects coming from
      // outside and partially outside the original |existing_damage| area. This
      // is due to the conditional inclusion of these damage rects based on
      // target damage in surface aggregator. So by restricting this damage to
      // the |existing_damage| we avoid unnecessary final damage output.
      // https://crbug.com/1197609
      curr_surface_damage.Intersect(existing_damage);
      // Only add damage back in if it is not occluded by the overlay.
      if (!occluding_rect.Contains(curr_surface_damage)) {
        root_damage_rect.Union(curr_surface_damage);
      }
    } else {
      // |surface_damage_rect_list| is ordered such that from here on the
      // |display_rect| for the overlay will act as an occluder for damage
      // after.
      if (is_opaque) {
        occluding_rect = display_rect;
      }
    }
  }
  return root_damage_rect;
}
}  // namespace

// Exclude overlay damage from the root damage when possible. In the steady
// state the overlay damage is always removed but transitions can require us to
// apply damage for the entire display size of the overlay. Underlays need to
// provide transition damage on both promotion and demotion as in both cases
// they need to change the primary plane (underlays need a primary plane black
// transparent quad). Overlays only need to produce transition damage on
// demotion as they do not use the primary plane during promoted phase.
void OverlayProcessorUsingStrategy::UpdateDamageRect(
    OverlayCandidateList* candidates,
    SurfaceDamageRectList* surface_damage_rect_list,
    const QuadList* quad_list,
    gfx::Rect* damage_rect) {
  // TODO(khaslett): This code only supports one overlay candidate, but we'll
  // check against `max_overlays_considered_` so we can still try the
  // UseMultipleOverlays feature without hitting DCHECKS.
  // To support multiple overlays one would need to track the difference set of
  // overlays between frames.
  DCHECK_LE(candidates->size(), static_cast<size_t>(max_overlays_considered_));

  gfx::Rect this_frame_overlay_rect;
  bool has_mask_filter = false;
  bool is_opaque_overlay = false;
  bool is_underlay = false;
  uint32_t exclude_overlay_index = OverlayCandidate::kInvalidDamageIndex;

  for (const OverlayCandidate& overlay : *candidates) {
    this_frame_overlay_rect = GetOverlayDamageRectForOutputSurface(overlay);
    has_mask_filter = overlay.has_mask_filter;
    if (overlay.plane_z_order >= 0) {
      // If an overlay candidate comes from output surface, its z-order should
      // be 0.
      overlay_damage_rect_.Union(this_frame_overlay_rect);
      is_opaque_overlay = overlay.is_opaque;
      exclude_overlay_index = overlay.overlay_damage_index;
    } else {
      // Underlay candidate is assumed to be opaque.
      is_opaque_overlay = true;
      is_underlay = true;
      exclude_overlay_index = overlay.overlay_damage_index;
    }
  }

  // Removes all damage from this overlay and occluded surface damages.
  *damage_rect = ComputeDamageExcludingIndex(
      exclude_overlay_index, surface_damage_rect_list, *damage_rect,
      this_frame_overlay_rect, is_opaque_overlay, is_underlay);

  // Drawing on the overlay_rect usually occurs on a different plane, but we
  // still need to damage the overlay_rect when certain changes occur from one
  // frame to the next.  https://crbug.com/875879  https://crbug.com/1107460
  if ((!previous_is_underlay && is_underlay) ||
      has_mask_filter != previous_has_mask_filter_ ||
      this_frame_overlay_rect != previous_frame_overlay_rect_) {
    damage_rect->Union(previous_frame_overlay_rect_);

    // We need to make sure that when we transition to an underlay we damage the
    // region where the underlay will be positioned. This is because a
    // black transparent hole is made for the underlay to show through
    // but its possible that the damage for this quad is less than the
    // complete size of the underlay.  https://crbug.com/1130733
    // Also a non-opaque overlay must damage the primary plane as we might be
    // able see through the overlay to the primary plane.
    // https://buganizer.corp.google.com/issues/192294199
    if (is_underlay || !is_opaque_overlay) {
      damage_rect->Union(this_frame_overlay_rect);
    }
  }

  // Record the first candidate.
  if (candidates->size() > 0 && (*candidates)[0].plane_z_order != 0) {
    RecordOverlayDamageRectHistograms(
        (*candidates)[0].plane_z_order > 0,
        (*candidates)[0].damage_area_estimate != 0, damage_rect->IsEmpty());
  }

  previous_frame_overlay_rect_ = this_frame_overlay_rect;
  previous_has_mask_filter_ = has_mask_filter;
  previous_is_underlay = is_underlay;
}

void OverlayProcessorUsingStrategy::AdjustOutputSurfaceOverlay(
    absl::optional<OutputSurfaceOverlayPlane>* output_surface_plane) {
  if (!output_surface_plane || !output_surface_plane->has_value())
    return;

  // If the overlay candidates cover the entire screen, the
  // |output_surface_plane| could be removed.
  if (last_successful_strategy_ &&
      last_successful_strategy_->RemoveOutputSurfaceAsOverlay())
    output_surface_plane->reset();
}

bool OverlayProcessorUsingStrategy::AttemptWithStrategies(
    const skia::Matrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidates,
    std::vector<gfx::Rect>* content_bounds) {
  last_successful_strategy_ = nullptr;
  for (const auto& strategy : strategies_) {
    if (strategy->Attempt(output_color_matrix, render_pass_backdrop_filters,
                          resource_provider, render_pass_list,
                          surface_damage_rect_list, primary_plane, candidates,
                          content_bounds)) {
      // This function is used by underlay strategy to mark the primary plane as
      // enable_blending.
      strategy->AdjustOutputSurfaceOverlay(primary_plane);
      LogStrategyEnumUMA(strategy->GetUMAEnum());
      last_successful_strategy_ = strategy.get();
      return true;
    }
  }

  LogStrategyEnumUMA(OverlayStrategy::kNoStrategyUsed);
  return false;
}

void OverlayProcessorUsingStrategy::SortProposedOverlayCandidatesPrioritized(
    Strategy::OverlayProposedCandidateList* proposed_candidates) {
  // Removes trackers for candidates that are no longer being rendered.
  for (auto it = tracked_candidates_.begin();
       it != tracked_candidates_.end();) {
    if (it->second.IsAbsent()) {
      it = tracked_candidates_.erase(it);
    } else {
      ++it;
    }
  }

  // This loop fills in data for the heuristic sort and thresholds candidates.
  for (auto it = proposed_candidates->begin();
       it != proposed_candidates->end();) {
    auto key = ToProposeKey(*it);
    // If no tracking exists we create a new one here.
    auto& track_data = tracked_candidates_[key];
    DBG_DRAW_TEXT_OPT(
        "candidate.surface.id", DBG_OPT_GREEN,
        it->candidate.display_rect.origin(),
        base::StringPrintf("%X , %d", key.tracking_id, key.strategy_id)
            .c_str());
    DBG_DRAW_TEXT_OPT(
        "candidate.mean.damage", DBG_OPT_GREEN,
        it->candidate.display_rect.origin(),
        base::StringPrintf(
            " %f, %f %d", track_data.MeanFrameRatioRate(tracker_config_),
            track_data.GetDamageRatioRate(),
            static_cast<int>(it->candidate.resource_id.value())));
    const auto display_area = it->candidate.display_rect.size().GetArea();
    // The |force_update| case is where we have damage and a damage index but
    // there are no changes in the |resource_id|. This is only known to occur
    // for low latency surfaces (inking like in the google keeps application).
    const bool force_update = it->candidate.overlay_damage_index !=
                                  OverlayCandidate::kInvalidDamageIndex &&
                              it->candidate.damage_area_estimate != 0;
    track_data.AddRecord(
        frame_sequence_number_,
        static_cast<float>(it->candidate.damage_area_estimate) / display_area,
        it->candidate.resource_id, tracker_config_, force_update);
    // Here a series of criteria are considered for wholesale rejection of a
    // candidate. The rational for rejection is usually power improvements but
    // this can indirectly reallocate limited overlay resources to another
    // candidate.
    bool passes_min_threshold =
        ((track_data.IsActivelyChanging(frame_sequence_number_,
                                        tracker_config_) ||
          !prioritization_config_.changing_threshold) &&
         (track_data.GetModeledPowerGain(frame_sequence_number_,
                                         tracker_config_, display_area) >= 0 ||
          !prioritization_config_.damage_rate_threshold));

    if (it->candidate.requires_overlay || passes_min_threshold) {
      it->relative_power_gain = track_data.GetModeledPowerGain(
          frame_sequence_number_, tracker_config_, display_area);
      ++it;
    } else {
      // We 'Reset' rather than delete the |track_data| because this candidate
      // will still be present next frame.
      track_data.Reset();
      it = proposed_candidates->erase(it);
    }
  }

  // Heuristic sorting:
  // The stable sort of proposed candidates will not change the prioritized
  // order of candidates that have equal sort. What this means is that in a
  // situation where there are multiple candidates with identical rects we will
  // output a sort that respects the original strategies order. An example of
  // this would be the single_on_top strategy coming before the underlay
  // strategy for a overlay candidate that has zero occlusion. This sort
  // function must provide weak ordering.
  auto prio_config = prioritization_config_;
  std::stable_sort(
      proposed_candidates->begin(), proposed_candidates->end(),
      [prio_config](const auto& a, const auto& b) {
        // DRM/CDM HW overlay required:
        // This comparison is for correctness over performance reasons. Some
        // candidates must be an HW overlay to function. If both require an HW
        // overlay we leave them in order so the topmost one gets the overlay.
        if (a.candidate.requires_overlay || b.candidate.requires_overlay) {
          return a.candidate.requires_overlay && !b.candidate.requires_overlay;
        }

        // Opaque Power Metric:
        // |relative_power_gain| is computed in the tracker for each overlay
        // candidate and being proportional to power saved is directly
        // comparable.
        if (prio_config.power_gain_sort) {
          if (a.relative_power_gain != b.relative_power_gain) {
            return a.relative_power_gain > b.relative_power_gain;
          }
        }

        return false;
      });
}

bool OverlayProcessorUsingStrategy::AttemptWithStrategiesPrioritized(
    const skia::Matrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidates,
    std::vector<gfx::Rect>* content_bounds,
    gfx::Rect* incoming_damage) {
  last_successful_strategy_ = nullptr;
  Strategy::OverlayProposedCandidateList proposed_candidates;
  for (const auto& strategy : strategies_) {
    strategy->ProposePrioritized(
        output_color_matrix, render_pass_backdrop_filters, resource_provider,
        render_pass_list, surface_damage_rect_list, primary_plane,
        &proposed_candidates, content_bounds);
  }

  size_t num_proposed_pre_sort = proposed_candidates.size();
  UMA_HISTOGRAM_COUNTS_1000(
      "Viz.DisplayCompositor.OverlayNumProposedCandidates",
      num_proposed_pre_sort);

  SortProposedOverlayCandidatesPrioritized(&proposed_candidates);

  if (ShouldAttemptMultipleOverlays(proposed_candidates)) {
    auto* render_pass = render_pass_list->back().get();
    return AttemptMultipleOverlays(proposed_candidates, primary_plane,
                                   render_pass, *candidates);
  }

  bool has_required_overlay = false;
  for (auto&& candidate : proposed_candidates) {
    // Underlays change the material so we save it here to record proper UMA.
    DrawQuad::Material quad_material =
        candidate.strategy->GetUMAEnum() != OverlayStrategy::kUnknown
            ? candidate.quad_iter->material
            : DrawQuad::Material::kInvalid;
    if (candidate.candidate.requires_overlay)
      has_required_overlay = true;

    bool used_overlay = candidate.strategy->AttemptPrioritized(
        output_color_matrix, render_pass_backdrop_filters, resource_provider,
        render_pass_list, surface_damage_rect_list, primary_plane, candidates,
        content_bounds, candidate);
    if (!used_overlay && candidate.candidate.requires_overlay) {
      // Check if we likely failed due to scaling capabilities, and if so, try
      // to adjust things to make it work. We do this by tracking what scale
      // factors succeed for downscaling, and then if we hit a failure case we
      // decrease the amount iteratively until it succeeds. We then cache that
      // information as hints to speed up the process next time around.
      // When we scale less, we then clip instead in order to fit into the
      // target area.  This is more visually appealing than blacking out the
      // quad since an overlay is required.
      float scale_factor = GetMinScaleFactor(candidate.candidate);
      if (scale_factor < 1.0f) {
        // When we are trying to determine the min allowed downscale, this is
        // the amount we will adjust the factor by for each iteration we
        // attempt.
        constexpr float kScaleAdjust = 0.05f;
        gfx::RectF org_src_rect = gfx::ScaleRect(
            candidate.candidate.uv_rect,
            candidate.candidate.resource_size_in_pixels.width(),
            candidate.candidate.resource_size_in_pixels.height());
        for (float new_scale_factor = std::min(
                 min_working_scale_,
                 std::max(max_failed_scale_, scale_factor) + kScaleAdjust);
             new_scale_factor < 1.0f; new_scale_factor += kScaleAdjust) {
          float zoom_scale = new_scale_factor / scale_factor;
          ScaleCandidateSrcRect(org_src_rect, zoom_scale, &candidate.candidate);
          if (candidate.strategy->AttemptPrioritized(
                  output_color_matrix, render_pass_backdrop_filters,
                  resource_provider, render_pass_list, surface_damage_rect_list,
                  primary_plane, candidates, content_bounds, candidate)) {
            used_overlay = true;
            break;
          } else {
            UpdateDownscalingCapabilities(new_scale_factor, /*success=*/false);
          }
        }
      }
    }
    if (used_overlay) {
      // This function is used by underlay strategy to mark the primary plane as
      // enable_blending.
      candidate.strategy->AdjustOutputSurfaceOverlay(primary_plane);
      LogStrategyEnumUMA(candidate.strategy->GetUMAEnum());
      last_successful_strategy_ = candidate.strategy;
      OnOverlaySwitchUMA(ToProposeKey(candidate));
      UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayQuadMaterial",
                                quad_material);
      if (candidate.candidate.requires_overlay) {
        // Track how much we can downscale successfully.
        float scale_factor = GetMinScaleFactor(candidate.candidate);
        if (scale_factor < 1.0f) {
          UpdateDownscalingCapabilities(scale_factor, /*success=*/true);
        }
      }
      RegisterOverlayRequirement(has_required_overlay);
      return true;
    }
  }
  RegisterOverlayRequirement(has_required_overlay);

  if (proposed_candidates.size() == 0) {
    LogStrategyEnumUMA(num_proposed_pre_sort != 0
                           ? OverlayStrategy::kNoStrategyFailMin
                           : OverlayStrategy::kNoStrategyUsed);
  } else {
    LogStrategyEnumUMA(OverlayStrategy::kNoStrategyAllFail);
  }
  OnOverlaySwitchUMA(ProposedCandidateKey());
  return false;
}

bool OverlayProcessorUsingStrategy::ShouldAttemptMultipleOverlays(
    const Strategy::OverlayProposedCandidateList& sorted_candidates) {
  if (max_overlays_considered_ <= 1) {
    return false;
  }

  for (auto& proposed : sorted_candidates) {
    // When candidates that require overlays fail, they get retried with
    // different scale factors. This becomes complicated when using multiple
    // overlays at once so we won't attempt multiple in that case.
    if (proposed.candidate.requires_overlay) {
      return false;
    }
    // Using multiple overlays only makes sense with SingleOnTop and Underlay
    // strategies.
    OverlayStrategy type = proposed.strategy->GetUMAEnum();
    if (type != OverlayStrategy::kSingleOnTop &&
        type != OverlayStrategy::kUnderlay) {
      return false;
    }
  }

  return true;
}

bool OverlayProcessorUsingStrategy::AttemptMultipleOverlays(
    const Strategy::OverlayProposedCandidateList& sorted_candidates,
    OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    AggregatedRenderPass* render_pass,
    OverlayCandidateList& candidates) {
  int max_overlays_possible = std::min(
      max_overlays_considered_, static_cast<int>(sorted_candidates.size()));

  Strategy::OverlayProposedCandidateList test_candidates;
  // Reserve max possible overlays so iterators remain stable while we insert
  // candidates.
  test_candidates.reserve(max_overlays_possible);
  bool testing_underlay = false;
  // We'll keep track of the underlays that we're testing so we can assign their
  // `plane_z_order`s based on their order in the QuadList.
  std::vector<Strategy::OverlayProposedCandidateList::iterator> underlay_iters;
  // Used to prevent testing multiple candidates representing the same DrawQuad.
  std::set<size_t> used_quad_indices;

  for (auto& cand : sorted_candidates) {
    // Skip candidates whose quads have already been added to the test list. A
    // quad could have an on top and an underlay candidate.
    bool inserted = used_quad_indices.insert(cand.quad_iter.index()).second;
    if (!inserted) {
      continue;
    }
    test_candidates.push_back(cand);

    switch (cand.strategy->GetUMAEnum()) {
      case OverlayStrategy::kSingleOnTop:
        // Ordering of on top candidates doesn't matter (they can't overlap), so
        // they can all have z = 1.
        test_candidates.back().candidate.plane_z_order = 1;
        break;
      case OverlayStrategy::kUnderlay:
        testing_underlay = true;
        underlay_iters.push_back(test_candidates.end() - 1);
        break;
      default:
        // Unsupported strategy type.
        NOTREACHED();
    }
    if (test_candidates.size() == static_cast<size_t>(max_overlays_possible)) {
      break;
    }
  }

  // We don't sort the actual items in `test_candidates` here in order to
  // maintain the power-gain sorted order.
  AssignUnderlayZOrders(underlay_iters);

  candidates.reserve(test_candidates.size());
  for (auto& proposed_candidate : test_candidates) {
    candidates.push_back(proposed_candidate.candidate);
  }

  if (!testing_underlay || !primary_plane) {
    CheckOverlaySupport(primary_plane, &candidates);
  } else {
    Strategy::PrimaryPlane new_plane_candidate(*primary_plane);
    new_plane_candidate.enable_blending = true;
    // Check for support.
    CheckOverlaySupport(&new_plane_candidate, &candidates);
  }

  bool underlay_used = false;
  auto cand_it = candidates.begin();
  auto test_it = test_candidates.begin();
  while (cand_it != candidates.end()) {
    // Update the test candidates so we can use EraseIf below.
    test_it->candidate.overlay_handled = cand_it->overlay_handled;
    if (cand_it->overlay_handled && cand_it->plane_z_order < 0) {
      underlay_used = true;
    }
    cand_it++;
    test_it++;
  }
  // Remove failed candidates
  base::EraseIf(candidates, [](auto& cand) { return !cand.overlay_handled; });
  base::EraseIf(test_candidates, [](auto& proposed) -> bool {
    return !proposed.candidate.overlay_handled;
  });

  if (candidates.empty()) {
    return false;
  }

  if (underlay_used && primary_plane) {
    // Using underlays means the primary plane needs blending enabled.
    primary_plane->enable_blending = true;
  }

  // Sort test candidates in reverse order so we can commit them from back to
  // front. This makes sure none of the quad iterators are invalidated when some
  // are removed from the QuadList as they're committed.
  //
  // TODO(khaslett): Remove this hacky workaround. Instead of erasing quads we
  // could probably replace them with solid colour quads or make them invisible
  // instead.
  std::sort(test_candidates.begin(), test_candidates.end(),
            [](const Strategy::OverlayProposedCandidate& c1,
               const Strategy::OverlayProposedCandidate& c2) -> bool {
              return c1.quad_iter.index() > c2.quad_iter.index();
            });
  // Commit successful candidates.
  for (auto& test_candidate : test_candidates) {
    test_candidate.strategy->CommitCandidate(test_candidate, render_pass);
  }

  return true;
}

void OverlayProcessorUsingStrategy::AssignUnderlayZOrders(
    std::vector<Strategy::OverlayProposedCandidateList::iterator>&
        underlay_iters) {
  // Sort the underlay iterators by DrawQuad order, frontmost first.
  std::sort(
      underlay_iters.begin(), underlay_iters.end(),
      [](const Strategy::OverlayProposedCandidateList::iterator& c1,
         const Strategy::OverlayProposedCandidateList::iterator& c2) -> bool {
        return c1->quad_iter.index() < c2->quad_iter.index();
      });
  // Assign underlay candidate plane_z_orders based on DrawQuad order.
  int underlay_z_order = -1;
  for (auto& it : underlay_iters) {
    it->candidate.plane_z_order = underlay_z_order--;
  }
}

gfx::Rect OverlayProcessorUsingStrategy::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& overlay) const {
  return ToEnclosedRect(overlay.display_rect);
}

void OverlayProcessorUsingStrategy::OnOverlaySwitchUMA(
    OverlayProcessorUsingStrategy::ProposedCandidateKey overlay_tracking_id) {
  auto curr_tick = base::TimeTicks::Now();
  if (!(prev_overlay_tracking_id_ == overlay_tracking_id)) {
    prev_overlay_tracking_id_ = overlay_tracking_id;
    UMA_HISTOGRAM_TIMES("Viz.DisplayCompositor.OverlaySwitchInterval",
                        curr_tick - last_time_interval_switch_overlay_tick_);
    last_time_interval_switch_overlay_tick_ = curr_tick;
  }
}

void OverlayProcessorUsingStrategy::UpdateDownscalingCapabilities(
    float scale_factor,
    bool success) {
  if (success) {
    // Adjust the working bound up by this amount so we don't end up with
    // floating point errors based on the true minimum that actually
    // works.
    constexpr float kScaleBoundsTolerance = 0.001f;
    min_working_scale_ =
        std::min(scale_factor + kScaleBoundsTolerance, min_working_scale_);
    // If something worked that failed before, reset the known maximum for
    // failure.
    if (min_working_scale_ < max_failed_scale_)
      max_failed_scale_ = 0.0f;
    return;
  }

  max_failed_scale_ = std::max(max_failed_scale_, scale_factor);
  // If something failed that worked before, reset the known working
  // minimum.
  if (max_failed_scale_ > min_working_scale_)
    min_working_scale_ = 1.0f;
}

void OverlayProcessorUsingStrategy::LogCheckOverlaySupportMetrics() {
  UMA_HISTOGRAM_COUNTS_100(
      "Compositing.Display.OverlayProcessorUsingStrategy."
      "CheckOverlaySupportCallCount",
      check_overlay_support_call_count_);
  check_overlay_support_call_count_ = 0;
}

}  // namespace viz
