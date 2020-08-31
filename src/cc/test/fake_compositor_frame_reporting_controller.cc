// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_compositor_frame_reporting_controller.h"

#include <utility>
#include <vector>

#include "components/viz/common/frame_timing_details.h"

namespace cc {
base::TimeDelta INTERVAL = base::TimeDelta::FromMilliseconds(16);

FakeCompositorFrameReportingController::FakeCompositorFrameReportingController()
    : CompositorFrameReportingController(/*should_report_metrics=*/true) {}

void FakeCompositorFrameReportingController::WillBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  if (!HasReporterAt(PipelineStage::kBeginImplFrame))
    CompositorFrameReportingController::WillBeginImplFrame(args);
  CompositorFrameReportingController::WillBeginMainFrame(args);
}

void FakeCompositorFrameReportingController::BeginMainFrameAborted(
    const viz::BeginFrameId& id) {
  if (!HasReporterAt(PipelineStage::kBeginMainFrame)) {
    viz::BeginFrameArgs args = viz::BeginFrameArgs();
    args.frame_id = id;
    args.frame_time = Now();
    args.interval = INTERVAL;
    WillBeginMainFrame(args);
  }
  CompositorFrameReportingController::BeginMainFrameAborted(id);
}

void FakeCompositorFrameReportingController::WillCommit() {
  if (!HasReporterAt(PipelineStage::kBeginMainFrame)) {
    viz::BeginFrameArgs args = viz::BeginFrameArgs();
    args.frame_id = viz::BeginFrameId();
    args.frame_time = Now();
    args.interval = INTERVAL;
    WillBeginMainFrame(args);
  }
  CompositorFrameReportingController::WillCommit();
}

void FakeCompositorFrameReportingController::DidCommit() {
  if (!HasReporterAt(PipelineStage::kBeginMainFrame))
    WillCommit();
  CompositorFrameReportingController::DidCommit();
}

void FakeCompositorFrameReportingController::WillActivate() {
  if (!HasReporterAt(PipelineStage::kCommit))
    DidCommit();
  CompositorFrameReportingController::WillActivate();
}

void FakeCompositorFrameReportingController::DidActivate() {
  if (!HasReporterAt(PipelineStage::kCommit))
    WillActivate();
  CompositorFrameReportingController::DidActivate();
}

void FakeCompositorFrameReportingController::DidSubmitCompositorFrame(
    uint32_t frame_token,
    const viz::BeginFrameId& current_frame_id,
    const viz::BeginFrameId& last_activated_frame_id,
    EventMetricsSet events_metrics) {
  CompositorFrameReportingController::DidSubmitCompositorFrame(
      frame_token, current_frame_id, last_activated_frame_id,
      std::move(events_metrics));

  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = base::TimeTicks::Now();
  CompositorFrameReportingController::DidPresentCompositorFrame(frame_token,
                                                                details);
}

void FakeCompositorFrameReportingController::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {}
}  // namespace cc
