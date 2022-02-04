// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_USING_STRATEGY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_USING_STRATEGY_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_candidate_temporal_tracker.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"

namespace cc {
class DisplayResourceProvider;
}

namespace viz {
// OverlayProcessor subclass that goes through a list of strategies to determine
// overlay candidates. THis is used by Android and Ozone platforms.
class VIZ_SERVICE_EXPORT OverlayProcessorUsingStrategy
    : public OverlayProcessorInterface {
 public:
  using CandidateList = OverlayCandidateList;
  // TODO(weiliangc): Move it to an external class.
  class VIZ_SERVICE_EXPORT Strategy {
   public:
    class VIZ_SERVICE_EXPORT OverlayProposedCandidate {
     public:
      OverlayProposedCandidate(QuadList::Iterator it,
                               OverlayCandidate overlay_candidate,
                               Strategy* overlay_strategy)
          : quad_iter(it),
            candidate(overlay_candidate),
            strategy(overlay_strategy) {}

      // A iterator in the vector of quads.
      QuadList::Iterator quad_iter;
      // This is needed to sort candidates based on DrawQuad order.
      size_t quad_index;
      OverlayCandidate candidate;
      raw_ptr<Strategy> strategy = nullptr;

      // heuristic sort element
      int relative_power_gain = 0;
    };

    using OverlayProposedCandidateList = std::vector<OverlayProposedCandidate>;

    virtual ~Strategy() = default;
    using PrimaryPlane = OverlayProcessorInterface::OutputSurfaceOverlayPlane;

    // Returns false if the strategy cannot be made to work with the
    // current set of render passes. Returns true if the strategy was successful
    // and adds any additional passes necessary to represent overlays to
    // |render_pass_list|. Most strategies should look at the primary
    // RenderPass, the last element.
    virtual bool Attempt(
        const skia::Matrix44& output_color_matrix,
        const FilterOperationsMap& render_pass_backdrop_filters,
        DisplayResourceProvider* resource_provider,
        AggregatedRenderPassList* render_pass_list,
        SurfaceDamageRectList* surface_damage_rect_list,
        const PrimaryPlane* primary_plane,
        OverlayCandidateList* candidates,
        std::vector<gfx::Rect>* content_bounds) = 0;

    // Appends all legitimate overlay candidates to the list |candidates|
    // for this strategy.  It is very important to note that this function
    // should not attempt a specific candidate it should merely identify them
    // and save the necessary data required to for a later attempt.
    virtual void ProposePrioritized(
        const skia::Matrix44& output_color_matrix,
        const FilterOperationsMap& render_pass_backdrop_filters,
        DisplayResourceProvider* resource_provider,
        AggregatedRenderPassList* render_pass_list,
        SurfaceDamageRectList* surface_damage_rect_list,
        const PrimaryPlane* primary_plane,
        OverlayProposedCandidateList* candidates,
        std::vector<gfx::Rect>* content_bounds) = 0;

    // Returns false if the specific |proposed_candidate| cannot be made to work
    // for this strategy with the current set of render passes. Returns true if
    // the strategy was successful and adds any additional passes necessary to
    // represent overlays to |render_pass_list|. Most strategies should look at
    // the primary RenderPass, the last element.
    virtual bool AttemptPrioritized(
        const skia::Matrix44& output_color_matrix,
        const FilterOperationsMap& render_pass_backdrop_filters,
        DisplayResourceProvider* resource_provider,
        AggregatedRenderPassList* render_pass_list,
        SurfaceDamageRectList* surface_damage_rect_list,
        const PrimaryPlane* primary_plane,
        OverlayCandidateList* candidates,
        std::vector<gfx::Rect>* content_bounds,
        const OverlayProposedCandidate& proposed_candidate) = 0;

    // Commits to using the proposed candidate by updating |render_pass| as
    // appropriate when this candidate is presented in an overlay plane.
    virtual void CommitCandidate(
        const OverlayProposedCandidate& proposed_candidate,
        AggregatedRenderPass* render_pass) = 0;

    // Currently this is only overridden by the Underlay strategy: the underlay
    // strategy needs to enable blending for the primary plane in order to show
    // content underneath.
    virtual void AdjustOutputSurfaceOverlay(
        OutputSurfaceOverlayPlane* output_surface_plane) {}

    // Currently this is only overridden by the Fullscreen strategy: the
    // fullscreen strategy covers the entire screen and there is no need to use
    // the primary plane.
    virtual bool RemoveOutputSurfaceAsOverlay();

    virtual OverlayStrategy GetUMAEnum() const;

    // Does a null-check on |primary_plane| and returns it's |display_rect|
    // member if non-null and an empty gfx::RectF otherwise.
    gfx::RectF GetPrimaryPlaneDisplayRect(const PrimaryPlane* primary_plane);
  };
  using StrategyList = std::vector<std::unique_ptr<Strategy>>;

  OverlayProcessorUsingStrategy(const OverlayProcessorUsingStrategy&) = delete;
  OverlayProcessorUsingStrategy& operator=(
      const OverlayProcessorUsingStrategy&) = delete;

  ~OverlayProcessorUsingStrategy() override;

  gfx::Rect GetPreviousFrameOverlaysBoundingRect() const final;
  gfx::Rect GetAndResetOverlayDamage() final;

  // Override OverlayProcessor.
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  void SetViewportSize(const gfx::Size& size) override {}
  void SetFrameSequenceNumber(uint64_t frame_sequence_number_) override;
  // Attempt to replace quads from the specified root render pass with overlays.
  // This must be called every frame.
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_passes,
      const skia::Matrix44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds)
      // TODO(petermcneeley) : Restore to "final" once
      // |OverlayProcessorDelegated| has been reintegrated into
      // |OverlayProcessorOzone|.
      override;

  // This function takes a pointer to the absl::optional instance so the
  // instance can be reset. When overlay strategy covers the entire output
  // surface, we no longer need the output surface as a separate overlay. This
  // is also used by SurfaceControl to adjust rotation.
  // TODO(weiliangc): Internalize the |output_surface_plane| inside the overlay
  // processor.
  void AdjustOutputSurfaceOverlay(
      absl::optional<OutputSurfaceOverlayPlane>* output_surface_plane) override;

  OverlayProcessorUsingStrategy();

  // A list of possible overlay candidates is presented to this function.
  // The expected result is that those candidates that can be in a separate
  // plane are marked with |overlay_handled| set to true, otherwise they are
  // to be traditionally composited. Candidates with |overlay_handled| set to
  // true must also have their |display_rect| converted to integer
  // coordinates if necessary.
  void CheckOverlaySupport(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidate_list);

  // This should be called during overlay processing to register whether or not
  // there is a candidate that requires an overlay so that the manager can allow
  // the overlay on the display with the requirement only.
  virtual void RegisterOverlayRequirement(bool requires_overlay) {}

 protected:
  virtual gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& overlay) const;

  StrategyList strategies_;
  raw_ptr<Strategy> last_successful_strategy_ = nullptr;

  gfx::Rect overlay_damage_rect_;
  bool previous_is_underlay = false;
  bool previous_has_mask_filter_ = false;
  gfx::Rect previous_frame_overlay_rect_;

  struct OverlayPrioritizationConfig {
    // Threshold criteria required for a proposed candidate to be considered for
    // overlay promotion
    bool changing_threshold = true;
    bool damage_rate_threshold = true;

    // Sorting criteria that determines the relative order of consideration for
    // a overlay candidate.
    bool power_gain_sort = true;
  };

  OverlayPrioritizationConfig prioritization_config_;
  OverlayCandidateTemporalTracker::Config tracker_config_;

 private:
  // The platform specific implementation to check overlay support that will be
  // called by `CheckOverlaySupport()`.
  virtual void CheckOverlaySupportImpl(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidate_list) = 0;

  // Update |damage_rect| by removing damage caused by |candidates|.
  void UpdateDamageRect(OverlayCandidateList* candidates,
                        SurfaceDamageRectList* surface_damage_rect_list,
                        const QuadList* quad_list,
                        gfx::Rect* damage_rect);

  // Iterate through a list of strategies and attempt to overlay with each.
  // Returns true if one of the attempts is successful. Has to be called after
  // InitializeStrategies(). A |primary_plane| represents the output surface's
  // buffer that comes from |BufferQueue|. It is passed in here so it could be
  // pass through to hardware through CheckOverlaySupport. It is not passed
  // through as a const member because the underlay strategy changes the
  // |primary_plane|'s blending setting.
  bool AttemptWithStrategies(
      const skia::Matrix44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds);

  // Iterate through a list of strategies and attempt to overlay with each.
  // Returns true if one of the attempts is successful. Has to be called after
  // InitializeStrategies(). A |primary_plane| represents the output surface's
  // buffer that comes from |BufferQueue|. It is passed in here so it could be
  // pass through to hardware through CheckOverlaySupport. It is not passed
  // through as a const member because the underlay strategy changes the
  // |primary_plane|'s blending setting.
  bool AttemptWithStrategiesPrioritized(
      const skia::Matrix44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds,
      gfx::Rect* incoming_damage);

  // Determines if we should attempt multiple overlays. This is based on
  // `max_overlays_considered_`, the strategies proposed, and if any of the
  // candidates require an overlay.
  bool ShouldAttemptMultipleOverlays(
      const Strategy::OverlayProposedCandidateList& sorted_candidates);

  // Attempts to promote multiple candidates to overlays. Returns a boolean
  // indicating if any of the attempted candidates were successfully promoted to
  // overlays.
  //
  // TODO(khaslett): Write unit tests for this function before launching
  // UseMultipleOverlays feature.
  bool AttemptMultipleOverlays(
      const Strategy::OverlayProposedCandidateList& sorted_candidates,
      OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      AggregatedRenderPass* render_pass,
      OverlayCandidateList& candidates);

  // Assigns `plane_z_order`s to the proposed underlay candidates based on their
  // DrawQuad orderings.
  //
  // TODO(khaslett): Write unit tests for this function before launching
  // UseMultipleOverlays feature.
  void AssignUnderlayZOrders(
      std::vector<Strategy::OverlayProposedCandidateList::iterator>&
          underlay_iters);

  // This function reorders and removes |proposed_candidates| based on a
  // heuristic designed to maximize the effectiveness of the limited number
  // of Hardware overlays. Effectiveness here is primarily about power and
  // secondarily about of performance.
  void SortProposedOverlayCandidatesPrioritized(
      Strategy::OverlayProposedCandidateList* proposed_candidates);

  // Used by Android pre-SurfaceControl to notify promotion hints.
  virtual void NotifyOverlayPromotion(
      DisplayResourceProvider* display_resource_provider,
      const OverlayCandidateList& candidate_list,
      const QuadList& quad_list);

  // Used to update |min_working_scale_| and |max_failed_scale_|. |scale_factor|
  // should be the src->dst scaling amount that is < 1.0f and |success| should
  // be whether that scaling worked or not.
  void UpdateDownscalingCapabilities(float scale_factor, bool success);

  // Logs the number of times CheckOverlaySupport was called this frame, and
  // resets the counter to 0.
  void LogCheckOverlaySupportMetrics();

  struct ProposedCandidateKey {
    OverlayCandidate::TrackingId tracking_id;
    OverlayStrategy strategy_id = OverlayStrategy::kUnknown;

    bool operator==(const ProposedCandidateKey& other) const {
      return (tracking_id == other.tracking_id &&
              strategy_id == other.strategy_id);
    }
  };

  struct ProposedCandidateKeyHasher {
    std::size_t operator()(const ProposedCandidateKey& k) const {
      return base::Hash(&k, sizeof(k));
    }
  };

  static ProposedCandidateKey ToProposeKey(
      const Strategy::OverlayProposedCandidate& proposed);

  const int max_overlays_considered_;

  std::unordered_map<ProposedCandidateKey,
                     OverlayCandidateTemporalTracker,
                     ProposedCandidateKeyHasher>
      tracked_candidates_;

  // These variables are used only for UMA purposes.
  void OnOverlaySwitchUMA(ProposedCandidateKey overlay_tracking_key);
  base::TimeTicks last_time_interval_switch_overlay_tick_;
  ProposedCandidateKey prev_overlay_tracking_id_;
  uint64_t frame_sequence_number_ = 0;
  int check_overlay_support_call_count_ = 0;

  // These values are used for tracking how much we can downscale with overlays
  // and is used for when we require an overlay so we can determine how much we
  // can downscale without failing.
  float min_working_scale_ = 1.0f;
  float max_failed_scale_ = 0.0f;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_USING_STRATEGY_H_
