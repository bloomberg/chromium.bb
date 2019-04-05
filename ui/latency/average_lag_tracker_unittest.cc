// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/average_lag_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;

namespace ui {
namespace {

class AverageLagTrackerTest : public testing::Test {
 public:
  AverageLagTrackerTest() { ResetHistograms(); }

  void ResetHistograms() {
    histogram_tester_.reset(new base::HistogramTester());
  }

  const base::HistogramTester& histogram_tester() { return *histogram_tester_; }

  void SetUp() override {
    average_lag_tracker_ = std::make_unique<AverageLagTracker>();
  }

  void SyntheticTouchScrollBeginLatencyInfo(base::TimeTicks event_time,
                                            base::TimeTicks frame_time,
                                            float delta) {
    ui::LatencyInfo touch_latency(ui::SourceEventType::TOUCH);
    touch_latency.set_scroll_update_delta(delta);
    touch_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
        event_time, 1);
    touch_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT, event_time,
        1);
    average_lag_tracker_->AddLatencyInFrame(touch_latency, frame_time,
                                            "ScrollBegin");
  }

  void SyntheticTouchScrollUpdateLatencyInfo(base::TimeTicks event_time,
                                             base::TimeTicks frame_time,
                                             float delta) {
    ui::LatencyInfo touch_latency(ui::SourceEventType::TOUCH);
    touch_latency.set_scroll_update_delta(delta);
    touch_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT, event_time,
        1);
    touch_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT, event_time,
        1);
    average_lag_tracker_->AddLatencyInFrame(touch_latency, frame_time,
                                            "ScrollUpdate");
  }

 protected:
  std::unique_ptr<AverageLagTracker> average_lag_tracker_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

base::TimeTicks MillisecondsToTimeTicks(float t_ms) {
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(t_ms);
}

// Simulate a simple situation that events at every 10ms and start at t=15ms,
// frame swaps at every 10ms too and start at t=20ms and test we record one
// UMA for ScrollUpdate in one second.
TEST_F(AverageLagTrackerTest, OneSecondInterval) {
  base::TimeTicks event_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(5);
  base::TimeTicks frame_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(10);
  float scroll_delta = 10;

  // ScrollBegin
  event_time += base::TimeDelta::FromMilliseconds(10);  // 15ms
  frame_time += base::TimeDelta::FromMilliseconds(10);  // 20ms
  SyntheticTouchScrollBeginLatencyInfo(event_time, frame_time, scroll_delta);

  // Send 101 ScrollUpdate events to verify that there is 1 AverageLag record
  // per 1 second.
  const int kUpdates = 101;
  for (int i = 0; i < kUpdates; i++) {
    event_time += base::TimeDelta::FromMilliseconds(10);
    frame_time += base::TimeDelta::FromMilliseconds(10);
    // First 50 has positive delta, others negetive delta.
    const int sign = (i < kUpdates / 2) ? 1 : -1;
    SyntheticTouchScrollUpdateLatencyInfo(event_time, frame_time,
                                          sign * scroll_delta);
  }

  // ScrollBegin report_time is at 20ms, so the next ScrollUpdate report_time is
  // at 1020ms. The last event_time that finish this report should be later than
  // 1020ms.
  EXPECT_EQ(event_time,
            base::TimeTicks() + base::TimeDelta::FromMilliseconds(1025));
  EXPECT_EQ(frame_time,
            base::TimeTicks() + base::TimeDelta::FromMilliseconds(1030));

  // ScrollBegin AverageLag are the area between the event original component
  // (time=15ms, delta=10px) to the frame swap time (time=20ms, expect finger
  // position at delta=15px). The AverageLag scaled to 1 second is
  // (0.5*(10px+15px)*5ms)/5ms = 12.5px.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.ScrollBegin.Touch.AverageLag"),
              ElementsAre(Bucket(12, 1)));
  // This ScrollUpdate AverageLag are calculated as the finger uniformly scroll
  // 10px each frame. For scroll up/down frame, the Lag at the last frame swap
  // is 5px, and Lag at this frame swap is 15px. For the one changing direction,
  // the Lag is from 5 to 10 and down to 5 again. So total LagArea is 99 * 100,
  // plus 75. the AverageLag in 1 second is 9.975px.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.ScrollUpdate.Touch.AverageLag"),
              ElementsAre(Bucket(9, 1)));
  ResetHistograms();

  // Send another ScrollBegin to end the unfinished ScrollUpdate report.
  event_time += base::TimeDelta::FromMilliseconds(10);
  frame_time += base::TimeDelta::FromMilliseconds(10);
  SyntheticTouchScrollBeginLatencyInfo(event_time, frame_time, scroll_delta);

  // The last ScrollUpdate's lag is 8.75px and truncated to 8.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.ScrollUpdate.Touch.AverageLag"),
              ElementsAre(Bucket(8, 1)));
}

// Test the case that event's frame swap time is later than next event's
// creation time. (i.e, event at t=10ms will be dispatch at t=30ms, while next
// event is at t=20ms).
TEST_F(AverageLagTrackerTest, LargerLatency) {
  base::TimeTicks event_time = MillisecondsToTimeTicks(10);
  base::TimeTicks frame_time =
      event_time + base::TimeDelta::FromMilliseconds(20);
  float scroll_delta = 10;

  SyntheticTouchScrollBeginLatencyInfo(event_time, frame_time, scroll_delta);

  // Send 2 ScrollUpdate. The second one will record AverageLag.ScrollBegin as
  // it's event_time is larger or equal to ScrollBegin's frame_time.
  for (int i = 0; i < 2; i++) {
    event_time += base::TimeDelta::FromMilliseconds(10);
    frame_time = event_time + base::TimeDelta::FromMilliseconds(20);
    SyntheticTouchScrollUpdateLatencyInfo(event_time, frame_time, scroll_delta);
  }

  // ScrollBegin AveragLag are from t=10ms to t=30ms, with absolute scroll
  // position from 10 to 30. The AverageLag should be:
  // (0.5*(10px + 30px)*20ms/20ms) = 20px.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.ScrollBegin.Touch.AverageLag"),
              ElementsAre(Bucket(20, 1)));

  // Another ScrollBegin to flush unfinished frames.
  // event_time doesn't matter here because the previous frames' lag are
  // compute from their frame_time.
  event_time = MillisecondsToTimeTicks(1000);
  frame_time = MillisecondsToTimeTicks(1000);
  SyntheticTouchScrollBeginLatencyInfo(event_time, frame_time, scroll_delta);
  // The to unfinished frames' lag are (finger_positon-rendered_position)*time,
  // AverageLag is ((30px-10px)*10ms+(30px-20px)*10ms)/20ms = 15px.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.ScrollUpdate.Touch.AverageLag"),
              ElementsAre(Bucket(14, 1)));
}

// Test that multiple latency being flush in the same frame swap.
TEST_F(AverageLagTrackerTest, TwoLatencyInfoInSameFrame) {
  // ScrollBegin
  base::TimeTicks event_time = MillisecondsToTimeTicks(10);
  base::TimeTicks frame_time = MillisecondsToTimeTicks(20);
  SyntheticTouchScrollBeginLatencyInfo(event_time, frame_time,
                                       -10 /* scroll_delta */);

  // ScrollUpdate with event_time >= ScrollBegin frame_time will generate
  // a histogram for AverageLag.ScrollBegin.
  event_time = MillisecondsToTimeTicks(20);
  frame_time = MillisecondsToTimeTicks(30);
  SyntheticTouchScrollUpdateLatencyInfo(event_time, frame_time,
                                        -10 /* scroll_delta */);

  // Absolute position from -10 to -20. The AverageLag should be:
  // (0.5*(10px + 20px)*10ms/10ms) = 15px.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.ScrollBegin.Touch.AverageLag"),
              ElementsAre(Bucket(14, 1)));

  event_time = MillisecondsToTimeTicks(25);
  frame_time = MillisecondsToTimeTicks(30);
  SyntheticTouchScrollUpdateLatencyInfo(event_time, frame_time,
                                        5 /* scroll_delta */);

  // Another ScrollBegin to flush unfinished frames.
  event_time = MillisecondsToTimeTicks(1000);
  frame_time = MillisecondsToTimeTicks(1000);
  SyntheticTouchScrollBeginLatencyInfo(event_time, frame_time, 0);

  // The ScrollUpdates are at t=20ms, finger_pos=-20px, rendered_pos=-10px,
  // at t=25ms, finger_pos=-15px, rendered_pos=-10px;
  // To t=30ms both events get flush.
  // AverageLag is (0.5*(10px+5px)*5ms + 5px*5ms)/10ms = 6.25px
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.ScrollUpdate.Touch.AverageLag"),
              ElementsAre(Bucket(6, 1)));
}

// Test the case that switching direction causes lag at current frame
// time and previous frame time are in different direction.
TEST_F(AverageLagTrackerTest, ChangeDirectionInFrame) {
  // ScrollBegin
  base::TimeTicks event_time = MillisecondsToTimeTicks(10);
  base::TimeTicks frame_time = MillisecondsToTimeTicks(20);
  SyntheticTouchScrollBeginLatencyInfo(event_time, frame_time,
                                       10 /* scroll_delta */);

  // At t=20, lag = 10px.
  event_time = MillisecondsToTimeTicks(20);
  frame_time = MillisecondsToTimeTicks(30);
  SyntheticTouchScrollUpdateLatencyInfo(event_time, frame_time,
                                        10 /* scroll_delta */);

  // At t=30, lag = -10px.
  event_time = MillisecondsToTimeTicks(30);
  frame_time = MillisecondsToTimeTicks(40);
  SyntheticTouchScrollUpdateLatencyInfo(event_time, frame_time,
                                        -20 /* scroll_delta */);

  // Another ScrollBegin to flush unfinished frames.
  event_time = MillisecondsToTimeTicks(1000);
  frame_time = MillisecondsToTimeTicks(1000);
  SyntheticTouchScrollBeginLatencyInfo(event_time, frame_time, 0);

  // From t=20 to t=30, lag_area=2*(0.5*10px*5ms)=50px*ms.
  // From t=30 to t=40, lag_area=20px*10ms=200px*ms
  // AverageLag = (50+200)/20 = 12.5px.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.ScrollUpdate.Touch.AverageLag"),
              ElementsAre(Bucket(12, 1)));
}

}  // namespace
}  // namespace ui
