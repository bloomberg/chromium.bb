// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_COMPOSITOR_FRAME_REPORTER_H_
#define CC_SCHEDULER_COMPOSITOR_FRAME_REPORTER_H_

#include "cc/base/base_export.h"
#include "cc/cc_export.h"

namespace cc {
// This is used for tracing the pipeline stages of a single frame.
//
// For each stage in the frame pipeline, calling StartStage will start tracing
// that stage (and end any currently running stages).
// TODO(alsan): Report stage durations to UMA.
class CC_EXPORT CompositorFrameReporter {
 public:
  enum FrameTerminationStatus {
    // Compositor frame (with main thread updates) is submitted before a new
    // BeginImplFrame is issued (i.e. BF -> BMF -> Commit -> Activate ->
    // Submit).
    kSubmittedFrame,

    // Same as SubmittedFrame, but with the condition that there is another
    // frame being processed in the pipeline at an earlier stage.
    // This would imply that a new BeginImplFrame was issued during the lifetime
    // of this reporter, and therefore it missed its deadline
    // (e.g. BF1 -> BMF1 -> Submit -> BF2 -> Commit1 -> Activate1 -> BMF2 ->
    // Submit).
    kSubmittedFrameMissedDeadline,

    // Main frame was aborted; the reporter will not continue reporting.
    kMainFrameAborted,

    // Reporter that is currently at a stage is replaced by a new one (e.g. two
    // BeginImplFrames can happen without issuing BeginMainFrame, so the first
    // reporter would terminate with this status).
    // TODO(alsan): Track impl-only frames.
    kReplacedByNewReporter,

    // Frame that was being tracked did not end up being submitting (e.g. frame
    // had no damage or LTHI was ended).
    kDidNotProduceFrame,

    // Default termination status. Should not be reachable.
    kUnknown
  };

  CompositorFrameReporter();
  CompositorFrameReporter(const CompositorFrameReporter& reporter) = delete;
  ~CompositorFrameReporter();

  CompositorFrameReporter& operator=(const CompositorFrameReporter& reporter) =
      delete;

  void StartStage(const char* stage_name);
  void SetFrameTerminationStatus(FrameTerminationStatus termination_status);

 private:
  FrameTerminationStatus frame_termination_status_ =
      FrameTerminationStatus::kUnknown;
  void TerminateFrame();
};
}  // namespace cc

#endif  // CC_SCHEDULER_COMPOSITOR_FRAME_REPORTER_H_"
