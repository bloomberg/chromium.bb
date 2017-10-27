// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/InteractiveDetector.h"

#include "core/dom/Document.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class NetworkActivityCheckerForTest
    : public InteractiveDetector::NetworkActivityChecker {
 public:
  NetworkActivityCheckerForTest(Document* document)
      : InteractiveDetector::NetworkActivityChecker(document) {}

  virtual void SetActiveConnections(int active_connections) {
    active_connections_ = active_connections;
  };
  virtual int GetActiveConnections() { return active_connections_; }

 private:
  int active_connections_ = 0;
};

struct TaskTiming {
  double start;
  double end;
  TaskTiming(double start, double end) : start(start), end(end) {}
};

static constexpr char kSupplementName[] = "InteractiveDetector";

class InteractiveDetectorTest : public ::testing::Test {
 public:
  InteractiveDetectorTest() : document_(Document::CreateForTest()) {
    InteractiveDetector* detector = new InteractiveDetector(
        *document_, new NetworkActivityCheckerForTest(document_));
    Supplement<Document>::ProvideTo(*document_, kSupplementName, detector);
    detector_ = detector;
  }

 protected:
  void SetUp() override { platform_->AdvanceClockSeconds(1); }

  Document* GetDocument() { return document_; }

  InteractiveDetector* GetDetector() { return detector_; }

  NetworkActivityCheckerForTest* GetNetworkActivityChecker() {
    // We know in this test context that network_activity_checker_ is an
    // instance of NetworkActivityCheckerForTest, so this static_cast is safe.
    return static_cast<NetworkActivityCheckerForTest*>(
        detector_->network_activity_checker_.get());
  }

  void SimulateNavigationStart(double nav_start_time) {
    RunTillTimestamp(nav_start_time);
    detector_->SetNavigationStartTime(nav_start_time);
  }

  void SimulateLongTask(double start, double end) {
    CHECK(end - start >= 0.05);
    RunTillTimestamp(end);
    detector_->OnLongTaskDetected(start, end);
  }

  void SimulateDOMContentLoadedEnd(double dcl_time) {
    RunTillTimestamp(dcl_time);
    detector_->OnDomContentLoadedEnd(dcl_time);
  }

  void SimulateFMPDetected(double fmp_time, double detection_time) {
    RunTillTimestamp(detection_time);
    detector_->OnFirstMeaningfulPaintDetected(fmp_time);
  }

  void RunTillTimestamp(double target_time) {
    double current_time = MonotonicallyIncreasingTime();
    platform_->RunForPeriodSeconds(std::max(0.0, target_time - current_time));
  }

  int GetActiveConnections() {
    return GetNetworkActivityChecker()->GetActiveConnections();
  }

  void SetActiveConnections(int active_connections) {
    GetNetworkActivityChecker()->SetActiveConnections(active_connections);
  }

  void SimulateResourceLoadBegin(double load_begin_time) {
    RunTillTimestamp(load_begin_time);
    detector_->OnResourceLoadBegin(load_begin_time);
    // ActiveConnections is incremented after detector runs OnResourceLoadBegin;
    SetActiveConnections(GetActiveConnections() + 1);
  }

  void SimulateResourceLoadEnd(double load_finish_time) {
    RunTillTimestamp(load_finish_time);
    int active_connections = GetActiveConnections();
    SetActiveConnections(active_connections - 1);
    detector_->OnResourceLoadEnd(load_finish_time);
  }

  double GetInteractiveTime() { return detector_->GetInteractiveTime(); }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

  Persistent<Document> document_;
  Persistent<InteractiveDetector> detector_;
};

// Note: The tests currently assume kTimeToInteractiveWindowSeconds is 5
// seconds. The window size is unlikely to change, and this makes the test
// scenarios significantly easier to write.

// Note: Some of the tests are named W_X_Y_Z, where W, X, Y, Z can any of the
// following events:
// FMP: First Meaningful Paint
// DCL: DomContentLoadedEnd
// FmpDetect: Detection of FMP. FMP is not detected in realtime.
// LT: Long Task
// The name shows the ordering of these events in the test.

TEST_F(InteractiveDetectorTest, FMP_DCL_FmpDetect) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + 3.0);
  SimulateFMPDetected(/* fmp_time */ t0 + 5.0, /* detection_time */ t0 + 7.0);
  // Run until 5 seconds after FMP.
  RunTillTimestamp((t0 + 5.0) + 5.0 + 0.1);
  // Reached TTI at FMP.
  EXPECT_EQ(GetInteractiveTime(), t0 + 5.0);
}

TEST_F(InteractiveDetectorTest, DCL_FMP_FmpDetect) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + 5.0);
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 7.0);
  // Run until 5 seconds after FMP.
  RunTillTimestamp((t0 + 3.0) + 5.0 + 0.1);
  // Reached TTI at DCL.
  EXPECT_EQ(GetInteractiveTime(), t0 + 5.0);
}

TEST_F(InteractiveDetectorTest, InstantDetectionAtFmpDetectIfPossible) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + 5.0);
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 10.0);
  // Although we just detected FMP, the FMP timestamp is more than
  // kTimeToInteractiveWindowSeconds earlier. We should instantaneously
  // detect that we reached TTI at DCL.
  EXPECT_EQ(GetInteractiveTime(), t0 + 5.0);
}

TEST_F(InteractiveDetectorTest, FmpDetectFiresAfterLateLongTask) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + 3.0);
  SimulateLongTask(t0 + 9.0, t0 + 9.1);
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 10.0);
  // There is a 5 second quiet window after fmp_time - the long task is 6s
  // seconds after fmp_time. We should instantly detect we reached TTI at FMP.
  EXPECT_EQ(GetInteractiveTime(), t0 + 3.0);
}

TEST_F(InteractiveDetectorTest, FMP_FmpDetect_DCL) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 5.0);
  SimulateDOMContentLoadedEnd(t0 + 9.0);
  // TTI reached at DCL.
  EXPECT_EQ(GetInteractiveTime(), t0 + 9.0);
}

TEST_F(InteractiveDetectorTest, LongTaskBeforeFMPDoesNotAffectTTI) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + 3.0);
  SimulateLongTask(t0 + 5.1, t0 + 5.2);
  SimulateFMPDetected(/* fmp_time */ t0 + 8.0, /* detection_time */ t0 + 9.0);
  // Run till 5 seconds after FMP.
  RunTillTimestamp((t0 + 8.0) + 5.0 + 0.1);
  // TTI reached at FMP.
  EXPECT_EQ(GetInteractiveTime(), t0 + 8.0);
}

TEST_F(InteractiveDetectorTest, DCLDoesNotResetTimer) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 4.0);
  SimulateLongTask(t0 + 5.0, t0 + 5.1);
  SimulateDOMContentLoadedEnd(t0 + 8.0);
  // Run till 5 seconds after long task end.
  RunTillTimestamp((t0 + 5.1) + 5.0 + 0.1);
  // TTI Reached at DCL.
  EXPECT_EQ(GetInteractiveTime(), t0 + 8.0);
}

TEST_F(InteractiveDetectorTest, DCL_FMP_FmpDetect_LT) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + 3.0);
  SimulateFMPDetected(/* fmp_time */ t0 + 4.0, /* detection_time */ t0 + 5.0);
  SimulateLongTask(t0 + 7.0, t0 + 7.1);
  // Run till 5 seconds after long task end.
  RunTillTimestamp((t0 + 7.1) + 5.0 + 0.1);
  // TTI reached at long task end.
  EXPECT_EQ(GetInteractiveTime(), t0 + 7.1);
}

TEST_F(InteractiveDetectorTest, DCL_FMP_LT_FmpDetect) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + 3.0);
  SimulateLongTask(t0 + 7.0, t0 + 7.1);
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 5.0);
  // Run till 5 seconds after long task end.
  RunTillTimestamp((t0 + 7.1) + 5.0 + 0.1);
  // TTI reached at long task end.
  EXPECT_EQ(GetInteractiveTime(), t0 + 7.1);
}

TEST_F(InteractiveDetectorTest, FMP_FmpDetect_LT_DCL) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 4.0);
  SimulateLongTask(t0 + 7.0, t0 + 7.1);
  SimulateDOMContentLoadedEnd(t0 + 8.0);
  // Run till 5 seconds after long task end.
  RunTillTimestamp((t0 + 7.1) + 5.0 + 0.1);
  // TTI reached at DCL. Note that we do not need to wait for DCL + 5 seconds.
  EXPECT_EQ(GetInteractiveTime(), t0 + 8.0);
}

TEST_F(InteractiveDetectorTest, DclIsMoreThan5sAfterFMP) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  // Network is forever quiet for this test.
  SetActiveConnections(1);
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 4.0);
  SimulateLongTask(t0 + 7.0, t0 + 7.1);  // Long task 1.
  SimulateDOMContentLoadedEnd(t0 + 10.0);
  // Have not reached TTI yet.
  EXPECT_EQ(GetInteractiveTime(), 0.0);
  SimulateLongTask(t0 + 11.0, t0 + 11.1);  // Long task 2.
  // Run till long task 2 end + 5 seconds.
  RunTillTimestamp((t0 + 11.1) + 5.0 + 0.1);
  // TTI reached at long task 2 end.
  EXPECT_EQ(GetInteractiveTime(), (t0 + 11.1));
}

TEST_F(InteractiveDetectorTest, NetworkBusyBlocksTTIEvenWhenMainThreadQuiet) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + 2.0);
  SimulateResourceLoadBegin(t0 + 3.4);  // Request 2 start.
  SimulateResourceLoadBegin(t0 + 3.5);  // Request 3 start. Network busy.
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 4.0);
  SimulateLongTask(t0 + 7.0, t0 + 7.1);  // Long task 1.
  SimulateResourceLoadEnd(t0 + 12.2);    // Network quiet.
  // Network busy kept page from reaching TTI..
  EXPECT_EQ(GetInteractiveTime(), 0.0);
  SimulateLongTask(t0 + 13.0, t0 + 13.1);  // Long task 2.
  // Run till 5 seconds after long task 2 end.
  RunTillTimestamp((t0 + 13.1) + 5.0 + 0.1);
  EXPECT_EQ(GetInteractiveTime(), (t0 + 13.1));
}

TEST_F(InteractiveDetectorTest, LongEnoughQuietWindowBetweenFMPAndFmpDetect) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + 2.0);
  SimulateLongTask(t0 + 2.1, t0 + 2.2);  // Long task 1.
  SimulateLongTask(t0 + 8.2, t0 + 8.3);  // Long task 2.
  SimulateResourceLoadBegin(t0 + 8.4);   // Request 2 start.
  SimulateResourceLoadBegin(t0 + 8.5);   // Request 3 start. Network busy.
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 10.0);
  // Even though network is currently busy and we have long task finishing
  // recently, we should be able to detect that the page already achieved TTI at
  // FMP.
  EXPECT_EQ(GetInteractiveTime(), t0 + 3.0);
}

TEST_F(InteractiveDetectorTest, NetworkBusyEndIsNotTTI) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + 2.0);
  SimulateResourceLoadBegin(t0 + 3.4);  // Request 2 start.
  SimulateResourceLoadBegin(t0 + 3.5);  // Request 3 start. Network busy.
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 4.0);
  SimulateLongTask(t0 + 7.0, t0 + 7.1);    // Long task 1.
  SimulateLongTask(t0 + 13.0, t0 + 13.1);  // Long task 2.
  SimulateResourceLoadEnd(t0 + 14.0);      // Network quiet.
  // Run till 5 seconds after network busy end.
  RunTillTimestamp((t0 + 14.0) + 5.0 + 0.1);
  // TTI reached at long task 2 end, NOT at network busy end.
  EXPECT_EQ(GetInteractiveTime(), t0 + 13.1);
}

TEST_F(InteractiveDetectorTest, LateLongTaskWithLateFMPDetection) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + 2.0);
  SimulateResourceLoadBegin(t0 + 3.4);     // Request 2 start.
  SimulateResourceLoadBegin(t0 + 3.5);     // Request 3 start. Network busy.
  SimulateLongTask(t0 + 7.0, t0 + 7.1);    // Long task 1.
  SimulateResourceLoadEnd(t0 + 8.0);       // Network quiet.
  SimulateLongTask(t0 + 14.0, t0 + 14.1);  // Long task 2.
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 20.0);
  // TTI reached at long task 1 end, NOT at long task 2 end.
  EXPECT_EQ(GetInteractiveTime(), t0 + 7.1);
}

TEST_F(InteractiveDetectorTest, IntermittentNetworkBusyBlocksTTI) {
  double t0 = MonotonicallyIncreasingTime();
  SimulateNavigationStart(t0);
  SetActiveConnections(1);
  SimulateDOMContentLoadedEnd(t0 + 2.0);
  SimulateFMPDetected(/* fmp_time */ t0 + 3.0, /* detection_time */ t0 + 4.0);
  SimulateLongTask(t0 + 7.0, t0 + 7.1);  // Long task 1.
  SimulateResourceLoadBegin(t0 + 7.9);   // Active connections: 2
  // Network busy start.
  SimulateResourceLoadBegin(t0 + 8.0);  // Active connections: 3.
  // Network busy end.
  SimulateResourceLoadEnd(t0 + 8.5);  // Active connections: 2.
  // Network busy start.
  SimulateResourceLoadBegin(t0 + 11.0);  // Active connections: 3.
  // Network busy end.
  SimulateResourceLoadEnd(t0 + 12.0);      // Active connections: 2.
  SimulateLongTask(t0 + 14.0, t0 + 14.1);  // Long task 2.
  // Run till 5 seconds after long task 2 end.
  RunTillTimestamp((t0 + 14.1) + 5.0 + 0.1);
  // TTI reached at long task 2 end.
  EXPECT_EQ(GetInteractiveTime(), t0 + 14.1);
}

class InteractiveDetectorTestWithDummyPage : public ::testing::Test {
 public:
  // Public because it's executed on a task queue.
  void DummyTaskWithDuration(double duration_seconds) {
    platform_->AdvanceClockSeconds(duration_seconds);
    dummy_task_end_time_ = MonotonicallyIncreasingTime();
  }

 protected:
  void SetUp() override {
    platform_->AdvanceClockSeconds(1);
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  }

  double GetDummyTaskEndTime() { return dummy_task_end_time_; }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  double dummy_task_end_time_ = 0.0;
};

TEST_F(InteractiveDetectorTestWithDummyPage, TaskLongerThan5sBlocksTTI) {
  double t0 = MonotonicallyIncreasingTime();
  InteractiveDetector* detector = InteractiveDetector::From(GetDocument());
  detector->SetNavigationStartTime(t0);
  platform_->RunForPeriodSeconds(4.0);

  // DummyPageHolder automatically fires DomContentLoadedEnd, but not First
  // Meaningful Paint. We therefore manually Invoking the listener on
  // InteractiveDetector.
  detector->OnFirstMeaningfulPaintDetected(t0 + 3.0);

  // Post a task with 6 seconds duration.
  platform_->CurrentThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(
          &InteractiveDetectorTestWithDummyPage::DummyTaskWithDuration,
          CrossThreadUnretained(this), 6.0));
  platform_->RunUntilIdle();

  // We should be able to detect TTI 5s after the end of long task.
  platform_->RunForPeriodSeconds(5.1);
  EXPECT_EQ(detector->GetInteractiveTime(), GetDummyTaskEndTime());
}

TEST_F(InteractiveDetectorTestWithDummyPage, LongTaskAfterTTIDoesNothing) {
  double t0 = MonotonicallyIncreasingTime();
  InteractiveDetector* detector = InteractiveDetector::From(GetDocument());
  detector->SetNavigationStartTime(t0);
  platform_->RunForPeriodSeconds(4.0);

  // DummyPageHolder automatically fires DomContentLoadedEnd, but not First
  // Meaningful Paint. We therefore manually Invoking the listener on
  // InteractiveDetector.
  detector->OnFirstMeaningfulPaintDetected(t0 + 3.0);

  // Long task 1.
  platform_->CurrentThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(
          &InteractiveDetectorTestWithDummyPage::DummyTaskWithDuration,
          CrossThreadUnretained(this), 0.1));
  platform_->RunUntilIdle();

  double long_task_1_end_time = GetDummyTaskEndTime();
  // We should be able to detect TTI 5s after the end of long task.
  platform_->RunForPeriodSeconds(5.1);
  EXPECT_EQ(detector->GetInteractiveTime(), long_task_1_end_time);

  // Long task 2.
  platform_->CurrentThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(
          &InteractiveDetectorTestWithDummyPage::DummyTaskWithDuration,
          CrossThreadUnretained(this), 0.1));

  platform_->RunUntilIdle();
  // Wait 5 seconds to see if TTI time changes.
  platform_->RunForPeriodSeconds(5.1);
  // TTI time should not change.
  EXPECT_EQ(detector->GetInteractiveTime(), long_task_1_end_time);
}

}  // namespace blink
