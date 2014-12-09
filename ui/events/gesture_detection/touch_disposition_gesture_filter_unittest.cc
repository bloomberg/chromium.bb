// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/gesture_detection/touch_disposition_gesture_filter.h"
#include "ui/events/test/motion_event_test_utils.h"

using ui::test::MockMotionEvent;

namespace ui {
namespace {

const int kDefaultEventFlags = EF_ALT_DOWN | EF_SHIFT_DOWN;

}  // namespace

class TouchDispositionGestureFilterTest
    : public testing::Test,
      public TouchDispositionGestureFilterClient {
 public:
  TouchDispositionGestureFilterTest()
      : cancel_after_next_gesture_(false), sent_gesture_count_(0) {}
  ~TouchDispositionGestureFilterTest() override {}

  // testing::Test
  void SetUp() override {
    queue_.reset(new TouchDispositionGestureFilter(this));
    touch_event_.set_flags(kDefaultEventFlags);
  }

  void TearDown() override { queue_.reset(); }

  // TouchDispositionGestureFilterClient
  void ForwardGestureEvent(const GestureEventData& event) override {
    ++sent_gesture_count_;
    last_sent_gesture_.reset(new GestureEventData(event));
    sent_gestures_.push_back(event.type());
    if (event.type() == ET_GESTURE_SHOW_PRESS)
      show_press_bounding_box_ = event.details.bounding_box();
    if (cancel_after_next_gesture_) {
      cancel_after_next_gesture_ = false;
      SendPacket(CancelTouchPoint(), NoGestures());
      SendTouchNotConsumedAck();
    }
  }

 protected:
  typedef std::vector<EventType> GestureList;

  ::testing::AssertionResult GesturesMatch(const GestureList& expected,
                                           const GestureList& actual) {
    if (expected.size() != actual.size()) {
      return ::testing::AssertionFailure()
          << "actual.size(" << actual.size()
          << ") != expected.size(" << expected.size() << ")";
    }

    for (size_t i = 0; i < expected.size(); ++i) {
      if (expected[i] != actual[i]) {
        return ::testing::AssertionFailure()
            << "actual[" << i << "] ("
            << actual[i]
            << ") != expected[" << i << "] ("
            << expected[i] << ")";
      }
    }

    return ::testing::AssertionSuccess();
  }

  GestureList NoGestures() { return GestureList(0); }

  GestureList Gestures(EventType type) {
    return GestureList(1, type);
  }

  GestureList Gestures(EventType type0, EventType type1) {
    GestureList gestures(2);
    gestures[0] = type0;
    gestures[1] = type1;
    return gestures;
  }

  GestureList Gestures(EventType type0,
                       EventType type1,
                       EventType type2) {
    GestureList gestures(3);
    gestures[0] = type0;
    gestures[1] = type1;
    gestures[2] = type2;
    return gestures;
  }

  GestureList Gestures(EventType type0,
                       EventType type1,
                       EventType type2,
                       EventType type3) {
    GestureList gestures(4);
    gestures[0] = type0;
    gestures[1] = type1;
    gestures[2] = type2;
    gestures[3] = type3;
    return gestures;
  }

  TouchDispositionGestureFilter::PacketResult
  SendTouchGestures(const MotionEvent& touch,
                    const GestureEventDataPacket& packet) {
    GestureEventDataPacket touch_packet =
        GestureEventDataPacket::FromTouch(touch);
    for (size_t i = 0; i < packet.gesture_count(); ++i)
      touch_packet.Push(packet.gesture(i));
    return queue_->OnGesturePacket(touch_packet);
  }

  TouchDispositionGestureFilter::PacketResult
  SendTimeoutGesture(EventType type) {
    return queue_->OnGesturePacket(
        GestureEventDataPacket::FromTouchTimeout(CreateGesture(type)));
  }

  TouchDispositionGestureFilter::PacketResult
  SendGesturePacket(const GestureEventDataPacket& packet) {
    return queue_->OnGesturePacket(packet);
  }

  void SendTouchEventAck(bool event_consumed) {
    queue_->OnTouchEventAckForQueueFront(event_consumed);
  }

  void SendTouchConsumedAck() { SendTouchEventAck(true); }

  void SendTouchNotConsumedAck() { SendTouchEventAck(false); }

  void SendTouchEventAckForQueueBack(bool event_consumed) {
    queue_->OnTouchEventAckForQueueBack(event_consumed);
  }

  void SendTouchConsumedAckForQueueBack() {
    SendTouchEventAckForQueueBack(true);
  }

  void SendTouchNotConsumedAckForQueueBack() {
    SendTouchEventAckForQueueBack(false);
  }

  void PushGesture(EventType type) {
    pending_gesture_packet_.Push(CreateGesture(type));
  }

  void PushGesture(EventType type, float x, float y, float diameter) {
    pending_gesture_packet_.Push(CreateGesture(type, x, y, diameter));
  }

  const MockMotionEvent& PressTouchPoint(float x, float y) {
    touch_event_.PressPoint(x, y);
    touch_event_.SetRawOffset(raw_offset_.x(), raw_offset_.y());
    return touch_event_;
  }

  const MockMotionEvent& PressTouchPoint() { return PressTouchPoint(0, 0); }

  const MockMotionEvent& MoveTouchPoint() {
    touch_event_.MovePoint(0, 0, 0);
    touch_event_.set_event_time(base::TimeTicks::Now());
    return touch_event_;
  }

  const MockMotionEvent& ReleaseTouchPoint() {
    touch_event_.ReleasePoint();
    touch_event_.set_event_time(base::TimeTicks::Now());
    return touch_event_;
  }

  const MockMotionEvent& CancelTouchPoint() {
    touch_event_.CancelPoint();
    touch_event_.set_event_time(base::TimeTicks::Now());
    return touch_event_;
  }

  void SendPacket(const MockMotionEvent& touch_event,
                  GestureList gesture_list) {
    GestureEventDataPacket gesture_packet;
    for (EventType type : gesture_list)
      gesture_packet.Push(CreateGesture(type));

    EXPECT_EQ(TouchDispositionGestureFilter::SUCCESS,
              SendTouchGestures(touch_event, gesture_packet));
  }

  void SetRawTouchOffset(const gfx::Vector2dF& raw_offset) {
    raw_offset_ = raw_offset;
  }

  const MockMotionEvent& ResetTouchPoints() {
    touch_event_ = MockMotionEvent();
    return touch_event_;
  }

  bool GesturesSent() const { return !sent_gestures_.empty(); }

  const GestureEventData& last_sent_gesture() const {
    CHECK(last_sent_gesture_);
    return *last_sent_gesture_;
  }

  base::TimeTicks LastSentGestureTime() const {
    return last_sent_gesture().time;
  }

  base::TimeTicks CurrentTouchTime() const {
    return touch_event_.GetEventTime();
  }

  bool IsEmpty() const { return queue_->IsEmpty(); }

  GestureList GetAndResetSentGestures() {
    GestureList sent_gestures;
    sent_gestures.swap(sent_gestures_);
    return sent_gestures;
  }

  gfx::PointF LastSentGestureLocation() const {
    return gfx::PointF(last_sent_gesture().x, last_sent_gesture().y);
  }

  gfx::PointF LastSentGestureRawLocation() const {
    return gfx::PointF(last_sent_gesture().raw_x, last_sent_gesture().raw_y);
  }

  int LastSentGestureFlags() const {
    return last_sent_gesture().flags;
  }

  const gfx::RectF& ShowPressBoundingBox() const {
    return show_press_bounding_box_;
  }

  void SetCancelAfterNextGesture(bool cancel_after_next_gesture) {
    cancel_after_next_gesture_ = cancel_after_next_gesture;
  }

  GestureEventData CreateGesture(EventType type) {
    return CreateGesture(type, 0, 0, 0);
  }

  GestureEventData CreateGesture(EventType type,
                                 float x,
                                 float y,
                                 float diameter) {
    return GestureEventData(
        GestureEventDetails(type), 0, MotionEvent::TOOL_TYPE_FINGER,
        base::TimeTicks(), x, y, 0, 0, 1,
        gfx::RectF(x - diameter / 2, y - diameter / 2, diameter, diameter),
        kDefaultEventFlags);
  }

 private:
  scoped_ptr<TouchDispositionGestureFilter> queue_;
  bool cancel_after_next_gesture_;
  MockMotionEvent touch_event_;
  GestureEventDataPacket pending_gesture_packet_;
  size_t sent_gesture_count_;
  GestureList sent_gestures_;
  gfx::Vector2dF raw_offset_;
  scoped_ptr<GestureEventData> last_sent_gesture_;
  gfx::RectF show_press_bounding_box_;
};

TEST_F(TouchDispositionGestureFilterTest, BasicNoGestures) {
  PressTouchPoint();
  EXPECT_FALSE(GesturesSent());

  MoveTouchPoint();
  EXPECT_FALSE(GesturesSent());

  // No gestures should be dispatched by the ack, as the queued packets
  // contained no gestures.
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // Release the touch gesture.
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, BasicGestures) {
  GestureList press_gestures =
      Gestures(ET_GESTURE_BEGIN, ET_GESTURE_SCROLL_BEGIN);
  // An unconsumed touch's gesture should be sent.
  SendPacket(PressTouchPoint(), press_gestures);
  EXPECT_FALSE(GesturesSent());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(press_gestures, GetAndResetSentGestures()));

  // Multiple gestures can be queued for a single event.
  GestureList release_gestures =
      Gestures(ET_SCROLL_FLING_START, ET_SCROLL_FLING_CANCEL, ET_GESTURE_END);
  SendPacket(ReleaseTouchPoint(), release_gestures);
  EXPECT_FALSE(GesturesSent());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(release_gestures, GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, BasicGesturesConsumed) {
  // A consumed touch's gesture should not be sent.
  SendPacket(PressTouchPoint(),
             Gestures(ET_GESTURE_BEGIN, ET_GESTURE_SCROLL_BEGIN));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(
      ReleaseTouchPoint(),
      Gestures(ET_SCROLL_FLING_START, ET_SCROLL_FLING_CANCEL, ET_GESTURE_END));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, ConsumedThenNotConsumed) {
  // A consumed touch's gesture should not be sent.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // Even if the subsequent touch is not consumed, continue dropping gestures.
  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // Even if the subsequent touch had no consumer, continue dropping gestures.
  SendPacket(ReleaseTouchPoint(), Gestures(ET_SCROLL_FLING_START));
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, NotConsumedThenConsumed) {
  // A not consumed touch's gesture should be sent.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  // A newly consumed gesture should not be sent.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_PINCH_BEGIN));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // And subsequent non-consumed pinch updates should not be sent.
  SendPacket(MoveTouchPoint(),
             Gestures(ET_GESTURE_SCROLL_UPDATE, ET_GESTURE_PINCH_UPDATE));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_UPDATE),
                            GetAndResetSentGestures()));

  // End events dispatched only when their start events were.
  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_PINCH_END));
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_SCROLL_END));
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, ScrollAlternatelyConsumed) {
  // A consumed touch's gesture should not be sent.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  for (size_t i = 0; i < 3; ++i) {
    SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
    SendTouchConsumedAck();
    EXPECT_FALSE(GesturesSent());

    SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
    SendTouchNotConsumedAck();
    EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_UPDATE),
                              GetAndResetSentGestures()));
  }

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_SCROLL_END));
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, NotConsumedThenNoConsumer) {
  // An unconsumed touch's gesture should be sent.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  // If the subsequent touch has no consumer (e.g., a secondary pointer is
  // pressed but not on a touch handling rect), send the gesture.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_PINCH_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_BEGIN),
                            GetAndResetSentGestures()));

  // End events should be dispatched when their start events were, independent
  // of the ack state.
  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_PINCH_END));
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_END),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_SCROLL_END));
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, EndingEventsSent) {
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_PINCH_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_BEGIN),
                            GetAndResetSentGestures()));

  // Consuming the touchend event can't suppress the match end gesture.
  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_PINCH_END));
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_END),
                            GetAndResetSentGestures()));

  // But other events in the same packet are still suppressed.
  SendPacket(ReleaseTouchPoint(),
             Gestures(ET_GESTURE_SCROLL_UPDATE, ET_GESTURE_SCROLL_END));
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));

  // ET_GESTURE_SCROLL_END and ET_SCROLL_FLING_START behave the same in this
  // regard.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(ET_SCROLL_FLING_START));
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_SCROLL_FLING_START),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, EndingEventsNotSent) {
  // Consuming a begin event ensures no end events are sent.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_PINCH_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_PINCH_END));
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_SCROLL_END));
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, UpdateEventsSuppressedPerEvent) {
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  // Consuming a single scroll or pinch update should suppress only that event.
  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_PINCH_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_BEGIN),
                            GetAndResetSentGestures()));

  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_PINCH_UPDATE));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // Subsequent updates should not be affected.
  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_UPDATE),
                            GetAndResetSentGestures()));

  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_PINCH_UPDATE));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_UPDATE),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_PINCH_END));
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_END),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_SCROLL_END));
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, UpdateEventsDependOnBeginEvents) {
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // Scroll and pinch gestures depend on the scroll begin gesture being
  // dispatched.
  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_PINCH_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_PINCH_UPDATE));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_PINCH_END));
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_SCROLL_END));
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, MultipleTouchSequences) {
  // Queue two touch-to-gestures sequences.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_TAP_DOWN));
  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_TAP));
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_SCROLL_END));

  // The first gesture sequence should not be allowed.
  SendTouchConsumedAck();
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // The subsequent sequence should "reset" allowance.
  SendTouchNotConsumedAck();
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN,
                                     ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, FlingCancelledOnNewTouchSequence) {
  const gfx::Vector2dF raw_offset(1.3f, 3.7f);
  SetRawTouchOffset(raw_offset);

  // Simulate a fling.
  SendPacket(PressTouchPoint(),
             Gestures(ET_GESTURE_TAP_DOWN, ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(
      Gestures(
          ET_GESTURE_TAP_DOWN, ET_GESTURE_TAP_CANCEL, ET_GESTURE_SCROLL_BEGIN),
      GetAndResetSentGestures()));
  SendPacket(ReleaseTouchPoint(), Gestures(ET_SCROLL_FLING_START));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_SCROLL_FLING_START),
                            GetAndResetSentGestures()));

  // A new touch sequence should cancel the outstanding fling.
  SendPacket(PressTouchPoint(1, 1), NoGestures());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_SCROLL_FLING_CANCEL),
                            GetAndResetSentGestures()));
  EXPECT_EQ(CurrentTouchTime(), LastSentGestureTime());
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(1, 1));
  EXPECT_EQ(LastSentGestureRawLocation(), gfx::PointF(1, 1) + raw_offset);
  SendPacket(ReleaseTouchPoint(), NoGestures());
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, ScrollEndedOnTouchReleaseIfNoFling) {
  // Simulate a scroll.
  // Touch position will be used for synthesized scroll end gesture.
  SendPacket(PressTouchPoint(2, 3),
             Gestures(ET_GESTURE_TAP_DOWN, ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(
      Gestures(
          ET_GESTURE_TAP_DOWN, ET_GESTURE_TAP_CANCEL, ET_GESTURE_SCROLL_BEGIN),
      GetAndResetSentGestures()));
  SendPacket(ReleaseTouchPoint(), NoGestures());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
  EXPECT_EQ(CurrentTouchTime(), LastSentGestureTime());
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(2, 3));
}

TEST_F(TouchDispositionGestureFilterTest, ScrollEndedOnNewTouchSequence) {
  // Simulate a scroll.
  SendPacket(PressTouchPoint(),
             Gestures(ET_GESTURE_TAP_DOWN, ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(
      Gestures(
          ET_GESTURE_TAP_DOWN, ET_GESTURE_TAP_CANCEL, ET_GESTURE_SCROLL_BEGIN),
      GetAndResetSentGestures()));

  // A new touch sequence should end the outstanding scroll.
  ResetTouchPoints();

  // Touch position will be used for synthesized scroll end gesture.
  SendPacket(PressTouchPoint(2, 3), NoGestures());
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
  EXPECT_EQ(CurrentTouchTime(), LastSentGestureTime());
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(2, 3));
}

TEST_F(TouchDispositionGestureFilterTest, FlingCancelledOnScrollBegin) {
  // Simulate a fling sequence.
  SendPacket(PressTouchPoint(),
             Gestures(ET_GESTURE_TAP_DOWN, ET_GESTURE_SCROLL_BEGIN,
                      ET_SCROLL_FLING_START));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN,
                                     ET_GESTURE_TAP_CANCEL,
                                     ET_GESTURE_SCROLL_BEGIN,
                                     ET_SCROLL_FLING_START),
                            GetAndResetSentGestures()));

  // The new fling should cancel the preceding one.
  SendPacket(ReleaseTouchPoint(),
             Gestures(ET_GESTURE_SCROLL_BEGIN, ET_SCROLL_FLING_START));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_SCROLL_FLING_CANCEL,
                                     ET_GESTURE_SCROLL_BEGIN,
                                     ET_SCROLL_FLING_START),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, FlingNotCancelledIfGFCEventReceived) {
  // Simulate a fling that is started then cancelled.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  SendPacket(MoveTouchPoint(), Gestures(ET_SCROLL_FLING_START));
  SendTouchNotConsumedAck();
  GestureEventDataPacket packet;
  packet.Push(CreateGesture(ET_SCROLL_FLING_CANCEL, 2, 3, 0));
  SendTouchGestures(ReleaseTouchPoint(), packet);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN,
                                     ET_SCROLL_FLING_START,
                                     ET_SCROLL_FLING_CANCEL),
                            GetAndResetSentGestures()));
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(2, 3));

  // A new touch sequence will not inject a ET_SCROLL_FLING_CANCEL, as the fling
  // has already been cancelled.
  SendPacket(PressTouchPoint(), NoGestures());
  SendTouchNotConsumedAck();
  SendPacket(ReleaseTouchPoint(), NoGestures());
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledWhenScrollBegins) {
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_TAP_DOWN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  // If the subsequent touch turns into a scroll, the tap should be cancelled.
  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_CANCEL,
                                     ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledWhenTouchConsumed) {
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_TAP_DOWN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  // If the subsequent touch is consumed, the tap should be cancelled.
  GestureEventDataPacket packet;
  packet.Push(CreateGesture(ET_GESTURE_SCROLL_BEGIN, 2, 3, 0));
  SendTouchGestures(MoveTouchPoint(), packet);

  SendTouchConsumedAck();
  EXPECT_TRUE(
      GesturesMatch(Gestures(ET_GESTURE_TAP_CANCEL, ET_GESTURE_SCROLL_BEGIN),
                    GetAndResetSentGestures()));
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(2, 3));
}

TEST_F(TouchDispositionGestureFilterTest,
       TapNotCancelledIfTapEndingEventReceived) {
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_TAP_DOWN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(
      GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN), GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_TAP));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SHOW_PRESS, ET_GESTURE_TAP),
                            GetAndResetSentGestures()));

  // The tap should not be cancelled as it was terminated by a |ET_GESTURE_TAP|.
  SendPacket(PressTouchPoint(), NoGestures());
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, TimeoutGestures) {
  // If the sequence is allowed, and there are no preceding gestures, the
  // timeout gestures should be forwarded immediately.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_TAP_DOWN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(ET_GESTURE_SHOW_PRESS);
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SHOW_PRESS),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(ET_GESTURE_LONG_PRESS);
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_LONG_PRESS),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_LONG_TAP));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_CANCEL,
                                     ET_GESTURE_LONG_TAP),
                            GetAndResetSentGestures()));

  // If the sequence is disallowed, and there are no preceding gestures, the
  // timeout gestures should be dropped immediately.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_TAP_DOWN));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(ET_GESTURE_SHOW_PRESS);
  EXPECT_FALSE(GesturesSent());
  SendPacket(ReleaseTouchPoint(), NoGestures());
  SendTouchNotConsumedAck();

  // If the sequence has a pending ack, the timeout gestures should
  // remain queued until the ack is received.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_TAP_DOWN));
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(ET_GESTURE_LONG_PRESS);
  EXPECT_FALSE(GesturesSent());

  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN,
                                     ET_GESTURE_LONG_PRESS),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, SpuriousAcksIgnored) {
  // Acks received when the queue is empty will be safely ignored.
  ASSERT_TRUE(IsEmpty());
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchNotConsumedAck();
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN,
                                     ET_GESTURE_SCROLL_UPDATE),
                            GetAndResetSentGestures()));

  // Even if all packets have been dispatched, the filter may not be empty as
  // there could be follow-up timeout events.  Spurious acks in such cases
  // should also be safely ignored.
  ASSERT_FALSE(IsEmpty());
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, PacketWithInvalidTypeIgnored) {
  GestureEventDataPacket packet;
  EXPECT_EQ(TouchDispositionGestureFilter::INVALID_PACKET_TYPE,
            SendGesturePacket(packet));
  EXPECT_TRUE(IsEmpty());
}

TEST_F(TouchDispositionGestureFilterTest, PacketsWithInvalidOrderIgnored) {
  EXPECT_EQ(TouchDispositionGestureFilter::INVALID_PACKET_ORDER,
            SendTimeoutGesture(ET_GESTURE_SHOW_PRESS));
  EXPECT_TRUE(IsEmpty());
}

TEST_F(TouchDispositionGestureFilterTest, ConsumedTouchCancel) {
  // An unconsumed touch's gesture should be sent.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_TAP_DOWN));
  EXPECT_FALSE(GesturesSent());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  SendPacket(CancelTouchPoint(),
             Gestures(ET_GESTURE_TAP_CANCEL, ET_GESTURE_SCROLL_END));
  EXPECT_FALSE(GesturesSent());
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_CANCEL,
                                     ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TimeoutEventAfterRelease) {
  SendPacket(PressTouchPoint(), NoGestures());
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());
  SendPacket(ReleaseTouchPoint(),
             Gestures(ET_GESTURE_TAP_DOWN, ET_GESTURE_TAP_UNCONFIRMED));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(
      GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN, ET_GESTURE_TAP_UNCONFIRMED),
                    GetAndResetSentGestures()));

  SendTimeoutGesture(ET_GESTURE_TAP);
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SHOW_PRESS, ET_GESTURE_TAP),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, ShowPressInsertedBeforeTap) {
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_TAP_DOWN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(ET_GESTURE_TAP_UNCONFIRMED);
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_UNCONFIRMED),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_TAP));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SHOW_PRESS,
                                     ET_GESTURE_TAP),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, ShowPressNotInsertedIfAlreadySent) {
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_TAP_DOWN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(ET_GESTURE_SHOW_PRESS);
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SHOW_PRESS),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_TAP));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TapAndScrollCancelledOnTouchCancel) {
  const gfx::Vector2dF raw_offset(1.3f, 3.7f);
  SetRawTouchOffset(raw_offset);

  SendPacket(PressTouchPoint(1, 1), Gestures(ET_GESTURE_TAP_DOWN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  // A cancellation motion event should cancel the tap.
  SendPacket(CancelTouchPoint(), NoGestures());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_CANCEL),
                            GetAndResetSentGestures()));
  EXPECT_EQ(CurrentTouchTime(), LastSentGestureTime());
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(1, 1));
  EXPECT_EQ(LastSentGestureRawLocation(), gfx::PointF(1, 1) + raw_offset);

  SendPacket(PressTouchPoint(1, 1), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  // A cancellation motion event should end the scroll, even if the touch was
  // consumed.
  SendPacket(CancelTouchPoint(), NoGestures());
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
  EXPECT_EQ(CurrentTouchTime(), LastSentGestureTime());
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(1, 1));
  EXPECT_EQ(LastSentGestureRawLocation(), gfx::PointF(1, 1) + raw_offset);
}

TEST_F(TouchDispositionGestureFilterTest,
       ConsumedScrollUpdateMakesFlingScrollEnd) {
  // A consumed touch's gesture should not be sent.
  SendPacket(PressTouchPoint(),
             Gestures(ET_GESTURE_BEGIN, ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();

  EXPECT_TRUE(
      GesturesMatch(Gestures(ET_GESTURE_BEGIN, ET_GESTURE_SCROLL_BEGIN),
                    GetAndResetSentGestures()));

  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  GestureEventDataPacket packet;
  packet.Push(CreateGesture(ET_SCROLL_FLING_START));
  packet.Push(CreateGesture(ET_SCROLL_FLING_CANCEL));
  packet.Push(CreateGesture(ET_GESTURE_END, 2, 3, 0));
  SendTouchGestures(ReleaseTouchPoint(), packet);

  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END, ET_GESTURE_END),
                            GetAndResetSentGestures()));
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(2, 3));

  SendPacket(PressTouchPoint(),
             Gestures(ET_GESTURE_BEGIN, ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_BEGIN, ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledOnTouchCancel) {
  // Touch position is used for synthesized tap cancel.
  SendPacket(PressTouchPoint(2, 3), Gestures(ET_GESTURE_TAP_DOWN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  // A cancellation motion event should cancel the tap.
  SendPacket(CancelTouchPoint(), NoGestures());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_CANCEL),
                            GetAndResetSentGestures()));
  EXPECT_EQ(CurrentTouchTime(), LastSentGestureTime());
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(2, 3));
}

// Test that a GestureEvent whose dispatch causes a cancel event to be fired
// won't cause a crash.
TEST_F(TouchDispositionGestureFilterTest, TestCancelMidGesture) {
  SetCancelAfterNextGesture(true);
  // Synthesized tap cancel uses touch position.
  SendPacket(PressTouchPoint(1, 1), Gestures(ET_GESTURE_TAP_DOWN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN,
                                     ET_GESTURE_TAP_CANCEL),
                            GetAndResetSentGestures()));
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(1, 1));
}

// Test that a MultiFingerSwipe event is dispatched when appropriate.
TEST_F(TouchDispositionGestureFilterTest, TestAllowedMultiFingerSwipe) {
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_PINCH_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_BEGIN),
                            GetAndResetSentGestures()));

  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SWIPE));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SWIPE),
                            GetAndResetSentGestures()));
}

  // Test that a MultiFingerSwipe event is dispatched when appropriate.
TEST_F(TouchDispositionGestureFilterTest, TestDisallowedMultiFingerSwipe) {
  SendPacket(PressTouchPoint(), NoGestures());
  SendTouchNotConsumedAck();

  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_PINCH_BEGIN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_BEGIN),
                            GetAndResetSentGestures()));

  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_SWIPE));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelOnSecondFingerDown) {
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_TAP_DOWN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  SendPacket(PressTouchPoint(), NoGestures());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_CANCEL),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, ShowPressBoundingBox) {
  GestureEventDataPacket press_packet;
  press_packet.Push(CreateGesture(ET_GESTURE_TAP_DOWN, 9, 9, 8));
  SendTouchGestures(PressTouchPoint(), press_packet);

  SendTouchNotConsumedAck();
  EXPECT_TRUE(
      GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN), GetAndResetSentGestures()));

  GestureEventDataPacket release_packet;
  release_packet.Push(CreateGesture(ET_GESTURE_TAP, 10, 10, 10));
  SendTouchGestures(ReleaseTouchPoint(), release_packet);

  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SHOW_PRESS, ET_GESTURE_TAP),
                            GetAndResetSentGestures()));
  EXPECT_EQ(gfx::RectF(5, 5, 10, 10), ShowPressBoundingBox());
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledBeforeGestureEnd) {
  SendPacket(PressTouchPoint(),
             Gestures(ET_GESTURE_BEGIN, ET_GESTURE_TAP_DOWN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_BEGIN, ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));
  SendTimeoutGesture(ET_GESTURE_SHOW_PRESS);
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SHOW_PRESS),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(ET_GESTURE_LONG_PRESS);
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_LONG_PRESS),
                            GetAndResetSentGestures()));
  SendPacket(CancelTouchPoint(), Gestures(ET_GESTURE_END));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_CANCEL, ET_GESTURE_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, EventFlagPropagation) {
  // Real gestures should propagate flags from their causal touches.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_TAP_DOWN));
  SendTouchNotConsumedAck();
  EXPECT_TRUE(
      GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN), GetAndResetSentGestures()));
  EXPECT_EQ(kDefaultEventFlags, LastSentGestureFlags());

  // Synthetic gestures lack flags.
  SendPacket(PressTouchPoint(), NoGestures());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_CANCEL),
                            GetAndResetSentGestures()));
  EXPECT_EQ(0, LastSentGestureFlags());
}


TEST_F(TouchDispositionGestureFilterTest, PreviousScrollPrevented) {
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_BEGIN));
  EXPECT_FALSE(GesturesSent());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(
      GesturesMatch(Gestures(ET_GESTURE_BEGIN), GetAndResetSentGestures()));

  // The sent scroll update should always reflect whether any preceding scroll
  // update has been dropped.
  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchNotConsumedAck();
  ASSERT_TRUE(GesturesSent());
  EXPECT_FALSE(last_sent_gesture()
                   .details.previous_scroll_update_in_sequence_prevented());
  GetAndResetSentGestures();

  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchNotConsumedAck();
  ASSERT_TRUE(GesturesSent());
  EXPECT_TRUE(last_sent_gesture()
                  .details.previous_scroll_update_in_sequence_prevented());
  GetAndResetSentGestures();

  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchNotConsumedAck();
  ASSERT_TRUE(GesturesSent());
  EXPECT_TRUE(last_sent_gesture()
                  .details.previous_scroll_update_in_sequence_prevented());
}

TEST_F(TouchDispositionGestureFilterTest, AckQueueBack) {
  SendPacket(PressTouchPoint(1, 1), Gestures(ET_GESTURE_BEGIN));
  SendTouchNotConsumedAck();

  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_BEGIN));
  SendTouchNotConsumedAck();
  GetAndResetSentGestures();

  // Pending touch move.
  GestureEventDataPacket packet1;
  packet1.Push(CreateGesture(ET_GESTURE_SCROLL_UPDATE, 2, 3, 0));
  SendTouchGestures(MoveTouchPoint(), packet1);
  EXPECT_FALSE(GesturesSent());

  // Additional pending touch move.
  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));

  // Ack back of the queue consumed.
  SendTouchConsumedAckForQueueBack();

  // Ack the pending touch.
  GetAndResetSentGestures();
  SendTouchNotConsumedAck();

  // The consumed touch doesn't produce a gesture.
  EXPECT_TRUE(GesturesMatch(
      Gestures(ET_GESTURE_SCROLL_UPDATE),
      GetAndResetSentGestures()));
  EXPECT_EQ(gfx::PointF(2, 3), LastSentGestureLocation());

  // Pending touch move.
  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  EXPECT_FALSE(GesturesSent());

  // Ack back of the queue unconsumed (twice).
  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchNotConsumedAckForQueueBack();

  GestureEventDataPacket packet2;
  packet2.Push(CreateGesture(ET_GESTURE_SCROLL_UPDATE, 7, 8, 0));
  SendTouchGestures(MoveTouchPoint(), packet2);
  EXPECT_FALSE(GesturesSent());

  SendTouchNotConsumedAckForQueueBack();

  // Ack the pending touch.
  GetAndResetSentGestures();
  SendTouchNotConsumedAck();

  // Both touches have now been acked.
  EXPECT_TRUE(
      GesturesMatch(Gestures(ET_GESTURE_SCROLL_UPDATE, ET_GESTURE_SCROLL_UPDATE,
                             ET_GESTURE_SCROLL_UPDATE),
                    GetAndResetSentGestures()));
  EXPECT_EQ(gfx::PointF(7, 8), LastSentGestureLocation());
}

TEST_F(TouchDispositionGestureFilterTest, AckQueueGestureAtBack) {
  // Send gesture sequence
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_BEGIN));
  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_END));

  // Send second gesture sequence, and synchronously ack it.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_BEGIN));
  SendTouchNotConsumedAckForQueueBack();

  GestureEventDataPacket packet;
  packet.Push(CreateGesture(ET_GESTURE_END, 2, 3, 0));
  SendTouchGestures(ReleaseTouchPoint(), packet);
  SendTouchNotConsumedAckForQueueBack();

  // The second gesture sequence is blocked on the first.
  EXPECT_FALSE(GesturesSent());

  SendTouchNotConsumedAck();
  SendTouchNotConsumedAck();

  // Both gestures have now been acked.
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_BEGIN, ET_GESTURE_END,
                                     ET_GESTURE_BEGIN, ET_GESTURE_END),
                            GetAndResetSentGestures()));
  EXPECT_EQ(gfx::PointF(2, 3), LastSentGestureLocation());
}

TEST_F(TouchDispositionGestureFilterTest,
       SyncAcksOnlyTriggerAppropriateGestures) {
  // Queue a touch press.
  SendPacket(PressTouchPoint(), Gestures(ET_GESTURE_BEGIN));

  // Send and synchronously ack two touch moves.
  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchNotConsumedAckForQueueBack();

  SendPacket(MoveTouchPoint(), Gestures(ET_GESTURE_SCROLL_UPDATE));
  SendTouchNotConsumedAckForQueueBack();

  // Queue a touch release.
  SendPacket(ReleaseTouchPoint(), Gestures(ET_GESTURE_END));

  EXPECT_FALSE(GesturesSent());

  // Ack the touch press. All events but the release should be acked.
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_BEGIN, ET_GESTURE_SCROLL_UPDATE,
                                     ET_GESTURE_SCROLL_UPDATE),
                            GetAndResetSentGestures()));

  // The touch release still requires an ack.
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_END),
                            GetAndResetSentGestures()));
}

}  // namespace ui
