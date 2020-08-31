// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_
#define CC_TEST_FAKE_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_

#include <vector>

#include "cc/metrics/compositor_frame_reporting_controller.h"

namespace viz {
struct FrameTimingDetails;
}

namespace cc {
// This class is to be used for testing, during cases where the DCHECKs won't
// hold due to testing only a portion of the compositor pipeline. This class
// will automatically generate the preceding stages that are missing from the
// pipeline.
class FakeCompositorFrameReportingController
    : public CompositorFrameReportingController {
 public:
  FakeCompositorFrameReportingController();

  FakeCompositorFrameReportingController(
      const FakeCompositorFrameReportingController& controller) = delete;
  FakeCompositorFrameReportingController& operator=(
      const FakeCompositorFrameReportingController& controller) = delete;

  void WillBeginMainFrame(const viz::BeginFrameArgs& args) override;
  void BeginMainFrameAborted(const viz::BeginFrameId& id) override;
  void WillCommit() override;
  void DidCommit() override;
  void WillActivate() override;
  void DidActivate() override;
  void DidSubmitCompositorFrame(
      uint32_t frame_token,
      const viz::BeginFrameId& current_frame_id,
      const viz::BeginFrameId& last_activated_frame_id,
      EventMetricsSet events_metrics) override;
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override;
};
}  // namespace cc

#endif  // CC_TEST_FAKE_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_
