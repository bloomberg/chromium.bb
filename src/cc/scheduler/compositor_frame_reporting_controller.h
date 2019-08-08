// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_
#define CC_SCHEDULER_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_

#include <memory>

#include "base/time/time.h"
#include "cc/base/base_export.h"
#include "cc/cc_export.h"

namespace cc {
class CompositorFrameReporter;

// This is used for managing simultaneous CompositorFrameReporter instances
// in the case that the compositor has high latency. Calling one of the
// event functions will begin recording the time of the corresponding
// phase and trace it. If the frame is eventually submitted, then the
// recorded times of each phase will be reported in UMA.
// See CompositorFrameReporter.
class CC_EXPORT CompositorFrameReportingController {
 public:
  // Used as indices for accessing CompositorFrameReporters.
  enum PipelineStage {
    kBeginImplFrame = 0,
    kBeginMainFrame,
    kCommit,
    kActivate,
    kNumPipelineStages
  };

  CompositorFrameReportingController();
  virtual ~CompositorFrameReportingController();

  CompositorFrameReportingController(
      const CompositorFrameReportingController&) = delete;
  CompositorFrameReportingController& operator=(
      const CompositorFrameReportingController&) = delete;

  // Events to signal Beginning/Ending of phases.
  virtual void WillBeginImplFrame();
  virtual void WillBeginMainFrame();
  virtual void BeginMainFrameAborted();
  virtual void WillInvalidateOnImplSide();
  virtual void WillCommit();
  virtual void DidCommit();
  virtual void WillActivate();
  virtual void DidActivate();
  virtual void DidSubmitCompositorFrame();
  virtual void DidNotProduceFrame();

 protected:
  std::unique_ptr<CompositorFrameReporter>
      reporters_[PipelineStage::kNumPipelineStages];

 private:
  void AdvanceReporterStage(PipelineStage start, PipelineStage target);

  bool next_activate_has_invalidation_ = false;
};
}  // namespace cc

#endif  // CC_SCHEDULER_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_
