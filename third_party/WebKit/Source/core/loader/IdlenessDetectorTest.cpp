// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/IdlenessDetector.h"

#include "core/testing/DummyPageHolder.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class IdlenessDetectorTest : public ::testing::Test {
 protected:
  void SetUp() {
    platform_time_ = 1;
    platform_->AdvanceClockSeconds(platform_time_);
    page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  }

  IdlenessDetector* Detector() {
    return page_holder_->GetFrame().GetIdlenessDetector();
  }

  bool IsNetworkQuietTimerActive() {
    return Detector()->network_quiet_timer_.IsActive();
  }

  bool HadNetworkQuiet() {
    return Detector()->network_2_quiet_ == -1 &&
           Detector()->network_0_quiet_ == -1;
  }

  double NetworkQuietStartTime() {
    return Detector()->network_2_quiet_start_time_;
  }

  void WillProcessTask(double start_time) {
    DCHECK(start_time >= platform_time_);
    platform_->AdvanceClockSeconds(start_time - platform_time_);
    platform_time_ = start_time;
    Detector()->WillProcessTask(start_time);
  }

  void DidProcessTask(double start_time, double end_time) {
    DCHECK(start_time < end_time);
    platform_->AdvanceClockSeconds(end_time - start_time);
    platform_time_ = end_time;
    Detector()->DidProcessTask(start_time, end_time);
  }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

 private:
  double platform_time_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(IdlenessDetectorTest, NetworkQuietBasic) {
  EXPECT_TRUE(IsNetworkQuietTimerActive());

  WillProcessTask(1);
  DidProcessTask(1, 1.01);

  WillProcessTask(1.52);
  EXPECT_TRUE(HadNetworkQuiet());
  DidProcessTask(1.52, 1.53);
}

TEST_F(IdlenessDetectorTest, NetworkQuietWithLongTask) {
  EXPECT_TRUE(IsNetworkQuietTimerActive());

  WillProcessTask(1);
  DidProcessTask(1, 1.01);

  WillProcessTask(1.02);
  DidProcessTask(1.02, 1.6);
  EXPECT_FALSE(HadNetworkQuiet());

  WillProcessTask(2.11);
  EXPECT_TRUE(HadNetworkQuiet());
  DidProcessTask(2.11, 2.12);
}

TEST_F(IdlenessDetectorTest, NetworkQuietWatchdogTimerFired) {
  EXPECT_TRUE(IsNetworkQuietTimerActive());

  WillProcessTask(1);
  DidProcessTask(1, 1.01);

  platform_->RunForPeriodSeconds(3);
  EXPECT_FALSE(IsNetworkQuietTimerActive());
  EXPECT_TRUE(HadNetworkQuiet());
}

}  // namespace blink
