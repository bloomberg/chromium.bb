// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FirstMeaningfulPaintDetector.h"

#include "core/paint/PaintTiming.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/text/StringBuilder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FirstMeaningfulPaintDetectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    platform_->AdvanceClockSeconds(1);
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  }

  double AdvanceClockAndGetTime() {
    platform_->AdvanceClockSeconds(1);
    return MonotonicallyIncreasingTime();
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }
  PaintTiming& GetPaintTiming() { return PaintTiming::From(GetDocument()); }
  FirstMeaningfulPaintDetector& Detector() {
    return GetPaintTiming().GetFirstMeaningfulPaintDetector();
  }

  void SimulateLayoutAndPaint(int new_elements) {
    platform_->AdvanceClockSeconds(0.001);
    StringBuilder builder;
    for (int i = 0; i < new_elements; i++)
      builder.Append("<span>a</span>");
    GetDocument().write(builder.ToString());
    GetDocument().UpdateStyleAndLayout();
    Detector().NotifyPaint();
  }

  void SimulateNetworkStable() {
    GetDocument().SetParsingState(Document::kFinishedParsing);
    Detector().Network0QuietTimerFired(nullptr);
    Detector().Network2QuietTimerFired(nullptr);
  }

  void SimulateNetwork0Quiet() {
    GetDocument().SetParsingState(Document::kFinishedParsing);
    Detector().Network0QuietTimerFired(nullptr);
  }

  void SimulateNetwork2Quiet() {
    GetDocument().SetParsingState(Document::kFinishedParsing);
    Detector().Network2QuietTimerFired(nullptr);
  }

  void SimulateUserInput() { Detector().NotifyInputEvent(); }

  void SetActiveConnections(int connections) {
    Detector().SetNetworkQuietTimers(connections);
  }

  bool IsNetwork0QuietTimerActive() {
    return Detector().network0_quiet_timer_.IsActive();
  }

  bool IsNetwork2QuietTimerActive() {
    return Detector().network2_quiet_timer_.IsActive();
  }

  bool HadNetwork0Quiet() { return Detector().network0_quiet_reached_; }
  bool HadNetwork2Quiet() { return Detector().network2_quiet_reached_; }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

  static constexpr double kNetwork0QuietWindowSeconds =
      FirstMeaningfulPaintDetector::kNetwork0QuietWindowSeconds;
  static constexpr double kNetwork2QuietWindowSeconds =
      FirstMeaningfulPaintDetector::kNetwork2QuietWindowSeconds;

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(FirstMeaningfulPaintDetectorTest, NoFirstPaint) {
  SimulateLayoutAndPaint(1);
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), 0.0);
}

TEST_F(FirstMeaningfulPaintDetectorTest, OneLayout) {
  GetPaintTiming().MarkFirstContentfulPaint();
  SimulateLayoutAndPaint(1);
  double after_paint = AdvanceClockAndGetTime();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), 0.0);
  SimulateNetworkStable();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstPaint());
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_paint);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantSecond) {
  GetPaintTiming().MarkFirstContentfulPaint();
  SimulateLayoutAndPaint(1);
  double after_layout1 = AdvanceClockAndGetTime();
  SimulateLayoutAndPaint(10);
  double after_layout2 = AdvanceClockAndGetTime();
  SimulateNetworkStable();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), after_layout1);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_layout2);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantFirst) {
  GetPaintTiming().MarkFirstContentfulPaint();
  SimulateLayoutAndPaint(10);
  double after_layout1 = AdvanceClockAndGetTime();
  SimulateLayoutAndPaint(1);
  SimulateNetworkStable();
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstPaint());
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_layout1);
}

TEST_F(FirstMeaningfulPaintDetectorTest, FirstMeaningfulPaintCandidate) {
  GetPaintTiming().MarkFirstContentfulPaint();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), 0.0);
  SimulateLayoutAndPaint(1);
  double after_paint = AdvanceClockAndGetTime();
  // The first candidate gets ignored.
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), 0.0);
  SimulateLayoutAndPaint(10);
  // The second candidate gets reported.
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintCandidate(), after_paint);
  double candidate = GetPaintTiming().FirstMeaningfulPaintCandidate();
  // The third candidate gets ignored since we already saw the first candidate.
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), candidate);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       OnlyOneFirstMeaningfulPaintCandidateBeforeNetworkStable) {
  GetPaintTiming().MarkFirstContentfulPaint();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), 0.0);
  double before_paint = AdvanceClockAndGetTime();
  SimulateLayoutAndPaint(1);
  // The first candidate is initially ignored.
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), 0.0);
  SimulateNetworkStable();
  // The networkStable then promotes the first candidate.
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaintCandidate(), before_paint);
  double candidate = GetPaintTiming().FirstMeaningfulPaintCandidate();
  // The second candidate is then ignored.
  SimulateLayoutAndPaint(10);
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaintCandidate(), candidate);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       NetworkStableBeforeFirstContentfulPaint) {
  GetPaintTiming().MarkFirstPaint();
  SimulateLayoutAndPaint(1);
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), 0.0);
  GetPaintTiming().MarkFirstContentfulPaint();
  SimulateNetworkStable();
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaint(), 0.0);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       FirstMeaningfulPaintShouldNotBeBeforeFirstContentfulPaint) {
  GetPaintTiming().MarkFirstPaint();
  SimulateLayoutAndPaint(10);
  GetPaintTiming().MarkFirstContentfulPaint();
  SimulateNetworkStable();
  EXPECT_GE(GetPaintTiming().FirstMeaningfulPaint(),
            GetPaintTiming().FirstContentfulPaint());
}

TEST_F(FirstMeaningfulPaintDetectorTest, Network2QuietThen0Quiet) {
  GetPaintTiming().MarkFirstContentfulPaint();

  SimulateLayoutAndPaint(1);
  double after_first_paint = AdvanceClockAndGetTime();
  SimulateNetwork2Quiet();

  SimulateLayoutAndPaint(10);
  SimulateNetwork0Quiet();

  // The first paint is FirstMeaningfulPaint.
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), 0.0);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_first_paint);
}

TEST_F(FirstMeaningfulPaintDetectorTest, Network0QuietThen2Quiet) {
  GetPaintTiming().MarkFirstContentfulPaint();

  SimulateLayoutAndPaint(1);
  double after_first_paint = AdvanceClockAndGetTime();
  SimulateNetwork0Quiet();

  SimulateLayoutAndPaint(10);
  double after_second_paint = AdvanceClockAndGetTime();
  SimulateNetwork2Quiet();

  // The second paint is FirstMeaningfulPaint.
  EXPECT_GT(GetPaintTiming().FirstMeaningfulPaint(), after_first_paint);
  EXPECT_LT(GetPaintTiming().FirstMeaningfulPaint(), after_second_paint);
}

TEST_F(FirstMeaningfulPaintDetectorTest, Network0QuietTimer) {
  GetPaintTiming().MarkFirstContentfulPaint();

  SetActiveConnections(1);
  EXPECT_FALSE(IsNetwork0QuietTimerActive());

  SetActiveConnections(0);
  platform_->RunForPeriodSeconds(kNetwork0QuietWindowSeconds - 0.1);
  EXPECT_TRUE(IsNetwork0QuietTimerActive());
  EXPECT_FALSE(HadNetwork0Quiet());

  SetActiveConnections(0);  // This should reset the 0-quiet timer.
  platform_->RunForPeriodSeconds(kNetwork0QuietWindowSeconds - 0.1);
  EXPECT_TRUE(IsNetwork0QuietTimerActive());
  EXPECT_FALSE(HadNetwork0Quiet());

  platform_->RunForPeriodSeconds(0.1001);
  EXPECT_TRUE(HadNetwork0Quiet());
}

TEST_F(FirstMeaningfulPaintDetectorTest, Network2QuietTimer) {
  GetPaintTiming().MarkFirstContentfulPaint();

  SetActiveConnections(3);
  EXPECT_FALSE(IsNetwork2QuietTimerActive());

  SetActiveConnections(2);
  platform_->RunForPeriodSeconds(kNetwork2QuietWindowSeconds - 0.1);
  EXPECT_TRUE(IsNetwork2QuietTimerActive());
  EXPECT_FALSE(HadNetwork2Quiet());

  SetActiveConnections(2);  // This should reset the 2-quiet timer.
  platform_->RunForPeriodSeconds(kNetwork2QuietWindowSeconds - 0.1);
  EXPECT_TRUE(IsNetwork2QuietTimerActive());
  EXPECT_FALSE(HadNetwork2Quiet());

  SetActiveConnections(1);  // This should not reset the 2-quiet timer.
  platform_->RunForPeriodSeconds(0.1001);
  EXPECT_TRUE(HadNetwork2Quiet());
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       FirstMeaningfulPaintAfterUserInteraction) {
  GetPaintTiming().MarkFirstContentfulPaint();
  SimulateUserInput();
  SimulateLayoutAndPaint(10);
  SimulateNetworkStable();
  EXPECT_EQ(GetPaintTiming().FirstMeaningfulPaint(), 0.0);
}

TEST_F(FirstMeaningfulPaintDetectorTest, UserInteractionBeforeFirstPaint) {
  SimulateUserInput();
  GetPaintTiming().MarkFirstContentfulPaint();
  SimulateLayoutAndPaint(10);
  SimulateNetworkStable();
  EXPECT_NE(GetPaintTiming().FirstMeaningfulPaint(), 0.0);
}

}  // namespace blink
