// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/compositor_frame_reporting_controller.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class CompositorFrameReportingControllerTest;

class TestCompositorFrameReportingController
    : public CompositorFrameReportingController {
 public:
  TestCompositorFrameReportingController(
      CompositorFrameReportingControllerTest* test)
      : CompositorFrameReportingController(), test_(test) {}

  TestCompositorFrameReportingController(
      const TestCompositorFrameReportingController& controller) = delete;

  TestCompositorFrameReportingController& operator=(
      const TestCompositorFrameReportingController& controller) = delete;

  std::unique_ptr<CompositorFrameReporter>* reporters() { return reporters_; }

  int ActiveReporters() {
    int count = 0;
    for (int i = 0; i < PipelineStage::kNumPipelineStages; ++i) {
      if (reporters_[i])
        ++count;
    }
    return count;
  }

 protected:
  CompositorFrameReportingControllerTest* test_;
};

class CompositorFrameReportingControllerTest : public testing::Test {
 public:
  CompositorFrameReportingControllerTest() : reporting_controller_(this) {}

  // The following functions simulate the actions that would
  // occur for each phase of the reporting controller.
  void SimulateBeginImplFrame() { reporting_controller_.WillBeginImplFrame(); }

  void SimulateBeginMainFrame() {
    if (!reporting_controller_.reporters()[CompositorFrameReportingController::
                                               PipelineStage::kBeginImplFrame])
      SimulateBeginImplFrame();
    CHECK(
        reporting_controller_.reporters()[CompositorFrameReportingController::
                                              PipelineStage::kBeginImplFrame]);
    reporting_controller_.WillBeginMainFrame();
  }

  void SimulateCommit() {
    if (!reporting_controller_.reporters()[CompositorFrameReportingController::
                                               PipelineStage::kBeginMainFrame])
      SimulateBeginMainFrame();
    CHECK(
        reporting_controller_.reporters()[CompositorFrameReportingController::
                                              PipelineStage::kBeginMainFrame]);
    reporting_controller_.WillCommit();
    reporting_controller_.DidCommit();
  }

  void SimulateActivate() {
    if (!reporting_controller_.reporters()
             [CompositorFrameReportingController::PipelineStage::kCommit])
      SimulateCommit();
    CHECK(reporting_controller_.reporters()
              [CompositorFrameReportingController::PipelineStage::kCommit]);
    reporting_controller_.WillActivate();
    reporting_controller_.DidActivate();
  }

  void SimulateSubmitCompositorFrame() {
    if (!reporting_controller_.reporters()
             [CompositorFrameReportingController::PipelineStage::kActivate])
      SimulateActivate();
    CHECK(reporting_controller_.reporters()
              [CompositorFrameReportingController::PipelineStage::kActivate]);
    reporting_controller_.DidSubmitCompositorFrame();
  }

 protected:
  TestCompositorFrameReportingController reporting_controller_;
};

TEST_F(CompositorFrameReportingControllerTest, ActiveReporterCounts) {
  // Check that there are no leaks with the CompositorFrameReporter
  // objects no matter what the sequence of scheduled actions is
  // Note that due to DCHECKs in WillCommit(), WillActivate(), etc., it
  // is impossible to have 2 reporters both in BMF or Commit

  // Tests Cases:
  // - 2 Reporters at Activate phase
  // - 2 back-to-back BeginImplFrames
  // - 4 Simultaneous Reporters

  // BF
  reporting_controller_.WillBeginImplFrame();
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  // BF -> BF
  // Should replace previous reporter
  reporting_controller_.WillBeginImplFrame();
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF
  // Should add new reporter
  reporting_controller_.WillBeginMainFrame();
  reporting_controller_.WillBeginImplFrame();
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF -> Commit
  // Should stay same
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF -> Commit -> BMF -> Activate -> Commit -> Activation
  // Having two reporters at Activate phase should delete the older one
  reporting_controller_.WillBeginMainFrame();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  reporting_controller_.DidSubmitCompositorFrame();
  EXPECT_EQ(0, reporting_controller_.ActiveReporters());

  // 4 simultaneous reporters
  SimulateActivate();

  SimulateCommit();

  SimulateBeginMainFrame();

  SimulateBeginImplFrame();
  EXPECT_EQ(4, reporting_controller_.ActiveReporters());

  // Any additional BeginImplFrame's would be ignored
  SimulateBeginImplFrame();
  EXPECT_EQ(4, reporting_controller_.ActiveReporters());
}
}  // namespace
}  // namespace cc
