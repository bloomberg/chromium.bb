// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/frame_processing_time_estimator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class TestFrameProcessingTimeEstimator : public FrameProcessingTimeEstimator {
 public:
  TestFrameProcessingTimeEstimator() = default;
  ~TestFrameProcessingTimeEstimator() override = default;

  void TimeElapseMs(int delta) {
    now_ += base::TimeDelta::FromMilliseconds(delta);
  }

 private:
  base::TimeTicks Now() const override { return now_; }

  base::TimeTicks now_ = base::TimeTicks::Now();
};

WebrtcVideoEncoder::EncodedFrame CreateEncodedFrame(bool key_frame, int size) {
  WebrtcVideoEncoder::EncodedFrame result;
  result.key_frame = key_frame;
  result.data.assign(static_cast<size_t>(size), 'A');
  return result;
}

}  // namespace

TEST(FrameProcessingTimeEstimatorTest, EstimateDeltaAndKeyFrame) {
  TestFrameProcessingTimeEstimator estimator;
  for (int i = 0; i < 10; i++) {
    estimator.StartFrame();
    if (i % 5 == 0) {
      // Fake a key-frame.
      estimator.TimeElapseMs(10);
      estimator.FinishFrame(CreateEncodedFrame(true, 100));
    } else {
      // Fake a delta-frame.
      estimator.TimeElapseMs(1);
      estimator.FinishFrame(CreateEncodedFrame(false, 50));
    }
  }

  estimator.SetBandwidthKbps(50);
  estimator.SetBandwidthKbps(150);
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(10),
            estimator.EstimatedProcessingTime(true));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1),
            estimator.EstimatedProcessingTime(false));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(8),
            estimator.EstimatedTransitTime(true));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(4),
            estimator.EstimatedTransitTime(false));
}

TEST(FrameProcessingTimeEstimatorTest, NegativeBandwidthShouldBeDropped) {
  TestFrameProcessingTimeEstimator estimator;
  estimator.StartFrame();
  estimator.TimeElapseMs(10);
  estimator.FinishFrame(CreateEncodedFrame(true, 100));
  estimator.SetBandwidthKbps(100);
  estimator.SetBandwidthKbps(-100);
  ASSERT_EQ(base::TimeDelta::FromMilliseconds(8),
            estimator.EstimatedTransitTime(true));
}

TEST(FrameProcessingTimeEstimatorTest, ShouldNotReturn0WithOnlyKeyFrames) {
  TestFrameProcessingTimeEstimator estimator;
  estimator.StartFrame();
  estimator.TimeElapseMs(10);
  estimator.FinishFrame(CreateEncodedFrame(true, 100));
  estimator.SetBandwidthKbps(100);
  ASSERT_EQ(base::TimeDelta::FromMilliseconds(10),
            estimator.EstimatedProcessingTime(true));
  ASSERT_EQ(base::TimeDelta::FromMilliseconds(8),
            estimator.EstimatedTransitTime(true));
  ASSERT_EQ(base::TimeDelta::FromMilliseconds(10),
            estimator.EstimatedProcessingTime(false));
  ASSERT_EQ(base::TimeDelta::FromMilliseconds(8),
            estimator.EstimatedTransitTime(false));
}

TEST(FrameProcessingTimeEstimatorTest, ShouldNotReturn0WithOnlyDeltaFrames) {
  TestFrameProcessingTimeEstimator estimator;
  estimator.StartFrame();
  estimator.TimeElapseMs(1);
  estimator.FinishFrame(CreateEncodedFrame(false, 50));
  estimator.SetBandwidthKbps(100);
  ASSERT_EQ(base::TimeDelta::FromMilliseconds(1),
            estimator.EstimatedProcessingTime(false));
  ASSERT_EQ(base::TimeDelta::FromMilliseconds(4),
            estimator.EstimatedTransitTime(false));
  ASSERT_EQ(base::TimeDelta::FromMilliseconds(1),
            estimator.EstimatedProcessingTime(true));
  ASSERT_EQ(base::TimeDelta::FromMilliseconds(4),
            estimator.EstimatedTransitTime(true));
}

TEST(FrameProcessingTimeEstimatorTest, ShouldReturn0WithNoRecords) {
  TestFrameProcessingTimeEstimator estimator;
  ASSERT_EQ(base::TimeDelta(), estimator.EstimatedProcessingTime(true));
  ASSERT_EQ(base::TimeDelta(), estimator.EstimatedProcessingTime(false));
}

}  // namespace remoting
