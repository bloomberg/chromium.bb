// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/gesture_detection/mock_motion_event.h"
#include "ui/events/gesture_detection/touch_disposition_gesture_filter.h"

namespace ui {

class TouchDispositionGestureFilterTest
    : public testing::Test,
      public TouchDispositionGestureFilterClient {
 public:
  TouchDispositionGestureFilterTest() : sent_gesture_count_(0) {}
  virtual ~TouchDispositionGestureFilterTest() {}

  // testing::Test
  virtual void SetUp() OVERRIDE {
    queue_.reset(new TouchDispositionGestureFilter(this));
  }

  virtual void TearDown() OVERRIDE {
    queue_.reset();
  }

  // TouchDispositionGestureFilterClient
  virtual void ForwardGestureEvent(const GestureEventData& event) OVERRIDE {
    ++sent_gesture_count_;
    sent_gestures_.push_back(event.type);
  }

 protected:
  typedef std::vector<GestureEventType> GestureList;

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

  GestureList Gestures(GestureEventType type) {
    return GestureList(1, type);
  }

  GestureList Gestures(GestureEventType type0, GestureEventType type1) {
    GestureList gestures(2);
    gestures[0] = type0;
    gestures[1] = type1;
    return gestures;
  }

  GestureList Gestures(GestureEventType type0,
                       GestureEventType type1,
                       GestureEventType type2) {
    GestureList gestures(3);
    gestures[0] = type0;
    gestures[1] = type1;
    gestures[2] = type2;
    return gestures;
  }

  GestureList Gestures(GestureEventType type0,
                       GestureEventType type1,
                       GestureEventType type2,
                       GestureEventType type3) {
    GestureList gestures(4);
    gestures[0] = type0;
    gestures[1] = type1;
    gestures[2] = type2;
    gestures[3] = type3;
    return gestures;
  }

  void SendTouchGestures() {
    GestureEventDataPacket gesture_packet;
    std::swap(gesture_packet, pending_gesture_packet_);
    EXPECT_EQ(TouchDispositionGestureFilter::SUCCESS,
              SendTouchGestures(touch_event_, gesture_packet));
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
  SendTimeoutGesture(GestureEventType type) {
    return queue_->OnGesturePacket(
        GestureEventDataPacket::FromTouchTimeout(CreateGesture(type)));
  }

  TouchDispositionGestureFilter::PacketResult
  SendGesturePacket(const GestureEventDataPacket& packet) {
    return queue_->OnGesturePacket(packet);
  }

  void SendTouchEventACK(
      TouchDispositionGestureFilter::TouchEventAck ack_result) {
    queue_->OnTouchEventAck(ack_result);
  }

  void SendTouchConsumedAck() {
    SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  }

  void SendNotConsumedAck() {
    SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  }

  void SendNoConsumerExistsAck() {
    SendTouchEventACK(TouchDispositionGestureFilter::NO_CONSUMER_EXISTS);
  }

  void PushGesture(GestureEventType type) {
    pending_gesture_packet_.Push(CreateGesture(type));
  }

  void PressTouchPoint(int x, int y) {
    touch_event_.PressPoint(x, y);
    SendTouchGestures();
  }

  void MoveTouchPoint(size_t index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
    SendTouchGestures();
  }

  void ReleaseTouchPoint() {
    touch_event_.ReleasePoint();
    SendTouchGestures();
  }

  void CancelTouchPoint() {
    touch_event_.CancelPoint();
    SendTouchGestures();
  }

  bool GesturesSent() const {
    return !sent_gestures_.empty();
  }

  bool IsEmpty() const {
    return queue_->IsEmpty();
  }

  GestureList GetAndResetSentGestures() {
    GestureList sent_gestures;
    sent_gestures.swap(sent_gestures_);
    return sent_gestures;
  }

  static GestureEventData CreateGesture(GestureEventType type) {
    return GestureEventData(
        type, base::TimeTicks(), 0, 0, GestureEventData::Details());
  }

 private:
  scoped_ptr<TouchDispositionGestureFilter> queue_;
  MockMotionEvent touch_event_;
  GestureEventDataPacket pending_gesture_packet_;
  size_t sent_gesture_count_;
  GestureList sent_gestures_;
};

TEST_F(TouchDispositionGestureFilterTest, BasicNoGestures) {
  PressTouchPoint(1, 1);
  EXPECT_FALSE(GesturesSent());

  MoveTouchPoint(0, 2, 2);
  EXPECT_FALSE(GesturesSent());

  // No gestures should be dispatched by the ack, as the queued packets
  // contained no gestures.
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // Release the touch gesture.
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, BasicGestures) {
  // An unconsumed touch's gesture should be sent.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  EXPECT_FALSE(GesturesSent());
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  // Multiple gestures can be queued for a single event.
  PushGesture(GESTURE_FLING_START);
  PushGesture(GESTURE_FLING_CANCEL);
  ReleaseTouchPoint();
  EXPECT_FALSE(GesturesSent());
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_FLING_START,
                                     GESTURE_FLING_CANCEL),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, BasicGesturesConsumed) {
  // A consumed touch's gesture should not be sent.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(GESTURE_FLING_START);
  PushGesture(GESTURE_FLING_CANCEL);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, ConsumedThenNotConsumed) {
  // A consumed touch's gesture should not be sent.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // Even if the subsequent touch is not consumed, continue dropping gestures.
  PushGesture(GESTURE_SCROLL_UPDATE);
  MoveTouchPoint(0, 2, 2);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // Even if the subsequent touch had no consumer, continue dropping gestures.
  PushGesture(GESTURE_FLING_START);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, NotConsumedThenConsumed) {
  // A not consumed touch's gesture should be sent.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  // A newly consumed gesture should not be sent.
  PushGesture(GESTURE_PINCH_BEGIN);
  PressTouchPoint(10, 10);
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // And subsequent non-consumed pinch updates should not be sent.
  PushGesture(GESTURE_SCROLL_UPDATE);
  PushGesture(GESTURE_PINCH_UPDATE);
  MoveTouchPoint(0, 2, 2);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_UPDATE),
                            GetAndResetSentGestures()));

  // End events dispatched only when their start events were.
  PushGesture(GESTURE_PINCH_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, ScrollAlternatelyConsumed) {
  // A consumed touch's gesture should not be sent.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  for (size_t i = 0; i < 3; ++i) {
    PushGesture(GESTURE_SCROLL_UPDATE);
    MoveTouchPoint(0, 2, 2);
    SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
    EXPECT_FALSE(GesturesSent());

    PushGesture(GESTURE_SCROLL_UPDATE);
    MoveTouchPoint(0, 3, 3);
    SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
    EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_UPDATE),
                              GetAndResetSentGestures()));
  }

  PushGesture(GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, NotConsumedThenNoConsumer) {
  // An unconsumed touch's gesture should be sent.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  // If the subsequent touch has no consumer (e.g., a secondary pointer is
  // pressed but not on a touch handling rect), send the gesture.
  PushGesture(GESTURE_PINCH_BEGIN);
  PressTouchPoint(2, 2);
  SendTouchEventACK(TouchDispositionGestureFilter::NO_CONSUMER_EXISTS);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_PINCH_BEGIN),
                            GetAndResetSentGestures()));

  // End events should be dispatched when their start events were, independent
  // of the ack state.
  PushGesture(GESTURE_PINCH_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_PINCH_END),
                            GetAndResetSentGestures()));

  PushGesture(GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, EndingEventsSent) {
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  PushGesture(GESTURE_PINCH_BEGIN);
  PressTouchPoint(2, 2);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_PINCH_BEGIN),
                            GetAndResetSentGestures()));

  // Consuming the touchend event can't suppress the match end gesture.
  PushGesture(GESTURE_PINCH_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_PINCH_END),
                            GetAndResetSentGestures()));

  // But other events in the same packet are still suppressed.
  PushGesture(GESTURE_SCROLL_UPDATE);
  PushGesture(GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));

  // GESTURE_SCROLL_END and GESTURE_FLING_START behave the same in this regard.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  PushGesture(GESTURE_FLING_START);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_FLING_START),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, EndingEventsNotSent) {
  // Consuming a begin event ensures no end events are sent.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(GESTURE_PINCH_BEGIN);
  PressTouchPoint(2, 2);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(GESTURE_PINCH_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, UpdateEventsSuppressedPerEvent) {
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  // Consuming a single scroll or pinch update should suppress only that event.
  PushGesture(GESTURE_SCROLL_UPDATE);
  MoveTouchPoint(0, 2, 2);
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(GESTURE_PINCH_BEGIN);
  PressTouchPoint(2, 2);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_PINCH_BEGIN),
                            GetAndResetSentGestures()));

  PushGesture(GESTURE_PINCH_UPDATE);
  MoveTouchPoint(1, 2, 3);
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // Subsequent updates should not be affected.
  PushGesture(GESTURE_SCROLL_UPDATE);
  MoveTouchPoint(0, 4, 4);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_UPDATE),
                            GetAndResetSentGestures()));

  PushGesture(GESTURE_PINCH_UPDATE);
  MoveTouchPoint(0, 4, 5);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_PINCH_UPDATE),
                            GetAndResetSentGestures()));

  PushGesture(GESTURE_PINCH_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_PINCH_END),
                            GetAndResetSentGestures()));

  PushGesture(GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, UpdateEventsDependOnBeginEvents) {
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // Scroll and pinch gestures depend on the scroll begin gesture being
  // dispatched.
  PushGesture(GESTURE_SCROLL_UPDATE);
  MoveTouchPoint(0, 2, 2);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(GESTURE_PINCH_BEGIN);
  PressTouchPoint(2, 2);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(GESTURE_PINCH_UPDATE);
  MoveTouchPoint(1, 2, 3);
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(GESTURE_PINCH_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, MultipleTouchSequences) {
  // Queue two touch-to-gestures sequences.
  PushGesture(GESTURE_TAP_DOWN);
  PressTouchPoint(1, 1);
  PushGesture(GESTURE_TAP);
  ReleaseTouchPoint();
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  PushGesture(GESTURE_SCROLL_END);
  ReleaseTouchPoint();

  // The first gesture sequence should not be allowed.
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // The subsequent sequence should "reset" allowance.
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN,
                                     GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, FlingCancelledOnNewTouchSequence) {
  // Simulate a fling.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NO_CONSUMER_EXISTS);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));
  PushGesture(GESTURE_FLING_START);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_FLING_START),
                            GetAndResetSentGestures()));

  // A new touch seqeuence should cancel the outstanding fling.
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NO_CONSUMER_EXISTS);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_FLING_CANCEL),
                            GetAndResetSentGestures()));
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NO_CONSUMER_EXISTS);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, ScrollEndedOnNewTouchSequence) {
  // Simulate a scroll.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NO_CONSUMER_EXISTS);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NO_CONSUMER_EXISTS);

  // A new touch seqeuence should end the outstanding scroll.
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, FlingCancelledOnScrollBegin) {
  // Simulate a fling sequence.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PushGesture(GESTURE_FLING_START);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN,
                                     GESTURE_FLING_START),
                            GetAndResetSentGestures()));

  // The new fling should cancel the preceding one.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PushGesture(GESTURE_FLING_START);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_FLING_CANCEL,
                                     GESTURE_SCROLL_BEGIN,
                                     GESTURE_FLING_START),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, FlingNotCancelledIfGFCEventReceived) {
  // Simulate a fling that is started then cancelled.
  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NO_CONSUMER_EXISTS);
  PushGesture(GESTURE_FLING_START);
  MoveTouchPoint(0, 1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NO_CONSUMER_EXISTS);
  PushGesture(GESTURE_FLING_CANCEL);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN,
                                     GESTURE_FLING_START,
                                     GESTURE_FLING_CANCEL),
                            GetAndResetSentGestures()));

  // A new touch sequence will not inject a GESTURE_FLING_CANCEL, as the fling
  // has already been cancelled.
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NO_CONSUMER_EXISTS);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NO_CONSUMER_EXISTS);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledWhenScrollBegins) {
  PushGesture(GESTURE_TAP_DOWN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  // If the subsequent touch turns into a scroll, the tap should be cancelled.
  PushGesture(GESTURE_SCROLL_BEGIN);
  MoveTouchPoint(0, 2, 2);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_TAP_CANCEL,
                                     GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledWhenTouchConsumed) {
  PushGesture(GESTURE_TAP_DOWN);
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  // If the subsequent touch is consumed, the tap should be cancelled.
  PushGesture(GESTURE_SCROLL_BEGIN);
  MoveTouchPoint(0, 2, 2);
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_TAP_CANCEL),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest,
       TapNotCancelledIfTapEndingEventReceived) {
  PushGesture(GESTURE_TAP_DOWN);
  PressTouchPoint(1, 1);
  PressTouchPoint(2, 2);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  PushGesture(GESTURE_TAP);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_TAP),
                            GetAndResetSentGestures()));

  // The tap should not be cancelled as it was terminated by a |GESTURE_TAP|.
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, TimeoutGestures) {
  // If the sequence is allowed, and there are no preceding gestures, the
  // timeout gestures should be forwarded immediately.
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(GESTURE_SHOW_PRESS);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SHOW_PRESS),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(GESTURE_LONG_PRESS);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_LONG_PRESS),
                            GetAndResetSentGestures()));

  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);

  // If the sequence is disallowed, and there are no preceding gestures, the
  // timeout gestures should be dropped immediately.
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(GESTURE_SHOW_PRESS);
  EXPECT_FALSE(GesturesSent());
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);

  // If the sequence has a pending ack, the timeout gestures should
  // remain queued until the ack is received.
  PressTouchPoint(1, 1);
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(GESTURE_LONG_PRESS);
  EXPECT_FALSE(GesturesSent());

  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_LONG_PRESS),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, SpuriousAcksIgnored) {
  // Acks received when the queue is empty will be safely ignored.
  ASSERT_TRUE(IsEmpty());
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  PushGesture(GESTURE_SCROLL_UPDATE);
  MoveTouchPoint(0, 3,3);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_SCROLL_BEGIN,
                                     GESTURE_SCROLL_UPDATE),
                            GetAndResetSentGestures()));

  // Even if all packets have been dispatched, the filter may not be empty as
  // there could be follow-up timeout events.  Spurious acks in such cases
  // should also be safely ignored.
  ASSERT_FALSE(IsEmpty());
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
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
            SendTimeoutGesture(GESTURE_SHOW_PRESS));
  EXPECT_TRUE(IsEmpty());
}

TEST_F(TouchDispositionGestureFilterTest, ConsumedTouchCancel) {
  // An unconsumed touch's gesture should be sent.
  PushGesture(GESTURE_TAP_DOWN);
  PressTouchPoint(1, 1);
  EXPECT_FALSE(GesturesSent());
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  PushGesture(GESTURE_TAP_CANCEL);
  PushGesture(GESTURE_SCROLL_END);
  CancelTouchPoint();
  EXPECT_FALSE(GesturesSent());
  SendTouchEventACK(TouchDispositionGestureFilter::CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_TAP_CANCEL,
                                     GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TimeoutEventAfterRelease) {
  PressTouchPoint(1, 1);
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());
  PushGesture(GESTURE_TAP_UNCONFIRMED);
  ReleaseTouchPoint();
  SendTouchEventACK(TouchDispositionGestureFilter::NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_TAP_UNCONFIRMED),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(GESTURE_TAP);
  EXPECT_TRUE(GesturesMatch(Gestures(GESTURE_TAP),
                            GetAndResetSentGestures()));
}

}  // namespace ui
