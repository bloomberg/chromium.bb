// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/fling_controller.h"

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/fling_booster.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseWheelEvent;

namespace content {

class FakeFlingController : public FlingController {
 public:
  FakeFlingController(GestureEventQueue* gesture_event_queue,

                      FlingControllerEventSenderClient* event_sender_client,
                      FlingControllerSchedulerClient* scheduler_client,
                      const Config& config)
      : FlingController(gesture_event_queue,
                        event_sender_client,
                        scheduler_client,
                        config) {}

  bool FlingBoosted() const { return fling_booster_->fling_boosted(); }
};

class FlingControllerTest : public GestureEventQueueClient,
                            public FlingControllerEventSenderClient,
                            public FlingControllerSchedulerClient,
                            public testing::TestWithParam<bool> {
 public:
  // testing::Test
  FlingControllerTest()
      : needs_begin_frame_for_fling_progress_(GetParam()),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  ~FlingControllerTest() override {}

  void SetUp() override {
    queue_ = std::make_unique<GestureEventQueue>(this, this, this,
                                                 GestureEventQueue::Config());
    fling_controller_ = std::make_unique<FakeFlingController>(
        queue_.get(), this, this, FlingController::Config());
  }

  // GestureEventQueueClient
  void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& event) override {}
  void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                         InputEventAckSource ack_source,
                         InputEventAckState ack_result) override {}

  // FlingControllerEventSenderClient
  void SendGeneratedWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override {
    last_sent_wheel_ = wheel_event.event;
    first_wheel_event_sent_ = true;

    if (wheel_event.event.momentum_phase == WebMouseWheelEvent::kPhaseEnded)
      first_wheel_event_sent_ = false;
  }
  void SendGeneratedGestureScrollEvents(
      const GestureEventWithLatencyInfo& gesture_event) override {
    sent_scroll_gesture_count_++;
    last_sent_gesture_ = gesture_event.event;
  }

  // FlingControllerSchedulerClient
  void ScheduleFlingProgress(
      base::WeakPtr<FlingController> fling_controller) override {
    DCHECK(!scheduled_next_fling_progress_);
    scheduled_next_fling_progress_ = true;
  }
  void DidStopFlingingOnBrowser(
      base::WeakPtr<FlingController> fling_controller) override {
    notified_client_after_fling_stop_ = true;
  }
  bool NeedsBeginFrameForFlingProgress() override {
    return needs_begin_frame_for_fling_progress_;
  }

  void SimulateFlingStart(blink::WebGestureDevice source_device,
                          const gfx::Vector2dF& velocity) {
    scheduled_next_fling_progress_ = false;
    sent_scroll_gesture_count_ = 0;
    WebGestureEvent fling_start(WebInputEvent::kGestureFlingStart, 0,
                                base::TimeTicks::Now(), source_device);
    fling_start.data.fling_start.velocity_x = velocity.x();
    fling_start.data.fling_start.velocity_y = velocity.y();
    GestureEventWithLatencyInfo fling_start_with_latency(fling_start);
    if (!fling_controller_->FilterGestureEvent(fling_start_with_latency))
      fling_controller_->ProcessGestureFlingStart(fling_start_with_latency);
  }

  void SimulateFlingCancel(blink::WebGestureDevice source_device) {
    notified_client_after_fling_stop_ = false;
    WebGestureEvent fling_cancel(WebInputEvent::kGestureFlingCancel, 0,
                                 base::TimeTicks::Now(), source_device);
    // autoscroll fling cancel doesn't allow fling boosting.
    if (source_device == blink::kWebGestureDeviceSyntheticAutoscroll)
      fling_cancel.data.fling_cancel.prevent_boosting = true;
    GestureEventWithLatencyInfo fling_cancel_with_latency(fling_cancel);
    last_fling_cancel_filtered_ =
        fling_controller_->FilterGestureEvent(fling_cancel_with_latency);
    if (!last_fling_cancel_filtered_)
      fling_controller_->ProcessGestureFlingCancel(fling_cancel_with_latency);
  }

  void ProgressFling(base::TimeTicks current_time) {
    DCHECK(scheduled_next_fling_progress_);
    scheduled_next_fling_progress_ = false;
    fling_controller_->ProgressFling(current_time);
  }

  bool FlingInProgress() { return fling_controller_->fling_in_progress(); }
  bool FlingBoosted() { return fling_controller_->FlingBoosted(); }

 protected:
  std::unique_ptr<FakeFlingController> fling_controller_;
  WebMouseWheelEvent last_sent_wheel_;
  WebGestureEvent last_sent_gesture_;
  bool last_fling_cancel_filtered_;
  bool scheduled_next_fling_progress_;
  bool notified_client_after_fling_stop_;
  bool first_wheel_event_sent_ = false;
  int sent_scroll_gesture_count_ = 0;

 private:
  bool needs_begin_frame_for_fling_progress_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<GestureEventQueue> queue_;
  DISALLOW_COPY_AND_ASSIGN(FlingControllerTest);
};

INSTANTIATE_TEST_CASE_P(, FlingControllerTest, testing::Bool());

TEST_P(FlingControllerTest,
       ControllerSendsWheelEndOnTouchpadFlingWithZeroVelocity) {
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF());
  // The controller doesn't start a fling and sends a wheel end event
  // immediately.
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, last_sent_wheel_.momentum_phase);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_x);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_y);
}

TEST_P(FlingControllerTest,
       ControllerSendsGSEOnTouchscreenFlingWithZeroVelocity) {
  SimulateFlingStart(blink::kWebGestureDeviceTouchscreen, gfx::Vector2dF());
  // The controller doesn't start a fling and sends a GSE immediately.
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd, last_sent_gesture_.GetType());
}

// TODO(https://crbug.com/836996): Timing-dependent flakes on some platforms.
TEST_P(FlingControllerTest, DISABLED_ControllerHandlesTouchpadGestureFling) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  // Processing GFS will send the first fling prgoress event if the time delta
  // between the timestamp of the GFS and the time that ProcessGestureFlingStart
  // is called is large enough.
  bool process_GFS_sent_first_event = first_wheel_event_sent_;

  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);

  if (!process_GFS_sent_first_event) {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  } else {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged,
              last_sent_wheel_.momentum_phase);
  }
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // The rest of the wheel events must have momentum_phase == KPhaseChanged.
  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged, last_sent_wheel_.momentum_phase);
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // Now cancel the fling. The GFC will get suppressed by fling booster.
  SimulateFlingCancel(blink::kWebGestureDeviceTouchpad);
  EXPECT_TRUE(last_fling_cancel_filtered_);
  EXPECT_TRUE(FlingInProgress());

  // Wait for the boosting timer to expire. The delayed cancelation must work.
  progress_time += base::TimeDelta::FromMilliseconds(500);
  ProgressFling(progress_time);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, last_sent_wheel_.momentum_phase);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_x);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_y);
}

// TODO(https://crbug.com/836996): Timing-dependent flakes on some platforms.
TEST_P(FlingControllerTest, DISABLED_ControllerHandlesTouchscreenGestureFling) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());

  // The fling progress will generate and send GSU events with inertial state.
  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  ASSERT_EQ(WebInputEvent::kGestureScrollUpdate, last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::kMomentumPhase,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);

  // Now cancel the fling. The GFC will get suppressed by fling booster.
  SimulateFlingCancel(blink::kWebGestureDeviceTouchscreen);
  EXPECT_TRUE(last_fling_cancel_filtered_);
  EXPECT_TRUE(FlingInProgress());

  // Wait for the boosting timer to expire. The delayed cancelation must work.
  progress_time += base::TimeDelta::FromMilliseconds(500);
  ProgressFling(progress_time);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd, last_sent_gesture_.GetType());
}

// TODO(https://crbug.com/836996): Timing-dependent flakes on some platforms.
TEST_P(FlingControllerTest,
       DISABLED_ControllerSendsWheelEndWhenTouchpadFlingIsOver) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF(100, 0));
  EXPECT_TRUE(FlingInProgress());
  // Processing GFS will send the first fling prgoress event if the time delta
  // between the timestamp of the GFS and the time that ProcessGestureFlingStart
  // is called is large enough.
  bool process_GFS_sent_first_event = first_wheel_event_sent_;

  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  if (!process_GFS_sent_first_event) {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  } else {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged,
              last_sent_wheel_.momentum_phase);
  }
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  while (FlingInProgress()) {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged,
              last_sent_wheel_.momentum_phase);
    EXPECT_GT(last_sent_wheel_.delta_x, 0.f);
    progress_time += base::TimeDelta::FromMilliseconds(17);
    ProgressFling(progress_time);
  }

  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, last_sent_wheel_.momentum_phase);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_x);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_y);
}

// TODO(https://crbug.com/836996): Timing-dependent flakes on some platforms.
TEST_P(FlingControllerTest,
       DISABLED_ControllerSendsGSEWhenTouchscreenFlingIsOver) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchscreen,
                     gfx::Vector2dF(100, 0));
  EXPECT_TRUE(FlingInProgress());

  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  while (FlingInProgress()) {
    ASSERT_EQ(WebInputEvent::kGestureScrollUpdate,
              last_sent_gesture_.GetType());
    EXPECT_EQ(WebGestureEvent::kMomentumPhase,
              last_sent_gesture_.data.scroll_update.inertial_phase);
    EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);
    progress_time += base::TimeDelta::FromMilliseconds(17);
    ProgressFling(progress_time);
  }

  EXPECT_EQ(WebInputEvent::kGestureScrollEnd, last_sent_gesture_.GetType());
}

// TODO(https://crbug.com/836996): Timing-dependent flakes on some platforms.
TEST_P(FlingControllerTest,
       DISABLED_EarlyTouchpadFlingCancelationOnInertialGSUAckNotConsumed) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  // Processing GFS will send the first fling prgoress event if the time delta
  // between the timestamp of the GFS and the time that ProcessGestureFlingStart
  // is called is large enough.
  bool process_GFS_sent_first_event = first_wheel_event_sent_;

  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  if (!process_GFS_sent_first_event) {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  } else {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged,
              last_sent_wheel_.momentum_phase);
  }
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // A non-consumed GSU ack in inertial state cancels out the rest of the fling.
  WebGestureEvent scroll_update(WebInputEvent::kGestureScrollUpdate, 0,
                                base::TimeTicks::Now());
  scroll_update.data.scroll_update.inertial_phase =
      WebGestureEvent::kMomentumPhase;

  fling_controller_->OnGestureEventAck(
      GestureEventWithLatencyInfo(scroll_update),
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, last_sent_wheel_.momentum_phase);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_x);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_y);
}

// TODO(https://crbug.com/836996): Timing-dependent flakes on some platforms.
TEST_P(FlingControllerTest,
       DISABLED_EarlyTouchscreenFlingCancelationOnInertialGSUAckNotConsumed) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  ASSERT_EQ(WebInputEvent::kGestureScrollUpdate, last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::kMomentumPhase,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);

  // A non-consumed GSU ack in inertial state cancels out the rest of the fling.
  WebGestureEvent scroll_update(WebInputEvent::kGestureScrollUpdate, 0,
                                base::TimeTicks::Now());
  scroll_update.data.scroll_update.inertial_phase =
      WebGestureEvent::kMomentumPhase;

  fling_controller_->OnGestureEventAck(
      GestureEventWithLatencyInfo(scroll_update),
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd, last_sent_gesture_.GetType());
}

// Flaky. https://crbug.com/836996.
TEST_P(FlingControllerTest, DISABLED_EarlyTouchpadFlingCancelationOnFlingStop) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  // Processing GFS will send the first fling prgoress event if the time delta
  // between the timestamp of the GFS and the time that ProcessGestureFlingStart
  // is called is large enough.
  bool process_GFS_sent_first_event = first_wheel_event_sent_;

  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  if (!process_GFS_sent_first_event) {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  } else {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged,
              last_sent_wheel_.momentum_phase);
  }
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  fling_controller_->StopFling();
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, last_sent_wheel_.momentum_phase);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_x);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_y);
}

TEST_P(FlingControllerTest, EarlyTouchscreenFlingCancelationOnFlingStop) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());

  // progress fling must send GSU events.
  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  ASSERT_EQ(WebInputEvent::kGestureScrollUpdate, last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::kMomentumPhase,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);

  fling_controller_->StopFling();
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd, last_sent_gesture_.GetType());
}

TEST_P(FlingControllerTest, GestureFlingCancelsFiltered) {
  // GFC without previous GFS is dropped.
  SimulateFlingCancel(blink::kWebGestureDeviceTouchscreen);
  EXPECT_TRUE(last_fling_cancel_filtered_);

  // GFC after previous GFS is filtered by fling booster.
  SimulateFlingStart(blink::kWebGestureDeviceTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  SimulateFlingCancel(blink::kWebGestureDeviceTouchscreen);
  EXPECT_TRUE(last_fling_cancel_filtered_);
  EXPECT_TRUE(FlingInProgress());

  // Any other GFC while the fling cancelation is deferred gets filtered.
  SimulateFlingCancel(blink::kWebGestureDeviceTouchscreen);
  EXPECT_TRUE(last_fling_cancel_filtered_);
}

// Flaky. https://crbug.com/836996.
TEST_P(FlingControllerTest, DISABLED_GestureFlingNotCancelledBySmallTimeDelta) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());

  // If we the first progress tick happens too close to the fling_start time,
  // the controller won't send any GSU events, but the fling is still active.
  // progress_time += base::TimeDelta::FromMilliseconds(1);
  ProgressFling(progress_time);
  EXPECT_EQ(blink::kWebGestureDeviceUninitialized,
            last_sent_gesture_.SourceDevice());
  EXPECT_TRUE(FlingInProgress());

  // The rest of the progress flings must advance the fling normally.
  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen,
            last_sent_gesture_.SourceDevice());
  ASSERT_EQ(WebInputEvent::kGestureScrollUpdate, last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::kMomentumPhase,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);
}

// TODO(https://crbug.com/836996): Timing-dependent flakes on some platforms.
TEST_P(FlingControllerTest, DISABLED_GestureFlingWithNegativeTimeDelta) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  int current_sent_scroll_gesture_count = sent_scroll_gesture_count_;

  // If we get a negative time delta, that is, the Progress tick time happens
  // before the fling's start time then we should *not* try progressing the
  // fling.
  progress_time -= base::TimeDelta::FromMilliseconds(5);
  ProgressFling(progress_time);
  EXPECT_EQ(current_sent_scroll_gesture_count, sent_scroll_gesture_count_);

  // The rest of the progress flings must advance the fling normally.
  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen,
            last_sent_gesture_.SourceDevice());
  ASSERT_EQ(WebInputEvent::kGestureScrollUpdate, last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::kMomentumPhase,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);
}

#if defined(OS_LINUX)
#define MAYBE_ControllerBoostsTouchpadFling \
  DISABLED_ControllerBoostsTouchpadFling
#else
#define MAYBE_ControllerBoostsTouchpadFling ControllerBoostsTouchpadFling
#endif
TEST_P(FlingControllerTest, MAYBE_ControllerBoostsTouchpadFling) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  // Processing GFS will send the first fling prgoress event if the time delta
  // between the timestamp of the GFS and the time that ProcessGestureFlingStart
  // is called is large enough.
  bool process_GFS_sent_first_event = first_wheel_event_sent_;

  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  if (!process_GFS_sent_first_event) {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  } else {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged,
              last_sent_wheel_.momentum_phase);
  }
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // The rest of the wheel events must have momentum_phase == KPhaseChanged.
  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged, last_sent_wheel_.momentum_phase);
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // Now cancel the fling. The GFC will get suppressed by fling booster.
  SimulateFlingCancel(blink::kWebGestureDeviceTouchpad);
  EXPECT_TRUE(last_fling_cancel_filtered_);
  EXPECT_TRUE(FlingInProgress());

  // The second GFS will boost the current active fling.
  SimulateFlingStart(blink::kWebGestureDeviceTouchpad, gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  EXPECT_TRUE(FlingBoosted());
}

TEST_P(FlingControllerTest, ControllerBoostsTouchscreenFling) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());

  // Fling progress must send GSU events.
  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  ASSERT_EQ(WebInputEvent::kGestureScrollUpdate, last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::kMomentumPhase,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);

  // Now cancel the fling. The GFC will get suppressed by fling booster.
  SimulateFlingCancel(blink::kWebGestureDeviceTouchscreen);
  EXPECT_TRUE(last_fling_cancel_filtered_);
  EXPECT_TRUE(FlingInProgress());

  // The second GFS will boost the current active fling.
  SimulateFlingStart(blink::kWebGestureDeviceTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  EXPECT_TRUE(FlingBoosted());
}

TEST_P(FlingControllerTest, ControllerNotifiesTheClientAfterFlingStart) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());

  // Now cancel the fling. The GFC will get suppressed by fling booster.
  SimulateFlingCancel(blink::kWebGestureDeviceTouchscreen);
  EXPECT_TRUE(last_fling_cancel_filtered_);
  EXPECT_TRUE(FlingInProgress());

  // Wait for the boosting timer to expire. The delayed cancelation must work
  // and the client must be notified after fling cancelation.
  progress_time += base::TimeDelta::FromMilliseconds(500);
  ProgressFling(progress_time);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd, last_sent_gesture_.GetType());
  EXPECT_TRUE(notified_client_after_fling_stop_);
}

TEST_P(FlingControllerTest, MiddleClickAutoScrollFling) {
  base::TimeTicks progress_time = base::TimeTicks::Now();
  SimulateFlingStart(blink::kWebGestureDeviceSyntheticAutoscroll,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());

  progress_time += base::TimeDelta::FromMilliseconds(17);
  ProgressFling(progress_time);
  ASSERT_EQ(WebInputEvent::kGestureScrollUpdate, last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::kMomentumPhase,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);

  // Now send a new fling with different velocity and without sending a fling
  // cancel event, the new fling should always replace the old one even when
  // they are in the same direction.
  SimulateFlingStart(blink::kWebGestureDeviceSyntheticAutoscroll,
                     gfx::Vector2dF(2000, 0));
  EXPECT_TRUE(FlingInProgress());
  EXPECT_FALSE(FlingBoosted());

  // Now cancel the fling. The GFC won't get suppressed by fling booster since
  // autoscroll fling doesn't have boosting.
  SimulateFlingCancel(blink::kWebGestureDeviceSyntheticAutoscroll);
  EXPECT_FALSE(last_fling_cancel_filtered_);
  EXPECT_FALSE(FlingInProgress());
}

}  // namespace content
