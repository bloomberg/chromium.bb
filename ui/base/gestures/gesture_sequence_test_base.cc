// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/gestures/gesture_sequence_test_base.h"

#include <vector>
#include "ui/base/gestures/gesture_configuration.h"
#include "ui/base/gestures/gesture_sequence.h"

namespace ui {
namespace test {

int FakeGestureEvent::GetLowestTouchId() const {
  if (touch_ids_bitfield_ == 0)
    return -1;
  int i = -1;
  // Find the index of the least significant 1 bit
  while (!(1 << ++i & touch_ids_bitfield_));
  return i;
}

EventType FakeTouchEvent::GetEventType() const {
  return type_;
}

gfx::Point FakeTouchEvent::GetLocation() const {
  return location_;
}

int FakeTouchEvent::GetTouchId() const {
  return touch_id_;
}

int FakeTouchEvent::GetEventFlags() const {
  return 0;
}

base::TimeDelta FakeTouchEvent::GetTimestamp() const {
  return timestamp_;
};

TouchEvent* FakeTouchEvent::Copy() const {
  return new FakeTouchEvent(type_, location_, touch_id_, timestamp_);
};


GestureSequenceTestBase::GestureSequenceTestBase() {
}

GestureSequenceTestBase::~GestureSequenceTestBase() {
}

GestureEvent* MockGestureEventHelper::CreateGestureEvent(EventType type,
      const gfx::Point& location,
      int flags,
      const base::Time time,
      float param_first,
      float param_second,
      unsigned int touch_ids_bitfield) {
  return new FakeGestureEvent(type, param_first,
                              param_second, touch_ids_bitfield);
}


TouchEvent* MockGestureEventHelper::CreateTouchEvent(EventType type,
      const gfx::Point& location,
      int touch_id,
      base::TimeDelta time_stamp) {
  return NULL;
}

bool MockGestureEventHelper::DispatchLongPressGestureEvent(
    GestureEvent* event) {
  long_press_fired_ = true;
  return true;
};

bool MockGestureEventHelper::DispatchCancelTouchEvent(TouchEvent* event) {
  return false;
}

FakeGestures* GestureSequenceTestBase::Press(
    int x, int y, int touch_id) {
  return DoTouchWithDelay(ET_TOUCH_PRESSED, x, y, touch_id, 0);
}

FakeGestures* GestureSequenceTestBase::Move(
    int x, int y, int touch_id) {
  return DoTouchWithDelay(ET_TOUCH_MOVED, x, y, touch_id, 0);
}

FakeGestures* GestureSequenceTestBase::Release(
    int x, int y, int touch_id) {
  return DoTouchWithDelay(ET_TOUCH_RELEASED, x, y, touch_id, 0);
}

FakeGestures* GestureSequenceTestBase::Cancel(
    int x, int y, int touch_id) {
  return DoTouchWithDelay(ET_TOUCH_CANCELLED, x, y, touch_id, 0);
}

FakeGestures* GestureSequenceTestBase::Press(
    int x, int y, int touch_id, int delay) {
  return DoTouchWithDelay(ET_TOUCH_PRESSED, x, y, touch_id, delay);
}

FakeGestures* GestureSequenceTestBase::Move(
    int x, int y, int touch_id, int delay) {
  return DoTouchWithDelay(ET_TOUCH_MOVED, x, y, touch_id, delay);
}

FakeGestures* GestureSequenceTestBase::Release(
    int x, int y, int touch_id, int delay) {
  return DoTouchWithDelay(ET_TOUCH_RELEASED, x, y, touch_id, delay);
}

FakeGestures* GestureSequenceTestBase::Cancel(
    int x, int y, int touch_id, int delay) {
  return DoTouchWithDelay(ET_TOUCH_CANCELLED, x, y, touch_id, delay);
}

FakeGestures* GestureSequenceTestBase::ThreeFingeredSwipe(
    int id1, int x1, int y1, int id2, int x2, int y2, int id3, int x3, int y3) {
  Press(0, 0, id1);
  Press(0, 0, id2);
  Press(0, 0, id3);

  FakeGestures* gestures = new FakeGestures();
  for (int i = 0;
       i < GestureConfiguration::points_buffered_for_velocity();
       ++i) {
    FakeGestures* a = Move(x1 * i, y1 * i, id1);
    FakeGestures* b = Move(x2 * i, y2 * i, id2);
    FakeGestures* c = Move(x3 * i, y3 * i, id3);
    gestures->insert(gestures->end(), a->begin(), a->end());
    gestures->insert(gestures->end(), b->begin(), b->end());
    gestures->insert(gestures->end(), c->begin(), c->end());
  }

  return gestures;
}

FakeGestures* GestureSequenceTestBase::DoTouchWithDelay(
    EventType type,
    int x,
    int y,
    int touch_id,
    int delay) {
  base::TimeDelta now = base::Time::NowFromSystemTime() - base::Time();

  if (last_event_time_ < now)
    last_event_time_ = now;

  last_event_time_ += base::TimeDelta::FromMilliseconds(delay);
  FakeTouchEvent event(type, gfx::Point(x, y), touch_id, last_event_time_);

  GestureRecognizer::Gestures* gestures =
      gesture_sequence()->ProcessTouchEventForGesture(
          event, TOUCH_STATUS_UNKNOWN);

  FakeGestures* fake_gestures = NULL;

  if (gestures) {
    fake_gestures = new FakeGestures();

    for (size_t i = 0; i < gestures->size(); ++i)
      fake_gestures->push_back(static_cast<FakeGestureEvent*>((*gestures)[i]));

    gestures->weak_erase(gestures->begin(), gestures->end());
    delete gestures;
  }

  GestureState current_state = gesture_sequence()->state();
  if (current_state != states_->back())
    states_->push_back(gesture_sequence()->state());
  return fake_gestures;
}

bool GestureSequenceTestBase::EventTypesAre(
    FakeGestures* gestures,
    size_t num,
    ... /*EventType types */) {
  if (num != gestures->size())
    return false;

  va_list types;
  va_start(types, num);

  for (size_t i = 0; i < num; ++i) {
    if ((*gestures)[i]->type() != va_arg(types, int)) {
      va_end(types);
      return false;
    }
  }
  va_end(types);
  return true;
}

bool GestureSequenceTestBase::StatesAre(size_t num,
                                        ... /*GestureState* states*/) {
  if (num != states_->size())
    return false;

  va_list states;
  va_start(states, num);

  for (size_t i = 0; i < num; ++i) {
    if ((*states_)[i] != va_arg(states, int)) {
      va_end(states);
      return false;
    }
  }
  va_end(states);
  return true;
}

void GestureSequenceTestBase::SetUp() {
  testing::Test::SetUp();

  // Changing the parameters for gesture recognition shouldn't cause
  // tests to fail, so we use a separate set of parameters for unit
  // testing.
  GestureConfiguration::use_test_values();

  states_.reset(new std::vector<GestureState>());
  gesture_sequence_.reset(new TestGestureSequence(&helper_));
  states_->push_back(gesture_sequence()->state());
}

void GestureSequenceTestBase::TearDown() {
  gesture_sequence_.reset();
  testing::Test::TearDown();
}

}  // namespace test
}  // namespace ui
