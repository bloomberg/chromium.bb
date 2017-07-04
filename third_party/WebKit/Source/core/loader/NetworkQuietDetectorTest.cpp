// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/NetworkQuietDetector.h"

#include "core/testing/DummyPageHolder.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class NetworkQuietDetectorTest : public ::testing::Test {
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
  NetworkQuietDetector& Detector() {
    return NetworkQuietDetector::From(GetDocument());
  }

  void SimulateNetworkStable() {
    GetDocument().SetParsingState(Document::kFinishedParsing);
    Detector().NetworkQuietTimerFired(nullptr);
  }

  void SimulateNetworkQuiet() {
    GetDocument().SetParsingState(Document::kFinishedParsing);
    Detector().NetworkQuietTimerFired(nullptr);
  }

  void SetActiveConnections(int connections) {
    Detector().SetNetworkQuietTimers(connections);
  }

  bool IsNetworkQuietTimerActive() {
    return Detector().network_quiet_timer_.IsActive();
  }

  bool HadNetworkQuiet() { return Detector().network_quiet_reached_; }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

  static constexpr double kNetworkQuietWindowSeconds =
      NetworkQuietDetector::kNetworkQuietWindowSeconds;

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(NetworkQuietDetectorTest, NetworkQuietTimer) {
  SetActiveConnections(3);
  EXPECT_FALSE(IsNetworkQuietTimerActive());

  SetActiveConnections(2);
  platform_->RunForPeriodSeconds(kNetworkQuietWindowSeconds - 0.1);
  EXPECT_TRUE(IsNetworkQuietTimerActive());
  EXPECT_FALSE(HadNetworkQuiet());

  SetActiveConnections(2);  // This should reset the quiet timer.
  platform_->RunForPeriodSeconds(kNetworkQuietWindowSeconds - 0.1);
  EXPECT_TRUE(IsNetworkQuietTimerActive());
  EXPECT_FALSE(HadNetworkQuiet());

  SetActiveConnections(1);  // This should not reset the quiet timer.
  platform_->RunForPeriodSeconds(0.1001);
  EXPECT_TRUE(HadNetworkQuiet());
}

}  // namespace blink
