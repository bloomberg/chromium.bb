// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FirstMeaningfulPaintDetector.h"

#include "core/paint/PaintTiming.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

class FirstMeaningfulPaintDetectorTest : public testing::Test {
 protected:
  void SetUp() override {
    m_platform->advanceClockSeconds(1);
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
  }

  double advanceClockAndGetTime() {
    m_platform->advanceClockSeconds(1);
    return monotonicallyIncreasingTime();
  }

  Document& document() { return m_dummyPageHolder->document(); }
  PaintTiming& paintTiming() { return PaintTiming::from(document()); }
  FirstMeaningfulPaintDetector& detector() {
    return paintTiming().firstMeaningfulPaintDetector();
  }

  void simulateLayoutAndPaint(int newElements) {
    m_platform->advanceClockSeconds(0.001);
    StringBuilder builder;
    for (int i = 0; i < newElements; i++)
      builder.append("<span>a</span>");
    document().write(builder.toString());
    document().updateStyleAndLayout();
    detector().notifyPaint();
  }

  void simulateNetworkStable() {
    document().setParsingState(Document::FinishedParsing);
    detector().network0QuietTimerFired(nullptr);
    detector().network2QuietTimerFired(nullptr);
  }

  void simulateNetwork0Quiet() {
    document().setParsingState(Document::FinishedParsing);
    detector().network0QuietTimerFired(nullptr);
  }

  void simulateNetwork2Quiet() {
    document().setParsingState(Document::FinishedParsing);
    detector().network2QuietTimerFired(nullptr);
  }

  void setActiveConnections(int connections) {
    detector().setNetworkQuietTimers(connections);
  }

  bool isNetwork0QuietTimerActive() {
    return detector().m_network0QuietTimer.isActive();
  }

  bool isNetwork2QuietTimerActive() {
    return detector().m_network2QuietTimer.isActive();
  }

  bool hadNetwork0Quiet() { return detector().m_network0QuietReached; }
  bool hadNetwork2Quiet() { return detector().m_network2QuietReached; }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      m_platform;

  static constexpr double kNetwork0QuietWindowSeconds =
      FirstMeaningfulPaintDetector::kNetwork0QuietWindowSeconds;
  static constexpr double kNetwork2QuietWindowSeconds =
      FirstMeaningfulPaintDetector::kNetwork2QuietWindowSeconds;

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

TEST_F(FirstMeaningfulPaintDetectorTest, NoFirstPaint) {
  simulateLayoutAndPaint(1);
  simulateNetworkStable();
  EXPECT_EQ(paintTiming().firstMeaningfulPaint(), 0.0);
}

TEST_F(FirstMeaningfulPaintDetectorTest, OneLayout) {
  paintTiming().markFirstContentfulPaint();
  simulateLayoutAndPaint(1);
  double afterPaint = advanceClockAndGetTime();
  EXPECT_EQ(paintTiming().firstMeaningfulPaint(), 0.0);
  simulateNetworkStable();
  EXPECT_GT(paintTiming().firstMeaningfulPaint(), paintTiming().firstPaint());
  EXPECT_LT(paintTiming().firstMeaningfulPaint(), afterPaint);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantSecond) {
  paintTiming().markFirstContentfulPaint();
  simulateLayoutAndPaint(1);
  double afterLayout1 = advanceClockAndGetTime();
  simulateLayoutAndPaint(10);
  double afterLayout2 = advanceClockAndGetTime();
  simulateNetworkStable();
  EXPECT_GT(paintTiming().firstMeaningfulPaint(), afterLayout1);
  EXPECT_LT(paintTiming().firstMeaningfulPaint(), afterLayout2);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantFirst) {
  paintTiming().markFirstContentfulPaint();
  simulateLayoutAndPaint(10);
  double afterLayout1 = advanceClockAndGetTime();
  simulateLayoutAndPaint(1);
  simulateNetworkStable();
  EXPECT_GT(paintTiming().firstMeaningfulPaint(), paintTiming().firstPaint());
  EXPECT_LT(paintTiming().firstMeaningfulPaint(), afterLayout1);
}

TEST_F(FirstMeaningfulPaintDetectorTest, FirstMeaningfulPaintCandidate) {
  paintTiming().markFirstContentfulPaint();
  EXPECT_EQ(paintTiming().firstMeaningfulPaintCandidate(), 0.0);
  simulateLayoutAndPaint(1);
  double afterPaint = advanceClockAndGetTime();
  // The first candidate gets ignored.
  EXPECT_EQ(paintTiming().firstMeaningfulPaintCandidate(), 0.0);
  simulateLayoutAndPaint(10);
  // The second candidate gets reported.
  EXPECT_GT(paintTiming().firstMeaningfulPaintCandidate(), afterPaint);
  double candidate = paintTiming().firstMeaningfulPaintCandidate();
  // The third candidate gets ignored since we already saw the first candidate.
  simulateLayoutAndPaint(10);
  EXPECT_EQ(paintTiming().firstMeaningfulPaintCandidate(), candidate);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       OnlyOneFirstMeaningfulPaintCandidateBeforeNetworkStable) {
  paintTiming().markFirstContentfulPaint();
  EXPECT_EQ(paintTiming().firstMeaningfulPaintCandidate(), 0.0);
  double beforePaint = advanceClockAndGetTime();
  simulateLayoutAndPaint(1);
  // The first candidate is initially ignored.
  EXPECT_EQ(paintTiming().firstMeaningfulPaintCandidate(), 0.0);
  simulateNetworkStable();
  // The networkStable then promotes the first candidate.
  EXPECT_GT(paintTiming().firstMeaningfulPaintCandidate(), beforePaint);
  double candidate = paintTiming().firstMeaningfulPaintCandidate();
  // The second candidate is then ignored.
  simulateLayoutAndPaint(10);
  EXPECT_EQ(paintTiming().firstMeaningfulPaintCandidate(), candidate);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       NetworkStableBeforeFirstContentfulPaint) {
  paintTiming().markFirstPaint();
  simulateLayoutAndPaint(1);
  simulateNetworkStable();
  EXPECT_EQ(paintTiming().firstMeaningfulPaint(), 0.0);
  paintTiming().markFirstContentfulPaint();
  simulateNetworkStable();
  EXPECT_NE(paintTiming().firstMeaningfulPaint(), 0.0);
}

TEST_F(FirstMeaningfulPaintDetectorTest,
       FirstMeaningfulPaintShouldNotBeBeforeFirstContentfulPaint) {
  paintTiming().markFirstPaint();
  simulateLayoutAndPaint(10);
  paintTiming().markFirstContentfulPaint();
  simulateNetworkStable();
  EXPECT_GE(paintTiming().firstMeaningfulPaint(),
            paintTiming().firstContentfulPaint());
}

TEST_F(FirstMeaningfulPaintDetectorTest, Network2QuietThen0Quiet) {
  paintTiming().markFirstContentfulPaint();

  simulateLayoutAndPaint(1);
  double afterFirstPaint = advanceClockAndGetTime();
  simulateNetwork2Quiet();

  simulateLayoutAndPaint(10);
  simulateNetwork0Quiet();

  // The first paint is FirstMeaningfulPaint.
  EXPECT_GT(paintTiming().firstMeaningfulPaint(), 0.0);
  EXPECT_LT(paintTiming().firstMeaningfulPaint(), afterFirstPaint);
}

TEST_F(FirstMeaningfulPaintDetectorTest, Network0QuietThen2Quiet) {
  paintTiming().markFirstContentfulPaint();

  simulateLayoutAndPaint(1);
  double afterFirstPaint = advanceClockAndGetTime();
  simulateNetwork0Quiet();

  simulateLayoutAndPaint(10);
  double afterSecondPaint = advanceClockAndGetTime();
  simulateNetwork2Quiet();

  // The second paint is FirstMeaningfulPaint.
  EXPECT_GT(paintTiming().firstMeaningfulPaint(), afterFirstPaint);
  EXPECT_LT(paintTiming().firstMeaningfulPaint(), afterSecondPaint);
}

TEST_F(FirstMeaningfulPaintDetectorTest, Network0QuietTimer) {
  paintTiming().markFirstContentfulPaint();

  setActiveConnections(1);
  EXPECT_FALSE(isNetwork0QuietTimerActive());

  setActiveConnections(0);
  m_platform->runForPeriodSeconds(kNetwork0QuietWindowSeconds - 0.1);
  EXPECT_TRUE(isNetwork0QuietTimerActive());
  EXPECT_FALSE(hadNetwork0Quiet());

  setActiveConnections(0);  // This should reset the 0-quiet timer.
  m_platform->runForPeriodSeconds(kNetwork0QuietWindowSeconds - 0.1);
  EXPECT_TRUE(isNetwork0QuietTimerActive());
  EXPECT_FALSE(hadNetwork0Quiet());

  m_platform->runForPeriodSeconds(0.1001);
  EXPECT_TRUE(hadNetwork0Quiet());
}

TEST_F(FirstMeaningfulPaintDetectorTest, Network2QuietTimer) {
  paintTiming().markFirstContentfulPaint();

  setActiveConnections(3);
  EXPECT_FALSE(isNetwork2QuietTimerActive());

  setActiveConnections(2);
  m_platform->runForPeriodSeconds(kNetwork2QuietWindowSeconds - 0.1);
  EXPECT_TRUE(isNetwork2QuietTimerActive());
  EXPECT_FALSE(hadNetwork2Quiet());

  setActiveConnections(2);  // This should reset the 2-quiet timer.
  m_platform->runForPeriodSeconds(kNetwork2QuietWindowSeconds - 0.1);
  EXPECT_TRUE(isNetwork2QuietTimerActive());
  EXPECT_FALSE(hadNetwork2Quiet());

  setActiveConnections(1);  // This should not reset the 2-quiet timer.
  m_platform->runForPeriodSeconds(0.1001);
  EXPECT_TRUE(hadNetwork2Quiet());
}

}  // namespace blink
