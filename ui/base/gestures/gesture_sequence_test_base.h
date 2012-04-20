// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GESTURES_GESTURE_SEQUENCE_TEST_BASE_H_
#define UI_BASE_GESTURES_GESTURE_SEQUENCE_TEST_BASE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/gestures/gesture_types.h"
#include "ui/base/test/test_gesture_sequence.h"

namespace ui {
namespace test {

class FakeGestureEvent : public GestureEvent {
 public:
  FakeGestureEvent(EventType type,
                   float param_first,
                   float param_second,
                   unsigned int touch_ids_bitfield)
      : type_(type),
        param_first_(param_first),
        param_second_(param_second),
        touch_ids_bitfield_(touch_ids_bitfield) {
  }

  virtual ~FakeGestureEvent() {}

  EventType type() const { return type_; }
  float param_first() const { return param_first_; }
  float param_second() const { return param_second_; }

  virtual int GetLowestTouchId() const OVERRIDE;

  EventType type_;
  float param_first_;
  float param_second_;
  int touch_ids_bitfield_;
};

typedef ScopedVector<FakeGestureEvent> FakeGestures;

class FakeTouchEvent : public TouchEvent {
 public:
  FakeTouchEvent(
      EventType type,
      gfx::Point location,
      int touch_id,
      base::TimeDelta timestamp)
      : type_(type),
        location_(location),
        touch_id_(touch_id),
        timestamp_(timestamp) {
  }

  virtual ~FakeTouchEvent() {}

  virtual EventType GetEventType() const OVERRIDE;
  virtual gfx::Point GetLocation() const OVERRIDE;
  virtual int GetTouchId() const OVERRIDE;
  virtual int GetEventFlags() const OVERRIDE;
  virtual base::TimeDelta GetTimestamp() const OVERRIDE;

  // Returns a copy of this touch event. Used when queueing events for
  // asynchronous gesture recognition.
  virtual TouchEvent* Copy() const OVERRIDE;

  EventType type_;
  gfx::Point location_;
  int touch_id_;
  int flags_;
  base::TimeDelta timestamp_;
};

class MockGestureEventHelper : public GestureEventHelper {
 public:
  MockGestureEventHelper()
      : long_press_fired_(false) {
  }

  virtual GestureEvent* CreateGestureEvent(
      EventType type,
      const gfx::Point& location,
      int flags,
      const base::Time time,
      float param_first,
      float param_second,
      unsigned int touch_ids_bitfield) OVERRIDE;

  virtual TouchEvent* CreateTouchEvent(EventType type,
                                       const gfx::Point& location,
                                       int touch_id,
                                       base::TimeDelta time_stamp) OVERRIDE;

  virtual bool DispatchLongPressGestureEvent(GestureEvent* event) OVERRIDE;

  virtual bool DispatchCancelTouchEvent(TouchEvent* event) OVERRIDE;

  bool long_press_fired() {
    return long_press_fired_;
  }

  void Reset() {
    long_press_fired_ = false;
  }

 private:
  bool long_press_fired_;
};

// A base class for gesture sequence unit tests.

class GestureSequenceTestBase : public testing::Test {
 public:
  GestureSequenceTestBase();
  virtual ~GestureSequenceTestBase();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  TestGestureSequence* gesture_sequence() { return gesture_sequence_.get(); }

  FakeGestures* Press(int x, int y, int touch_id);
  FakeGestures* Press(int x, int y, int touch_id, int delay);
  FakeGestures* Move(int x, int y, int touch_id);
  FakeGestures* Move(int x, int y, int touch_id, int delay);
  FakeGestures* Release(int x, int y, int touch_id);
  FakeGestures* Release(int x, int y, int touch_id, int delay);
  FakeGestures* Cancel(int x, int y, int touch_id);
  FakeGestures* Cancel(int x, int y, int touch_id, int delay);

  FakeGestures* ThreeFingeredSwipe(
      int id1, int x1, int y1,
      int id2, int x2, int y2,
      int id3, int x3, int y3);

  bool EventTypesAre(FakeGestures* gestures,
                     size_t num,
                     ... /*EventType types */);
  bool StatesAre(size_t num, ... /*GestureState* states*/);
  MockGestureEventHelper helper_;

 private:
  FakeGestures* DoTouchWithDelay(
      EventType type,
      int x,
      int y,
      int touch_id,
      int delay);

  scoped_ptr<TestGestureSequence> gesture_sequence_;
  scoped_ptr<std::vector<GestureState> > states_;
  base::TimeDelta last_event_time_;
  MessageLoopForUI message_loop_;
  DISALLOW_COPY_AND_ASSIGN(GestureSequenceTestBase);
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_GESTURES_GESTURE_SEQUENCE_TEST_BASE_H_
