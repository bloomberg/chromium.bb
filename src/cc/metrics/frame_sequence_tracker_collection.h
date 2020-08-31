// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_FRAME_SEQUENCE_TRACKER_COLLECTION_H_
#define CC_METRICS_FRAME_SEQUENCE_TRACKER_COLLECTION_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_sequence_metrics.h"

namespace gfx {
struct PresentationFeedback;
}

namespace viz {
struct BeginFrameAck;
struct BeginFrameArgs;
}  // namespace viz

namespace cc {
class FrameSequenceTracker;
class CompositorFrameReportingController;
class ThroughputUkmReporter;
class UkmManager;

// Map of kCustom tracker results keyed by a sequence id.
using CustomTrackerResults =
    base::flat_map<int, FrameSequenceMetrics::ThroughputData>;

typedef uint16_t ActiveFrameSequenceTrackers;

// Used for notifying attached FrameSequenceTracker's of begin-frames and
// submitted frames.
class CC_EXPORT FrameSequenceTrackerCollection {
 public:
  FrameSequenceTrackerCollection(
      bool is_single_threaded,
      CompositorFrameReportingController* frame_reporting_controller);
  ~FrameSequenceTrackerCollection();

  FrameSequenceTrackerCollection(const FrameSequenceTrackerCollection&) =
      delete;
  FrameSequenceTrackerCollection& operator=(
      const FrameSequenceTrackerCollection&) = delete;

  // Creates a tracker for the specified sequence-type.
  FrameSequenceMetrics* StartSequence(FrameSequenceTrackerType type);

  // Schedules |tracker| for destruction. This is preferred instead of outright
  // desrtruction of the tracker, since this ensures that the actual tracker
  // instance is destroyed *after* the presentation-feedbacks have been received
  // for all submitted frames.
  void StopSequence(FrameSequenceTrackerType type);

  // Creates a kCustom tracker for the given sequence id. It is an error and
  // DCHECKs if there is already a tracker associated with the sequence id.
  void StartCustomSequence(int sequence_id);

  // Schedules the kCustom tracker representing |sequence_id| for destruction.
  // It is a no-op if there is no tracker associated with the sequence id.
  // Similar to StopSequence above, the tracker instance is destroyed *after*
  // the presentation feedbacks have been received for all submitted frames.
  void StopCustomSequence(int sequence_id);

  // Removes all trackers. This also immediately destroys all trackers that had
  // been scheduled for destruction, even if there are pending
  // presentation-feedbacks. This is typically used if the client no longer
  // expects to receive presentation-feedbacks for the previously submitted
  // frames (e.g. when the gpu process dies).
  void ClearAll();

  // Notifies all trackers of various events.
  void NotifyBeginImplFrame(const viz::BeginFrameArgs& args);
  void NotifyBeginMainFrame(const viz::BeginFrameArgs& args);
  void NotifyMainFrameProcessed(const viz::BeginFrameArgs& args);
  void NotifyImplFrameCausedNoDamage(const viz::BeginFrameAck& ack);
  void NotifyMainFrameCausedNoDamage(const viz::BeginFrameArgs& args);
  void NotifyPauseFrameProduction();
  void NotifySubmitFrame(uint32_t frame_token,
                         bool has_missing_content,
                         const viz::BeginFrameAck& ack,
                         const viz::BeginFrameArgs& origin_args);
  void NotifyFrameEnd(const viz::BeginFrameArgs& args,
                      const viz::BeginFrameArgs& main_args);

  // Note that this notifies the trackers of the presentation-feedbacks, and
  // destroys any tracker that had been scheduled for destruction (using
  // |ScheduleRemoval()|) if it has no more pending frames. Data from non
  // kCustom typed trackers are reported to UMA. Data from kCustom typed
  // trackers are added to |custom_tracker_results_| for caller to pick up.
  void NotifyFramePresented(uint32_t frame_token,
                            const gfx::PresentationFeedback& feedback);

  // Return the type of each active frame tracker, encoded into a 16 bit
  // integer with the bit at each position corresponding to the enum value of
  // each type.
  ActiveFrameSequenceTrackers FrameSequenceTrackerActiveTypes();

  // Reports the accumulated kCustom tracker results and clears it.
  CustomTrackerResults TakeCustomTrackerResults();

  FrameSequenceTracker* GetTrackerForTesting(FrameSequenceTrackerType type);
  FrameSequenceTracker* GetRemovalTrackerForTesting(
      FrameSequenceTrackerType type);

  void SetUkmManager(UkmManager* manager);

  base::Optional<int> current_universal_throughput() {
    return current_universal_throughput_;
  }

 private:
  friend class FrameSequenceTrackerTest;

  void RecreateTrackers(const viz::BeginFrameArgs& args);
  // Destroy the trackers that are ready to be terminated.
  void DestroyTrackers();

  // Adds collected metrics data for |custom_sequence_id| to be picked up via
  // TakeCustomTrackerResults() below.
  void AddCustomTrackerResult(
      int custom_sequence_id,
      FrameSequenceMetrics::ThroughputData throughput_data);

  const bool is_single_threaded_;
  // The callsite can use the type to manipulate the tracker.
  base::flat_map<FrameSequenceTrackerType,
                 std::unique_ptr<FrameSequenceTracker>>
      frame_trackers_;

  // Custom trackers are keyed by a custom sequence id.
  base::flat_map<int, std::unique_ptr<FrameSequenceTracker>>
      custom_frame_trackers_;
  CustomTrackerResults custom_tracker_results_;

  std::vector<std::unique_ptr<FrameSequenceTracker>> removal_trackers_;
  CompositorFrameReportingController* const
      compositor_frame_reporting_controller_;

  // The reporter takes throughput data and connect to UkmManager to report it.
  std::unique_ptr<ThroughputUkmReporter> throughput_ukm_reporter_;

  base::flat_map<
      std::pair<FrameSequenceTrackerType, FrameSequenceMetrics::ThreadType>,
      std::unique_ptr<FrameSequenceMetrics>>
      accumulated_metrics_;
  base::Optional<int> current_universal_throughput_;
};

}  // namespace cc

#endif  // CC_METRICS_FRAME_SEQUENCE_TRACKER_COLLECTION_H_
