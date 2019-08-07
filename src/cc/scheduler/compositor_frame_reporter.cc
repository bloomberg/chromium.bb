// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/compositor_frame_reporter.h"

#include "base/trace_event/trace_event.h"

namespace cc {

CompositorFrameReporter::CompositorFrameReporter() {
  TRACE_EVENT_ASYNC_BEGIN0("cc,benchmark", "PipelineReporter", this);
}

CompositorFrameReporter::~CompositorFrameReporter() {
  TerminateFrame();
}

void CompositorFrameReporter::StartStage(const char* stage_name) {
  TRACE_EVENT_ASYNC_STEP_INTO0("cc,benchmark", "PipelineReporter", this,
                               TRACE_STR_COPY(stage_name));
}

void CompositorFrameReporter::SetFrameTerminationStatus(
    FrameTerminationStatus termination_status) {
  frame_termination_status_ = termination_status;
}

void CompositorFrameReporter::TerminateFrame() {
  const char* termination_status_str;
  switch (frame_termination_status_) {
    case FrameTerminationStatus::kSubmittedFrame:
      termination_status_str = "submitted_frame";
      break;
    case FrameTerminationStatus::kSubmittedFrameMissedDeadline:
      termination_status_str = "missed_frame";
      break;
    case FrameTerminationStatus::kMainFrameAborted:
      termination_status_str = "main_frame_aborted";
      break;
    case FrameTerminationStatus::kReplacedByNewReporter:
      termination_status_str = "replaced_by_new_reporter_at_same_stage";
      break;
    case FrameTerminationStatus::kDidNotProduceFrame:
      termination_status_str = "did_not_produce_frame";
      break;
    case FrameTerminationStatus::kUnknown:
      NOTREACHED();
      break;
  }
  TRACE_EVENT_ASYNC_END1("cc,benchmark", "PipelineReporter", this,
                         "termination_status",
                         TRACE_STR_COPY(termination_status_str));
  // TODO(alsan): UMA histogram reporting
}
}  // namespace cc
