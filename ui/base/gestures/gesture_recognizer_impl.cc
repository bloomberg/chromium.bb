// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/gestures/gesture_recognizer_impl.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "ui/base/events.h"
#include "ui/base/gestures/gesture_configuration.h"
#include "ui/base/gestures/gesture_sequence.h"
#include "ui/base/gestures/gesture_types.h"

namespace {
// This is used to pop a std::queue when returning from a function.
class ScopedPop {
 public:
  explicit ScopedPop(std::queue<ui::TouchEvent*>* queue) : queue_(queue) {
  }

  ~ScopedPop() {
    delete queue_->front();
    queue_->pop();
  }

 private:
  std::queue<ui::TouchEvent*>* queue_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPop);
};

// CancelledTouchEvent mirrors a ui::TouchEvent object, except for the
// type, which is always ET_TOUCH_CANCELLED.
class CancelledTouchEvent : public ui::TouchEvent {
 public:
  explicit CancelledTouchEvent(ui::TouchEvent* real)
      : src_event_(real) {
  }

  virtual ~CancelledTouchEvent() {
  }

 private:
  // Overridden from ui::TouchEvent.
  virtual ui::EventType GetEventType() const OVERRIDE {
    return ui::ET_TOUCH_CANCELLED;
  }

  virtual gfx::Point GetLocation() const OVERRIDE {
    return src_event_->GetLocation();
  }

  virtual int GetTouchId() const OVERRIDE {
    return src_event_->GetTouchId();
  }

  virtual int GetEventFlags() const OVERRIDE {
    return src_event_->GetEventFlags();
  }

  virtual base::TimeDelta GetTimestamp() const OVERRIDE {
    return src_event_->GetTimestamp();
  }

  virtual ui::TouchEvent* Copy() const OVERRIDE {
    return NULL;
  }

  virtual float RadiusX() const OVERRIDE {
    return src_event_->RadiusX();
  }

  virtual float RadiusY() const OVERRIDE {
    return src_event_->RadiusY();
  }

  virtual float RotationAngle() const OVERRIDE {
    return src_event_->RotationAngle();
  }

  virtual float Force() const OVERRIDE {
    return src_event_->Force();
  }

  ui::TouchEvent* src_event_;
  DISALLOW_COPY_AND_ASSIGN(CancelledTouchEvent);
};

// Touches which are cancelled by a touch capture are routed to a
// GestureEventIgnorer, which ignores them.
class GestureConsumerIgnorer : public ui::GestureConsumer {
 public:
  GestureConsumerIgnorer()
      : GestureConsumer(true) {
  }
};

}  // namespace

namespace ui {

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, public:

GestureRecognizerImpl::GestureRecognizerImpl(GestureEventHelper* helper)
    : helper_(helper) {
  gesture_consumer_ignorer_.reset(new GestureConsumerIgnorer());
}

GestureRecognizerImpl::~GestureRecognizerImpl() {
}

GestureConsumer* GestureRecognizerImpl::GetTouchLockedTarget(
    TouchEvent* event) {
  return touch_id_target_[event->GetTouchId()];
}

GestureConsumer* GestureRecognizerImpl::GetTargetForGestureEvent(
    GestureEvent* event) {
  GestureConsumer* target = NULL;
  int touch_id = event->GetLowestTouchId();
  target = touch_id_target_for_gestures_[touch_id];
  return target;
}

GestureConsumer* GestureRecognizerImpl::GetTargetForLocation(
    const gfx::Point& location) {
  const GesturePoint* closest_point = NULL;
  int closest_distance_squared = 0;
  std::map<GestureConsumer*, GestureSequence*>::iterator i;
  for (i = consumer_sequence_.begin(); i != consumer_sequence_.end(); ++i) {
    const GesturePoint* points = i->second->points();
    for (int j = 0; j < GestureSequence::kMaxGesturePoints; ++j) {
      if (!points[j].in_use())
        continue;
      gfx::Point delta =
          points[j].last_touch_position().Subtract(location);
      int distance = delta.x() * delta.x() + delta.y() * delta.y();
      if (!closest_point || distance < closest_distance_squared) {
        closest_point = &points[j];
        closest_distance_squared = distance;
      }
    }
  }

  const int max_distance =
      GestureConfiguration::max_separation_for_gesture_touches_in_pixels();

  if (closest_distance_squared < max_distance * max_distance && closest_point)
    return touch_id_target_[closest_point->touch_id()];
  else
    return NULL;
}

void GestureRecognizerImpl::CancelNonCapturedTouches(
    GestureConsumer* capturer) {
  std::map<int, GestureConsumer*>::iterator i;
  // Fire touch cancels, and target touch_ids to gesture_consumer_ignorer.
  for (i = touch_id_target_.begin(); i != touch_id_target_.end(); ++i) {
    if (capturer != i->second) {
      scoped_ptr<TouchEvent> touchEvent(helper_->CreateTouchEvent(
            ui::ET_TOUCH_CANCELLED, gfx::Point(0, 0),
            i->first,
            base::Time::NowFromSystemTime() - base::Time()));
      helper_->DispatchCancelTouchEvent(touchEvent.get());
      touch_id_target_[i->first] = gesture_consumer_ignorer_.get();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, protected:

GestureSequence* GestureRecognizerImpl::CreateSequence(
    GestureEventHelper* helper) {
  return new GestureSequence(helper);
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, private:

GestureSequence* GestureRecognizerImpl::GetGestureSequenceForConsumer(
    GestureConsumer* consumer) {
  GestureSequence* gesture_sequence = consumer_sequence_[consumer];
  if (!gesture_sequence) {
    gesture_sequence = CreateSequence(helper_);
    consumer_sequence_[consumer] = gesture_sequence;
  }
  return gesture_sequence;
}

GestureSequence::Gestures* GestureRecognizerImpl::ProcessTouchEventForGesture(
    const TouchEvent& event,
    ui::TouchStatus status,
    GestureConsumer* target) {
  if (event.GetEventType() == ui::ET_TOUCH_RELEASED ||
      event.GetEventType() == ui::ET_TOUCH_CANCELLED) {
    touch_id_target_[event.GetTouchId()] = NULL;
  } else {
    touch_id_target_[event.GetTouchId()] = target;
    if (target)
      touch_id_target_for_gestures_[event.GetTouchId()] = target;
  }

  GestureSequence* gesture_sequence = GetGestureSequenceForConsumer(target);
  return gesture_sequence->ProcessTouchEventForGesture(event, status);
}

void GestureRecognizerImpl::QueueTouchEventForGesture(GestureConsumer* consumer,
                                                      const TouchEvent& event) {
  if (!event_queue_[consumer])
    event_queue_[consumer] = new std::queue<TouchEvent*>();
  event_queue_[consumer]->push(event.Copy());
}

GestureSequence::Gestures* GestureRecognizerImpl::AdvanceTouchQueue(
    GestureConsumer* consumer,
    bool processed) {
  if (!event_queue_[consumer] || event_queue_[consumer]->empty()) {
    LOG(ERROR) << "Trying to advance an empty gesture queue for " << consumer;
    return NULL;
  }

  ScopedPop pop(event_queue_[consumer]);
  TouchEvent* event = event_queue_[consumer]->front();

  GestureSequence* sequence = GetGestureSequenceForConsumer(consumer);

  if (processed && event->GetEventType() == ui::ET_TOUCH_RELEASED) {
    // A touch release was was processed (e.g. preventDefault()ed by a
    // web-page), but we still need to process a touch cancel.
    CancelledTouchEvent cancelled(event);
    return sequence->ProcessTouchEventForGesture(cancelled,
                                                 ui::TOUCH_STATUS_UNKNOWN);
  }

  return sequence->ProcessTouchEventForGesture(
      *event,
      processed ? ui::TOUCH_STATUS_CONTINUE : ui::TOUCH_STATUS_UNKNOWN);
}

void GestureRecognizerImpl::FlushTouchQueue(GestureConsumer* consumer) {
  if (consumer_sequence_.count(consumer)) {
    delete consumer_sequence_[consumer];
    consumer_sequence_.erase(consumer);
  }

  if (event_queue_.count(consumer)) {
    delete event_queue_[consumer];
    event_queue_.erase(consumer);
  }

  int touch_id = -1;
  std::map<int, GestureConsumer*>::iterator i;
  for (i = touch_id_target_.begin(); i != touch_id_target_.end(); ++i) {
    if (i->second == consumer)
      touch_id = i->first;
  }

  if (touch_id_target_.count(touch_id))
    touch_id_target_.erase(touch_id);

  for (i = touch_id_target_for_gestures_.begin();
       i != touch_id_target_for_gestures_.end();
       ++i) {
    if (i->second == consumer)
      touch_id = i->first;
  }

  if (touch_id_target_for_gestures_.count(touch_id))
    touch_id_target_for_gestures_.erase(touch_id);
}

// GestureRecognizer, static
GestureRecognizer* GestureRecognizer::Create(GestureEventHelper* helper) {
  return new GestureRecognizerImpl(helper);
}

}  // namespace ui
