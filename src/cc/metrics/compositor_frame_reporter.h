// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_COMPOSITOR_FRAME_REPORTER_H_
#define CC_METRICS_COMPOSITOR_FRAME_REPORTER_H_

#include <bitset>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/cc_export.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/frame_info.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "cc/scheduler/scheduler.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_timing_details.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace viz {
struct FrameTimingDetails;
}

namespace cc {
class FrameSequenceTrackerCollection;
class DroppedFrameCounter;
class LatencyUkmReporter;

struct GlobalMetricsTrackers {
  DroppedFrameCounter* dropped_frame_counter = nullptr;
  LatencyUkmReporter* latency_ukm_reporter = nullptr;
  FrameSequenceTrackerCollection* frame_sequence_trackers = nullptr;
};

// This is used for tracing and reporting the duration of pipeline stages within
// a single frame.
//
// For each stage in the frame pipeline, calling StartStage will start tracing
// that stage (and end any currently running stages).
//
// If the tracked frame is presented (i.e. the frame termination status is
// kPresentedFrame), then the duration of each stage along with the total
// latency will be reported to UMA. If the tracked frame is not presented (i.e.
// the frame termination status is kDidNotPresentFrame or
// kReplacedByNewReporter), then the duration is reported under
// CompositorLatency.DroppedFrame.*.
// The format of each stage reported to UMA is
// "CompositorLatency.[DroppedFrame.][Interaction_name.].<StageName>".
class CC_EXPORT CompositorFrameReporter {
 public:
  enum class FrameTerminationStatus {
    // The tracked compositor frame was presented.
    kPresentedFrame,

    // The tracked compositor frame was submitted to the display compositor but
    // was not presented.
    kDidNotPresentFrame,

    // Reporter that is currently at a stage is replaced by a new one (e.g. two
    // BeginImplFrames can happen without issuing BeginMainFrame, so the first
    // reporter would terminate with this status).
    kReplacedByNewReporter,

    // Frame that was being tracked did not end up being submitting (e.g. frame
    // had no damage or LTHI was ended).
    kDidNotProduceFrame,

    // Default termination status. Should not be reachable.
    kUnknown
  };

  // These values are used for indexing the UMA histograms.
  enum class FrameReportType {
    kNonDroppedFrame = 0,
    kMissedDeadlineFrame = 1,
    kDroppedFrame = 2,
    kCompositorOnlyFrame = 3,
    kMaxValue = kCompositorOnlyFrame
  };

  // These values are used for indexing the UMA histograms.
  enum class StageType {
    kBeginImplFrameToSendBeginMainFrame = 0,
    kSendBeginMainFrameToCommit = 1,
    kCommit = 2,
    kEndCommitToActivation = 3,
    kActivation = 4,
    kEndActivateToSubmitCompositorFrame = 5,
    kSubmitCompositorFrameToPresentationCompositorFrame = 6,
    kTotalLatency = 7,
    kStageTypeCount
  };

  enum class VizBreakdown {
    kSubmitToReceiveCompositorFrame = 0,
    kReceivedCompositorFrameToStartDraw = 1,
    kStartDrawToSwapStart = 2,
    kSwapStartToSwapEnd = 3,
    kSwapEndToPresentationCompositorFrame = 4,

    // This is a breakdown of SwapStartToSwapEnd stage which is optionally
    // recorded if querying these timestamps is supported by the platform.
    kSwapStartToBufferAvailable = 5,
    kBufferAvailableToBufferReady = 6,
    kBufferReadyToLatch = 7,
    kLatchToSwapEnd = 8,
    kBreakdownCount
  };

  enum class BlinkBreakdown {
    kHandleInputEvents = 0,
    kAnimate = 1,
    kStyleUpdate = 2,
    kLayoutUpdate = 3,
    kPrepaint = 4,
    kCompositingInputs = 5,
    kPaint = 6,
    kCompositeCommit = 7,
    kUpdateLayers = 8,
    kBeginMainSentToStarted = 9,
    kBreakdownCount
  };

  struct CC_EXPORT StageData {
    StageType stage_type;
    base::TimeTicks start_time;
    base::TimeTicks end_time;
    StageData();
    StageData(StageType stage_type,
              base::TimeTicks start_time,
              base::TimeTicks end_time);
    StageData(const StageData&);
    ~StageData();
  };

  using SmoothThread = FrameInfo::SmoothThread;

  // Holds a processed list of Blink breakdowns with an `Iterator` class to
  // easily iterator over them.
  class CC_EXPORT ProcessedBlinkBreakdown {
   public:
    class Iterator {
     public:
      explicit Iterator(const ProcessedBlinkBreakdown* owner);
      ~Iterator();

      bool IsValid() const;
      void Advance();
      BlinkBreakdown GetBreakdown() const;
      base::TimeDelta GetLatency() const;

     private:
      const ProcessedBlinkBreakdown* owner_;

      size_t index_ = 0;
    };

    ProcessedBlinkBreakdown(base::TimeTicks blink_start_time,
                            base::TimeTicks begin_main_frame_start,
                            const BeginMainFrameMetrics& blink_breakdown);
    ~ProcessedBlinkBreakdown();

    ProcessedBlinkBreakdown(const ProcessedBlinkBreakdown&) = delete;
    ProcessedBlinkBreakdown& operator=(const ProcessedBlinkBreakdown&) = delete;

    // Returns a new iterator for the Blink breakdowns.
    Iterator CreateIterator() const;

   private:
    base::TimeDelta list_[static_cast<int>(BlinkBreakdown::kBreakdownCount)];
  };

  // Holds a processed list of Viz breakdowns with an `Iterator` class to easily
  // iterate over them.
  class CC_EXPORT ProcessedVizBreakdown {
   public:
    class Iterator {
     public:
      Iterator(const ProcessedVizBreakdown* owner,
               bool skip_swap_start_to_swap_end);
      ~Iterator();

      bool IsValid() const;
      void Advance();
      VizBreakdown GetBreakdown() const;
      base::TimeTicks GetStartTime() const;
      base::TimeTicks GetEndTime() const;
      base::TimeDelta GetDuration() const;

     private:
      const ProcessedVizBreakdown* owner_;
      const bool skip_swap_start_to_swap_end_;

      size_t index_ = 0;
    };

    ProcessedVizBreakdown(base::TimeTicks viz_start_time,
                          const viz::FrameTimingDetails& viz_breakdown);
    ~ProcessedVizBreakdown();

    ProcessedVizBreakdown(const ProcessedVizBreakdown&) = delete;
    ProcessedVizBreakdown& operator=(const ProcessedVizBreakdown&) = delete;

    // Returns a new iterator for the Viz breakdowns. If buffer ready breakdowns
    // are available, `skip_swap_start_to_swap_end_if_breakdown_available` can
    // be used to skip `kSwapStartToSwapEnd` breakdown.
    Iterator CreateIterator(
        bool skip_swap_start_to_swap_end_if_breakdown_available) const;

    base::TimeTicks swap_start() const { return swap_start_; }

   private:
    absl::optional<std::pair<base::TimeTicks, base::TimeTicks>>
        list_[static_cast<int>(VizBreakdown::kBreakdownCount)];

    bool buffer_ready_available_ = false;
    base::TimeTicks swap_start_;
  };

  CompositorFrameReporter(const ActiveTrackers& active_trackers,
                          const viz::BeginFrameArgs& args,
                          bool should_report_metrics,
                          SmoothThread smooth_thread,
                          FrameInfo::SmoothEffectDrivingThread scrolling_thread,
                          int layer_tree_host_id,
                          const GlobalMetricsTrackers& trackers);
  ~CompositorFrameReporter();

  CompositorFrameReporter(const CompositorFrameReporter& reporter) = delete;
  CompositorFrameReporter& operator=(const CompositorFrameReporter& reporter) =
      delete;

  // Creates and returns a clone of the reporter, only if it is currently in the
  // 'begin impl frame' stage. For any other state, it returns null.
  // This is used only when there is a partial update. So the cloned reporter
  // depends in this reporter to decide whether it contains be partial updates
  // or complete updates.
  std::unique_ptr<CompositorFrameReporter> CopyReporterAtBeginImplStage();

  // Note that the started stage may be reported to UMA. If the histogram is
  // intended to be reported then the histograms.xml file must be updated too.
  void StartStage(StageType stage_type, base::TimeTicks start_time);
  void TerminateFrame(FrameTerminationStatus termination_status,
                      base::TimeTicks termination_time);
  void SetBlinkBreakdown(std::unique_ptr<BeginMainFrameMetrics> blink_breakdown,
                         base::TimeTicks begin_main_start);
  void SetVizBreakdown(const viz::FrameTimingDetails& viz_breakdown);

  void AddEventsMetrics(EventMetrics::List events_metrics);
  EventMetrics::List TakeEventsMetrics();

  size_t stage_history_size_for_testing() const {
    return stage_history_.size();
  }

  void OnFinishImplFrame(base::TimeTicks timestamp);
  void OnAbortBeginMainFrame(base::TimeTicks timestamp);
  void OnDidNotProduceFrame(FrameSkippedReason skip_reason);
  void EnableCompositorOnlyReporting();
  bool did_finish_impl_frame() const { return did_finish_impl_frame_; }
  base::TimeTicks impl_frame_finish_time() const {
    return impl_frame_finish_time_;
  }

  bool did_not_produce_frame() const {
    return did_not_produce_frame_time_.has_value();
  }
  base::TimeTicks did_not_produce_frame_time() const {
    return *did_not_produce_frame_time_;
  }

  bool did_abort_main_frame() const {
    return main_frame_abort_time_.has_value();
  }
  base::TimeTicks main_frame_abort_time() const {
    return *main_frame_abort_time_;
  }

  FrameSkippedReason frame_skip_reason() const { return *frame_skip_reason_; }

  void set_tick_clock(const base::TickClock* tick_clock) {
    DCHECK(tick_clock);
    tick_clock_ = tick_clock;
  }

  void set_has_missing_content(bool has_missing_content) {
    has_missing_content_ = has_missing_content;
  }

  void SetPartialUpdateDecider(CompositorFrameReporter* decider);

  size_t partial_update_dependents_size_for_testing() const {
    return partial_update_dependents_.size();
  }

  size_t owned_partial_update_dependents_size_for_testing() const {
    return owned_partial_update_dependents_.size();
  }

  void set_is_accompanied_by_main_thread_update(
      bool is_accompanied_by_main_thread_update) {
    is_accompanied_by_main_thread_update_ =
        is_accompanied_by_main_thread_update;
  }

  const viz::BeginFrameId& frame_id() const { return args_.frame_id; }

  // Adopts |cloned_reporter|, i.e. keeps |cloned_reporter| alive until after
  // this reporter terminates. Note that the |cloned_reporter| must have been
  // created from this reporter using |CopyReporterAtBeginImplStage()|.
  void AdoptReporter(std::unique_ptr<CompositorFrameReporter> cloned_reporter);

  // If this is a cloned reporter, then this returns a weak-ptr to the original
  // reporter this was cloned from (using |CopyReporterAtBeginImplStage()|).

  CompositorFrameReporter* partial_update_decider() const {
    return partial_update_decider_.get();
  }
  using FrameReportTypes =
      std::bitset<static_cast<size_t>(FrameReportType::kMaxValue) + 1>;

 protected:
  void set_has_partial_update(bool has_partial_update) {
    has_partial_update_ = has_partial_update;
  }

 private:
  void TerminateReporter();
  void EndCurrentStage(base::TimeTicks end_time);

  void ReportCompositorLatencyHistograms() const;
  void ReportStageHistogramWithBreakdown(
      const StageData& stage,
      FrameSequenceTrackerType frame_sequence_tracker_type =
          FrameSequenceTrackerType::kMaxType) const;
  void ReportCompositorLatencyBlinkBreakdowns(
      FrameSequenceTrackerType frame_sequence_tracker_type) const;
  void ReportCompositorLatencyVizBreakdowns(
      FrameSequenceTrackerType frame_sequence_tracker_type) const;
  void ReportCompositorLatencyHistogram(
      FrameSequenceTrackerType intraction_type,
      const int stage_type_index,
      base::TimeDelta time_delta) const;

  void ReportEventLatencyHistograms() const;
  void ReportCompositorLatencyTraceEvents(const FrameInfo& info) const;
  void ReportEventLatencyTraceEvents() const;

  void EnableReportType(FrameReportType report_type) {
    report_types_.set(static_cast<size_t>(report_type));
  }
  bool TestReportType(FrameReportType report_type) const {
    return report_types_.test(static_cast<size_t>(report_type));
  }

  // This method is only used for DCheck
  base::TimeDelta SumOfStageHistory() const;

  // Terminating reporters in partial_update_dependents_ after a limit.
  void DiscardOldPartialUpdateReporters();

  base::TimeTicks Now() const;

  FrameInfo GenerateFrameInfo() const;

  base::WeakPtr<CompositorFrameReporter> GetWeakPtr();

  const bool should_report_metrics_;
  const viz::BeginFrameArgs args_;

  StageData current_stage_;

  BeginMainFrameMetrics blink_breakdown_;
  base::TimeTicks blink_start_time_;
  std::unique_ptr<ProcessedBlinkBreakdown> processed_blink_breakdown_;

  viz::FrameTimingDetails viz_breakdown_;
  base::TimeTicks viz_start_time_;
  std::unique_ptr<ProcessedVizBreakdown> processed_viz_breakdown_;

  // Stage data is recorded here. On destruction these stages will be reported
  // to UMA if the termination status is |kPresentedFrame|. Reported data will
  // be divided based on the frame submission status.
  std::vector<StageData> stage_history_;

  // List of metrics for events affecting this frame.
  EventMetrics::List events_metrics_;

  FrameReportTypes report_types_;

  base::TimeTicks frame_termination_time_;
  base::TimeTicks begin_main_frame_start_;
  FrameTerminationStatus frame_termination_status_ =
      FrameTerminationStatus::kUnknown;

  const ActiveTrackers active_trackers_;
  const FrameInfo::SmoothEffectDrivingThread scrolling_thread_;

  // Indicates if work on Impl frame is finished.
  bool did_finish_impl_frame_ = false;
  // The time that work on Impl frame is finished. It's only valid if the
  // reporter is in a stage other than begin impl frame.
  base::TimeTicks impl_frame_finish_time_;

  // The timestamp of when the frame was marked as not having produced a frame
  // (through a call to DidNotProduceFrame()).
  absl::optional<base::TimeTicks> did_not_produce_frame_time_;
  absl::optional<FrameSkippedReason> frame_skip_reason_;
  absl::optional<base::TimeTicks> main_frame_abort_time_;

  raw_ptr<const base::TickClock> tick_clock_ =
      base::DefaultTickClock::GetInstance();

  bool has_partial_update_ = false;

  // If the submitted frame has update from main thread
  bool is_accompanied_by_main_thread_update_ = false;

  const SmoothThread smooth_thread_;
  const int layer_tree_host_id_;

  // Indicates whether the submitted frame had any missing content (i.e. content
  // with checkerboarding).
  bool has_missing_content_ = false;

  // For a reporter A, if the main-thread takes a long time to respond
  // to a begin-main-frame, then all reporters created (and terminated) until
  // the main-thread responds depends on this reporter to decide whether those
  // frames contained partial updates (i.e. main-thread made some visual
  // updates, but were not included in the frame), or complete updates.
  // In such cases, |partial_update_dependents_| for A contains all the frames
  // that depend on A for deciding whether they had partial updates or not, and
  // |partial_update_decider_| is set to A for all these reporters.
  std::queue<base::WeakPtr<CompositorFrameReporter>> partial_update_dependents_;
  base::WeakPtr<CompositorFrameReporter> partial_update_decider_;
  uint32_t discarded_partial_update_dependents_count_ = 0;

  // From the above example, it may be necessary for A to keep all the
  // dependents alive until A terminates, so that the dependents can set their
  // |has_partial_update_| flags correctly. This is done by passing ownership of
  // these reporters (using |AdoptReporter()|).
  std::queue<std::unique_ptr<CompositorFrameReporter>>
      owned_partial_update_dependents_;

  const GlobalMetricsTrackers global_trackers_;

  base::WeakPtrFactory<CompositorFrameReporter> weak_factory_{this};
};

}  // namespace cc

#endif  // CC_METRICS_COMPOSITOR_FRAME_REPORTER_H_"
