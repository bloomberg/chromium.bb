// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FirstMeaningfulPaintDetector.h"

#include "core/paint/PaintTiming.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/scheduler/test/fake_web_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

class FirstMeaningfulPaintDetectorTest : public testing::Test {
 protected:
  void SetUp() override {
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    s_timeElapsed = 0.0;
    m_originalTimeFunction = setTimeFunctionsForTesting(returnMockTime);
    m_taskRunner = adoptRef(new scheduler::FakeWebTaskRunner);
    detector().m_network0QuietTimer.moveToNewTaskRunner(m_taskRunner);
    detector().m_network2QuietTimer.moveToNewTaskRunner(m_taskRunner);
  }

  void TearDown() override {
    setTimeFunctionsForTesting(m_originalTimeFunction);
  }

  Document& document() { return m_dummyPageHolder->document(); }
  PaintTiming& paintTiming() { return PaintTiming::from(document()); }
  FirstMeaningfulPaintDetector& detector() {
    return paintTiming().firstMeaningfulPaintDetector();
  }

  void simulateLayoutAndPaint(int newElements) {
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
    double time0 = 0.0;
    double time2 = 0.0;
    m_taskRunner->setTime(returnMockTime());
    if (isNetwork0QuietTimerActive())
      time0 = detector().m_network0QuietTimer.nextFireInterval();
    if (isNetwork2QuietTimerActive())
      time2 = detector().m_network2QuietTimer.nextFireInterval();

    detector().setNetworkQuietTimers(connections);

    m_0QuietTimerRestarted =
        isNetwork0QuietTimerActive() &&
        detector().m_network0QuietTimer.nextFireInterval() != time0;
    m_2QuietTimerRestarted =
        isNetwork2QuietTimerActive() &&
        detector().m_network2QuietTimer.nextFireInterval() != time2;
  }

  bool isNetwork0QuietTimerActive() {
    return detector().m_network0QuietTimer.isActive();
  }

  bool isNetwork2QuietTimerActive() {
    return detector().m_network2QuietTimer.isActive();
  }

  bool isNetwork0QuietTimerRestarted() { return m_0QuietTimerRestarted; }
  bool isNetwork2QuietTimerRestarted() { return m_2QuietTimerRestarted; }

 private:
  static double returnMockTime() {
    s_timeElapsed += 1.0;
    return s_timeElapsed;
  }

  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  RefPtr<scheduler::FakeWebTaskRunner> m_taskRunner;
  TimeFunction m_originalTimeFunction;
  bool m_0QuietTimerRestarted;
  bool m_2QuietTimerRestarted;
  static double s_timeElapsed;
};

double FirstMeaningfulPaintDetectorTest::s_timeElapsed;

TEST_F(FirstMeaningfulPaintDetectorTest, NoFirstPaint) {
  simulateLayoutAndPaint(1);
  simulateNetworkStable();
  EXPECT_EQ(paintTiming().firstMeaningfulPaint(), 0.0);
}

TEST_F(FirstMeaningfulPaintDetectorTest, OneLayout) {
  paintTiming().markFirstContentfulPaint();
  simulateLayoutAndPaint(1);
  double afterPaint = monotonicallyIncreasingTime();
  EXPECT_EQ(paintTiming().firstMeaningfulPaint(), 0.0);
  simulateNetworkStable();
  EXPECT_GT(paintTiming().firstMeaningfulPaint(), paintTiming().firstPaint());
  EXPECT_LT(paintTiming().firstMeaningfulPaint(), afterPaint);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantSecond) {
  paintTiming().markFirstContentfulPaint();
  simulateLayoutAndPaint(1);
  double afterLayout1 = monotonicallyIncreasingTime();
  simulateLayoutAndPaint(10);
  double afterLayout2 = monotonicallyIncreasingTime();
  simulateNetworkStable();
  EXPECT_GT(paintTiming().firstMeaningfulPaint(), afterLayout1);
  EXPECT_LT(paintTiming().firstMeaningfulPaint(), afterLayout2);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantFirst) {
  paintTiming().markFirstContentfulPaint();
  simulateLayoutAndPaint(10);
  double afterLayout1 = monotonicallyIncreasingTime();
  simulateLayoutAndPaint(1);
  simulateNetworkStable();
  EXPECT_GT(paintTiming().firstMeaningfulPaint(), paintTiming().firstPaint());
  EXPECT_LT(paintTiming().firstMeaningfulPaint(), afterLayout1);
}

TEST_F(FirstMeaningfulPaintDetectorTest, FirstMeaningfulPaintCandidate) {
  paintTiming().markFirstContentfulPaint();
  EXPECT_EQ(paintTiming().firstMeaningfulPaintCandidate(), 0.0);
  simulateLayoutAndPaint(1);
  double afterPaint = monotonicallyIncreasingTime();
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
  double afterFirstPaint = monotonicallyIncreasingTime();
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
  double afterFirstPaint = monotonicallyIncreasingTime();
  simulateNetwork0Quiet();

  simulateLayoutAndPaint(10);
  double afterSecondPaint = monotonicallyIncreasingTime();
  simulateNetwork2Quiet();

  // The second paint is FirstMeaningfulPaint.
  EXPECT_GT(paintTiming().firstMeaningfulPaint(), afterFirstPaint);
  EXPECT_LT(paintTiming().firstMeaningfulPaint(), afterSecondPaint);
}

TEST_F(FirstMeaningfulPaintDetectorTest, NetworkQuietTimers) {
  setActiveConnections(3);
  EXPECT_FALSE(isNetwork0QuietTimerActive());
  EXPECT_FALSE(isNetwork2QuietTimerActive());

  setActiveConnections(2);
  EXPECT_FALSE(isNetwork0QuietTimerActive());
  EXPECT_TRUE(isNetwork2QuietTimerActive());

  setActiveConnections(1);
  EXPECT_FALSE(isNetwork0QuietTimerActive());
  EXPECT_TRUE(isNetwork2QuietTimerActive());
  EXPECT_FALSE(isNetwork2QuietTimerRestarted());

  setActiveConnections(2);
  EXPECT_TRUE(isNetwork2QuietTimerRestarted());

  setActiveConnections(0);
  EXPECT_TRUE(isNetwork0QuietTimerActive());
  EXPECT_TRUE(isNetwork2QuietTimerActive());
  EXPECT_FALSE(isNetwork2QuietTimerRestarted());

  setActiveConnections(0);
  EXPECT_TRUE(isNetwork0QuietTimerActive());
  EXPECT_TRUE(isNetwork0QuietTimerRestarted());
  EXPECT_TRUE(isNetwork2QuietTimerActive());
  EXPECT_FALSE(isNetwork2QuietTimerRestarted());
}

}  // namespace blink
