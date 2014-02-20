// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capture_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

static const int kTestInputs[] = { 100, 50, 30, 20, 10, 30, 60, 80 };
static const int kMinumumFrameIntervalMs = 50;

TEST(CaptureSchedulerTest, SingleSampleSameTimes) {
  const int kTestResults[][arraysize(kTestInputs)] = {
    { 400, 200, 120, 80, 50, 120, 240, 320 }, // One core.
    { 200, 100, 60, 50, 50, 60, 120, 160 },   // Two cores.
    { 100, 50, 50, 50, 50, 50, 60, 80 },      // Four cores.
    { 50, 50, 50, 50, 50, 50, 50, 50 }        // Eight cores.
  };

  for (size_t i = 0; i < arraysize(kTestResults); ++i) {
    for (size_t j = 0; j < arraysize(kTestInputs); ++j) {
      CaptureScheduler scheduler;
      scheduler.SetNumOfProcessorsForTest(1 << i);
      scheduler.set_minimum_interval(
          base::TimeDelta::FromMilliseconds(kMinumumFrameIntervalMs));
      scheduler.RecordCaptureTime(
          base::TimeDelta::FromMilliseconds(kTestInputs[j]));
      scheduler.RecordEncodeTime(
          base::TimeDelta::FromMilliseconds(kTestInputs[j]));
      EXPECT_EQ(kTestResults[i][j],
                scheduler.NextCaptureDelay().InMilliseconds()) << i  << " "<< j;
    }
  }
}

TEST(CaptureSchedulerTest, SingleSampleDifferentTimes) {
  const int kTestResults[][arraysize(kTestInputs)] = {
    { 360, 220, 120, 60, 60, 120, 220, 360 }, // One core.
    { 180, 110, 60, 50, 50, 60, 110, 180 },   // Two cores.
    { 90, 55, 50, 50, 50, 50, 55, 90 },       // Four cores.
    { 50, 50, 50, 50, 50, 50, 50, 50 }        // Eight cores.
  };

  for (size_t i = 0; i < arraysize(kTestResults); ++i) {
    for (size_t j = 0; j < arraysize(kTestInputs); ++j) {
      CaptureScheduler scheduler;
      scheduler.SetNumOfProcessorsForTest(1 << i);
      scheduler.set_minimum_interval(
          base::TimeDelta::FromMilliseconds(kMinumumFrameIntervalMs));
      scheduler.RecordCaptureTime(
          base::TimeDelta::FromMilliseconds(kTestInputs[j]));
      scheduler.RecordEncodeTime(
          base::TimeDelta::FromMilliseconds(
              kTestInputs[arraysize(kTestInputs) - 1 - j]));
      EXPECT_EQ(kTestResults[i][j],
                scheduler.NextCaptureDelay().InMilliseconds());
    }
  }
}

TEST(CaptureSchedulerTest, RollingAverageDifferentTimes) {
  const int kTestResults[][arraysize(kTestInputs)] = {
    { 360, 290, 233, 133, 80, 80, 133, 233 }, // One core.
    { 180, 145, 116, 66, 50, 50, 66, 116 },   // Two cores.
    { 90, 72, 58, 50, 50, 50, 50, 58 },       // Four cores.
    { 50, 50, 50, 50, 50, 50, 50, 50 }        // Eight cores.
  };

  for (size_t i = 0; i < arraysize(kTestResults); ++i) {
    CaptureScheduler scheduler;
    scheduler.SetNumOfProcessorsForTest(1 << i);
    scheduler.set_minimum_interval(
        base::TimeDelta::FromMilliseconds(kMinumumFrameIntervalMs));
    for (size_t j = 0; j < arraysize(kTestInputs); ++j) {
      scheduler.RecordCaptureTime(
          base::TimeDelta::FromMilliseconds(kTestInputs[j]));
      scheduler.RecordEncodeTime(
          base::TimeDelta::FromMilliseconds(
              kTestInputs[arraysize(kTestInputs) - 1 - j]));
      EXPECT_EQ(kTestResults[i][j],
                scheduler.NextCaptureDelay().InMilliseconds());
    }
  }
}

}  // namespace remoting
