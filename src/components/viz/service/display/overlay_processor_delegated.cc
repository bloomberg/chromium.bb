// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_delegated.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/overlay_strategy_fullscreen.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/display/overlay_strategy_underlay_cast.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/ozone_platform.h"

namespace {
DBG_FLAG_FBOOL("delegated.fd.usage", usage_every_frame)

void RecordFDUsageUMA() {
  static uint64_t sReportUsageFrameCounter = 0;
  sReportUsageFrameCounter++;
  constexpr uint32_t kReportEveryNFrames = 60 * 60 * 5;
  if (((sReportUsageFrameCounter % kReportEveryNFrames) != 0) &&
      !usage_every_frame()) {
    return;
  }

  base::TimeDelta delta_time_taken;
  int fd_max;
  int active_fd_count;
  int rlim_cur;

  if (!viz::GatherFDStats(&delta_time_taken, &fd_max, &active_fd_count,
                          &rlim_cur))
    return;

  static constexpr base::TimeDelta kHistogramMinTime = base::Microseconds(5);
  static constexpr base::TimeDelta kHistogramMaxTime = base::Milliseconds(10);
  static constexpr int kHistogramTimeBuckets = 50;
  int percentage_usage_int = (active_fd_count * 100) / fd_max;
  UMA_HISTOGRAM_PERCENTAGE("Viz.FileDescriptorTracking.PercentageUsed",
                           percentage_usage_int);
  UMA_HISTOGRAM_COUNTS_100000("Viz.FileDescriptorTracking.NumActive",
                              active_fd_count);
  UMA_HISTOGRAM_COUNTS_100000("Viz.FileDescriptorTracking.NumSoftMax",
                              rlim_cur);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Viz.FileDescriptorTracking.TimeToCompute", delta_time_taken,
      kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);

  DBG_LOG("delegated.fd.usage", "FD usage: %d / %d - time us: %f",
          active_fd_count, fd_max, delta_time_taken.InMicrosecondsF());
}
}  // namespace

namespace viz {

OverlayProcessorDelegated::OverlayProcessorDelegated(
    std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates,
    std::vector<OverlayStrategy> available_strategies,
    gpu::SharedImageInterface* shared_image_interface)
    : OverlayProcessorOzone(std::move(overlay_candidates),
                            available_strategies,
                            shared_image_interface) {
  // TODO(msisov, petermcneeley): remove this once Wayland uses only delegated
  // context. May be null in tests.
  if (ui::OzonePlatform::GetInstance()->GetOverlayManager())
    ui::OzonePlatform::GetInstance()
        ->GetOverlayManager()
        ->SetContextDelegated();
}

OverlayProcessorDelegated::~OverlayProcessorDelegated() = default;

bool OverlayProcessorDelegated::DisableSplittingQuads() const {
  // If there is quads to split these will happen delegee side.
  return true;
}

constexpr size_t kTooManyQuads = 64;

DBG_FLAG_FBOOL("delegated.disable.delegation", disable_delegation)

bool OverlayProcessorDelegated::AttemptWithStrategies(
    const skia::Matrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidates,
    std::vector<gfx::Rect>* content_bounds) {
  DCHECK(candidates->empty());
  auto* render_pass = render_pass_list->back().get();
  QuadList* quad_list = &render_pass->quad_list;
  constexpr bool is_delegated_context = true;
  delegated_status_ = DelegationStatus::kCompositedOther;

  if (disable_delegation())
    return false;

  if (quad_list->size() >= kTooManyQuads ||
      !render_pass_backdrop_filters.empty())
    return false;

  std::vector<QuadList::Iterator> candidate_quads;
  int num_quads_skipped = 0;
  for (auto it = quad_list->begin(); it != quad_list->end(); ++it) {
    OverlayCandidate candidate;
    auto& transform = it->shared_quad_state->quad_to_target_transform;
    auto display_rect = gfx::RectF(it->rect);
    transform.TransformRect(&display_rect);
    DBG_DRAW_TEXT(
        "delegated.overlay.type",
        gfx::Vector2dF(display_rect.origin().x(), display_rect.origin().y()),
        base::StringPrintf("m=%d rid=%d", static_cast<int>(it->material),
                           it->resources.begin()->value()));
    auto candidate_status = OverlayCandidate::FromDrawQuad(
        resource_provider, surface_damage_rect_list, output_color_matrix, *it,
        GetPrimaryPlaneDisplayRect(primary_plane), &candidate,
        is_delegated_context);
    if (candidate_status == OverlayCandidate::CandidateStatus::kSuccess) {
      if (it->material == DrawQuad::Material::kSolidColor) {
        DBG_DRAW_RECT("delegated.overlay.color", candidate.display_rect);
      } else if (it->material == DrawQuad::Material::kAggregatedRenderPass) {
        DBG_DRAW_RECT("delegated.overlay.aggregated", candidate.display_rect);
      } else {
        DBG_DRAW_RECT("delegated.overlay.candidate", candidate.display_rect);
      }
      candidates->push_back(candidate);
      candidate_quads.push_back(it);
    } else {
      if (candidate_status == OverlayCandidate::CandidateStatus::kFailVisible) {
        // This quad can be intentionally skipped.
        num_quads_skipped++;
      } else {
        DBG_DRAW_RECT("delegated.overlay.failed", display_rect);
      }

      if (candidate_status ==
          OverlayCandidate::CandidateStatus::kFailNotAxisAligned) {
        delegated_status_ = DelegationStatus::kCompositedNotAxisAligned;
      } else if (candidate_status ==
                 OverlayCandidate::CandidateStatus::kFailNotOverlay) {
        delegated_status_ = DelegationStatus::kCompositedNotOverlay;
      }
    }
  }

  if (candidates->empty() ||
      (candidates->size() + num_quads_skipped) != quad_list->size()) {
    candidates->clear();
    return false;
  }

  int curr_plane_order = candidates->size();
  for (auto&& each : *candidates) {
    each.plane_z_order = curr_plane_order--;
  }

  // Check for support.
  this->CheckOverlaySupport(primary_plane, candidates);

  for (auto&& each : *candidates) {
    if (!each.overlay_handled) {
      candidates->clear();
      delegated_status_ = DelegationStatus::kCompositedCheckOverlayFail;
      DBG_DRAW_RECT("delegated.handled.failed", each.display_rect);
      return false;
    }
  }

  // We cannot erase the quads that were handled as overlays because raw
  // pointers of the aggregate draw quads were placed in the |rpdq| member of
  // the |OverlayCandidate|. As keeping with the pattern in
  // overlay_processor_mac we will also set the damage to empty on the
  // successful promotion of all quads.
  delegated_status_ = DelegationStatus::kFullDelegation;
  return true;
}

gfx::RectF OverlayProcessorDelegated::GetPrimaryPlaneDisplayRect(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane) {
  return primary_plane ? primary_plane->display_rect : gfx::RectF();
}

void OverlayProcessorDelegated::ProcessForOverlays(
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
  DCHECK(candidates->empty());
  auto* render_pass = render_passes->back().get();
  bool success = false;
#if !BUILDFLAG(IS_APPLE)
  RecordFDUsageUMA();
#endif

  DBG_DRAW_RECT("delegated.incoming.damage", (*damage_rect));
  for (auto&& each : surface_damage_rect_list) {
    DBG_DRAW_RECT("delegated.surface.damage", each);
  }

  // If we have any copy requests, we can't remove any quads for overlays or
  // CALayers because the framebuffer would be missing the removed quads'
  // contents.
  if (render_pass->copy_requests.empty()) {
    success = AttemptWithStrategies(
        output_color_matrix, render_pass_backdrop_filters, resource_provider,
        render_passes, &surface_damage_rect_list, output_surface_plane,
        candidates, content_bounds);
  }

  DCHECK(candidates->empty() || success);

  if (success) {
    overlay_damage_rect_ = *damage_rect;
    // Save all the damage for the case when we fail delegation.
    previous_frame_overlay_rect_.Union(*damage_rect);
    // All quads handled. Primary plane damage is zero.
    *damage_rect = gfx::Rect();
  } else {
    overlay_damage_rect_ = previous_frame_overlay_rect_;
    // Add in all the damage from all fully delegated frames.
    damage_rect->Union(previous_frame_overlay_rect_);
    previous_frame_overlay_rect_ = gfx::Rect();
  }

  UMA_HISTOGRAM_ENUMERATION("Viz.DelegatedCompositing.Status",
                            delegated_status_);
  DBG_DRAW_RECT("delegated.outgoing.damage", (*damage_rect));

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                 "Scheduled overlay planes", candidates->size());

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                       "DelegatedCompositingStatus", TRACE_EVENT_SCOPE_THREAD,
                       "delegated_status", delegated_status_);
}

void OverlayProcessorDelegated::AdjustOutputSurfaceOverlay(
    absl::optional<OutputSurfaceOverlayPlane>* output_surface_plane) {
  if (!output_surface_plane->has_value())
    return;

  // TODO(https://crbug.com/1224991) : Damage propagation will allow us to
  // remove the primary plan entirely in the case of full delegation.
  // In that case we will do "output_surface_plane->reset()" like the existing
  // fullscreen overlay code.
}

}  // namespace viz
