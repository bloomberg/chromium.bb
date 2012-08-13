// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/gestures/gesture_recognizer_impl.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "ui/base/event.h"
#include "ui/base/events.h"
#include "ui/base/gestures/gesture_configuration.h"
#include "ui/base/gestures/gesture_sequence.h"
#include "ui/base/gestures/gesture_types.h"

namespace ui {

namespace {

// This is used to pop a std::queue when returning from a function.
class ScopedPop {
 public:
  explicit ScopedPop(std::queue<TouchEvent*>* queue) : queue_(queue) {
  }

  ~ScopedPop() {
    delete queue_->front();
    queue_->pop();
  }

 private:
  std::queue<TouchEvent*>* queue_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPop);
};

// CancelledTouchEvent mirrors a TouchEvent object.
class MirroredTouchEvent : public TouchEvent {
 public:
  explicit MirroredTouchEvent(const TouchEvent* real)
      : TouchEvent(real->type(),
                   real->location(),
                   real->touch_id(),
                   real->time_stamp()) {
    set_flags(real->flags());
    set_radius(real->radius_x(), real->radius_y());
    set_rotation_angle(real->rotation_angle());
    set_force(real->force());
  }

  virtual ~MirroredTouchEvent() {
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(MirroredTouchEvent);
};

// A mirrored event, except for the type, which is always ET_TOUCH_CANCELLED.
class CancelledTouchEvent : public MirroredTouchEvent {
 public:
  explicit CancelledTouchEvent(const TouchEvent* src)
      : MirroredTouchEvent(src) {
    set_type(ET_TOUCH_CANCELLED);
  }

  virtual ~CancelledTouchEvent() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CancelledTouchEvent);
};

// Touches which are cancelled by a touch capture are routed to a
// GestureEventIgnorer, which ignores them.
class GestureConsumerIgnorer : public GestureConsumer {
 public:
  GestureConsumerIgnorer()
      : GestureConsumer(true) {
  }
};

template <typename T>
void TransferConsumer(GestureConsumer* current_consumer,
                      GestureConsumer* new_consumer,
                      std::map<GestureConsumer*, T>* map) {
  if (map->count(current_consumer)) {
    (*map)[new_consumer] = (*map)[current_consumer];
    map->erase(current_consumer);
  }
}

void RemoveConsumerFromMap(GestureConsumer* consumer,
                           GestureRecognizerImpl::TouchIdToConsumerMap* map) {
  for (GestureRecognizerImpl::TouchIdToConsumerMap::iterator i = map->begin();
       i != map->end();) {
    if (i->second == consumer)
      map->erase(i++);
    else
      ++i;
  }
}

void TransferTouchIdToConsumerMap(
    GestureConsumer* old_consumer,
    GestureConsumer* new_consumer,
    GestureRecognizerImpl::TouchIdToConsumerMap* map) {
  for (GestureRecognizerImpl::TouchIdToConsumerMap::iterator i = map->begin();
       i != map->end(); ++i) {
    if (i->second == old_consumer)
      i->second = new_consumer;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, public:

GestureRecognizerImpl::GestureRecognizerImpl(GestureEventHelper* helper)
    : helper_(helper) {
  gesture_consumer_ignorer_.reset(new GestureConsumerIgnorer());
}

GestureRecognizerImpl::~GestureRecognizerImpl() {
  STLDeleteValues(&consumer_sequence_);
  STLDeleteValues(&event_queue_);
}

// Checks if this finger is already down, if so, returns the current target.
// Otherwise, returns NULL.
GestureConsumer* GestureRecognizerImpl::GetTouchLockedTarget(
    TouchEvent* event) {
  return touch_id_target_[event->touch_id()];
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

void GestureRecognizerImpl::TransferEventsTo(GestureConsumer* current_consumer,
                                             GestureConsumer* new_consumer) {
  // Send cancel to all those save |new_consumer| and |current_consumer|.
  // Don't send a cancel to |current_consumer|, unless |new_consumer| is NULL.
  for (TouchIdToConsumerMap::iterator i = touch_id_target_.begin();
       i != touch_id_target_.end(); ++i) {
    if (i->second != new_consumer &&
        (i->second != current_consumer || new_consumer == NULL) &&
        i->second != gesture_consumer_ignorer_.get()) {
      TouchEvent touch_event(ui::ET_TOUCH_CANCELLED, gfx::Point(0, 0),
            i->first, base::Time::NowFromSystemTime() - base::Time());
      helper_->DispatchCancelTouchEvent(&touch_event);
      i->second = gesture_consumer_ignorer_.get();
    }
  }

  // Transer events from |current_consumer| to |new_consumer|.
  if (current_consumer && new_consumer) {
    TransferTouchIdToConsumerMap(current_consumer, new_consumer,
                                 &touch_id_target_);
    TransferTouchIdToConsumerMap(current_consumer, new_consumer,
                                 &touch_id_target_for_gestures_);
    TransferConsumer(current_consumer, new_consumer, &event_queue_);
    TransferConsumer(current_consumer, new_consumer, &consumer_sequence_);
  }
}

bool GestureRecognizerImpl::GetLastTouchPointForTarget(
    GestureConsumer* consumer,
    gfx::Point* point) {
  if (consumer_sequence_.count(consumer) == 0)
    return false;

  *point = consumer_sequence_[consumer]->last_touch_location();
  return true;
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
  if (event.type() == ui::ET_TOUCH_RELEASED ||
      event.type() == ui::ET_TOUCH_CANCELLED) {
    touch_id_target_[event.touch_id()] = NULL;
  } else {
    touch_id_target_[event.touch_id()] = target;
    if (target)
      touch_id_target_for_gestures_[event.touch_id()] = target;
  }

  GestureSequence* gesture_sequence = GetGestureSequenceForConsumer(target);
  return gesture_sequence->ProcessTouchEventForGesture(event, status);
}

void GestureRecognizerImpl::QueueTouchEventForGesture(GestureConsumer* consumer,
                                                      const TouchEvent& event) {
  if (!event_queue_[consumer])
    event_queue_[consumer] = new std::queue<TouchEvent*>();
  event_queue_[consumer]->push(new MirroredTouchEvent(&event));
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

  if (processed && event->type() == ui::ET_TOUCH_RELEASED) {
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

  RemoveConsumerFromMap(consumer, &touch_id_target_);
  RemoveConsumerFromMap(consumer, &touch_id_target_for_gestures_);
}

// GestureRecognizer, static
GestureRecognizer* GestureRecognizer::Create(GestureEventHelper* helper) {
  return new GestureRecognizerImpl(helper);
}

}  // namespace ui
