// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_FRAME_SEQUENCE_METRICS_H_
#define CC_METRICS_FRAME_SEQUENCE_METRICS_H_

#include "base/callback.h"
#include "base/optional.h"
#include "base/trace_event/traced_value.h"
#include "cc/cc_export.h"

namespace cc {
class ThroughputUkmReporter;

enum class FrameSequenceTrackerType {
  // Used as an enum for metrics. DO NOT reorder or delete values. Rather,
  // add them at the end and increment kMaxType.
  kCompositorAnimation = 0,
  kMainThreadAnimation = 1,
  kPinchZoom = 2,
  kRAF = 3,
  kTouchScroll = 4,
  kUniversal = 5,
  kVideo = 6,
  kWheelScroll = 7,
  kScrollbarScroll = 8,
  kCustom = 9,  // Note that the metrics for kCustom are not reported on UMA,
                // and instead are dispatched back to the LayerTreeHostClient.
  kMaxType
};

class CC_EXPORT FrameSequenceMetrics {
 public:
  FrameSequenceMetrics(FrameSequenceTrackerType type,
                       ThroughputUkmReporter* ukm_reporter);
  ~FrameSequenceMetrics();

  FrameSequenceMetrics(const FrameSequenceMetrics&) = delete;
  FrameSequenceMetrics& operator=(const FrameSequenceMetrics&) = delete;

  enum class ThreadType { kMain, kCompositor, kSlower, kUnknown };

  struct ThroughputData {
    static std::unique_ptr<base::trace_event::TracedValue> ToTracedValue(
        const ThroughputData& impl,
        const ThroughputData& main);

    // Returns the throughput in percent, a return value of base::nullopt
    // indicates that no throughput metric is reported.
    static base::Optional<int> ReportHistogram(FrameSequenceMetrics* metrics,
                                               ThreadType thread_type,
                                               int metric_index,
                                               const ThroughputData& data);

    void Merge(const ThroughputData& data) {
      frames_expected += data.frames_expected;
      frames_produced += data.frames_produced;
#if DCHECK_IS_ON()
      frames_processed += data.frames_processed;
      frames_received += data.frames_received;
#endif
    }

    // Tracks the number of frames that were expected to be shown during this
    // frame-sequence.
    uint32_t frames_expected = 0;

    // Tracks the number of frames that were actually presented to the user
    // during this frame-sequence.
    uint32_t frames_produced = 0;

#if DCHECK_IS_ON()
    // Tracks the number of frames that is either submitted or reported as no
    // damage.
    uint32_t frames_processed = 0;

    // Tracks the number of begin-frames that are received.
    uint32_t frames_received = 0;
#endif
  };

  void SetScrollingThread(ThreadType thread);

  using CustomReporter =
      base::OnceCallback<void(ThroughputData throughput_data)>;
  // Sets reporter callback for kCustom typed sequence.
  void SetCustomReporter(CustomReporter custom_reporter);

  // Returns the 'effective thread' for the metrics (i.e. the thread most
  // relevant for this metric).
  ThreadType GetEffectiveThread() const;

  void Merge(std::unique_ptr<FrameSequenceMetrics> metrics);
  bool HasEnoughDataForReporting() const;
  bool HasDataLeftForReporting() const;
  // Report related metrics: throughput, checkboarding...
  void ReportMetrics();
  void ComputeAggregatedThroughputForTesting() {
    ComputeAggregatedThroughput();
  }

  ThroughputData& impl_throughput() { return impl_throughput_; }
  ThroughputData& main_throughput() { return main_throughput_; }
  ThroughputData& aggregated_throughput() { return aggregated_throughput_; }
  void add_checkerboarded_frames(int64_t frames) {
    frames_checkerboarded_ += frames;
  }
  uint32_t frames_checkerboarded() const { return frames_checkerboarded_; }

  FrameSequenceTrackerType type() const { return type_; }
  ThroughputUkmReporter* ukm_reporter() const {
    return throughput_ukm_reporter_;
  }

 private:
  void ComputeAggregatedThroughput();
  const FrameSequenceTrackerType type_;

  // Pointer to the reporter owned by the FrameSequenceTrackerCollection.
  ThroughputUkmReporter* const throughput_ukm_reporter_;

  ThroughputData impl_throughput_;
  ThroughputData main_throughput_;
  // The aggregated throughput for the main/compositor thread.
  ThroughputData aggregated_throughput_;

  ThreadType scrolling_thread_ = ThreadType::kUnknown;

  // Tracks the number of produced frames that had some amount of
  // checkerboarding, and how many frames showed such checkerboarded frames.
  uint32_t frames_checkerboarded_ = 0;

  // Callback invoked to report metrics for kCustom typed sequence.
  CustomReporter custom_reporter_;
};

}  // namespace cc

#endif  // CC_METRICS_FRAME_SEQUENCE_METRICS_H_
