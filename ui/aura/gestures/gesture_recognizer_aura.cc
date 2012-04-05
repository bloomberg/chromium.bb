// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/gestures/gesture_recognizer_aura.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "ui/aura/event.h"
#include "ui/aura/gestures/gesture_configuration.h"
#include "ui/aura/gestures/gesture_sequence.h"
#include "ui/aura/window.h"
#include "ui/base/events.h"

namespace {
// This is used to pop a std::queue when returning from a function.
class ScopedPop {
 public:
  explicit ScopedPop(std::queue<aura::TouchEvent*>* queue) : queue_(queue) {
  }

  ~ScopedPop() {
    delete queue_->front();
    queue_->pop();
  }

 private:
  std::queue<aura::TouchEvent*>* queue_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPop);
};

}  // namespace

namespace aura {

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerAura, public:

GestureRecognizerAura::GestureRecognizerAura() {
}

GestureRecognizerAura::~GestureRecognizerAura() {
}

Window* GestureRecognizerAura::GetTargetForTouchEvent(TouchEvent* event) {
  Window* target = touch_id_target_[event->touch_id()];
  if (!target)
    target = GetTargetForLocation(event->location());
  return target;
}

Window* GestureRecognizerAura::GetTargetForGestureEvent(GestureEvent* event) {
  Window* target = NULL;
  int touch_id = event->GetLowestTouchId();
  target = touch_id_target_for_gestures_[touch_id];
  return target;
}

Window* GestureRecognizerAura::GetTargetForLocation(
    const gfx::Point& location) {
  const GesturePoint* closest_point = NULL;
  int closest_distance_squared = 0;
  std::map<Window*, GestureSequence*>::iterator i;
  for (i = window_sequence_.begin(); i != window_sequence_.end(); ++i) {
    const GesturePoint* points = i->second->points();
    for (int j = 0; j < GestureSequence::kMaxGesturePoints; ++j) {
      if (!points[j].in_use())
        continue;
      gfx::Point delta =
          points[j].last_touch_position().Subtract(location);
      int distance = delta.x() * delta.x() + delta.y() * delta.y();
      if ( !closest_point || distance < closest_distance_squared ) {
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

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerAura, protected:

GestureSequence* GestureRecognizerAura::CreateSequence(
    RootWindow* root_window) {
  return new GestureSequence(root_window);
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerAura, private:

GestureSequence* GestureRecognizerAura::GetGestureSequenceForWindow(
    Window* window) {
  GestureSequence* gesture_sequence = window_sequence_[window];
  if (!gesture_sequence) {
    gesture_sequence = CreateSequence(window->GetRootWindow());
    window_sequence_[window] = gesture_sequence;
  }
  return gesture_sequence;
}

GestureSequence::Gestures* GestureRecognizerAura::ProcessTouchEventForGesture(
    const TouchEvent& event,
    ui::TouchStatus status,
    Window* target) {
  if (event.type() == ui::ET_TOUCH_RELEASED ||
      event.type() == ui::ET_TOUCH_CANCELLED) {
    touch_id_target_[event.touch_id()] = NULL;
  } else {
    touch_id_target_[event.touch_id()] = target;
    if (target)
      touch_id_target_for_gestures_[event.touch_id()] = target;
  }

  GestureSequence* gesture_sequence = GetGestureSequenceForWindow(target);
  return gesture_sequence->ProcessTouchEventForGesture(event, status);
}

void GestureRecognizerAura::QueueTouchEventForGesture(Window* window,
                                                      const TouchEvent& event) {
  if (!event_queue_[window])
    event_queue_[window] = new std::queue<TouchEvent*>();
  event_queue_[window]->push(event.Copy());
}

GestureSequence::Gestures* GestureRecognizerAura::AdvanceTouchQueue(
    Window* window,
    bool processed) {
  if (!event_queue_[window] || event_queue_[window]->empty()) {
    LOG(ERROR) << "Trying to advance an empty gesture queue for " << window;
    return NULL;
  }

  ScopedPop pop(event_queue_[window]);
  TouchEvent* event = event_queue_[window]->front();

  GestureSequence* sequence = GetGestureSequenceForWindow(window);

  if (processed && event->type() == ui::ET_TOUCH_RELEASED) {
    // A touch release was was processed (e.g. preventDefault()ed by a
    // web-page), but we still need to process a touch cancel.
    TouchEvent processed_event(ui::ET_TOUCH_CANCELLED,
                               event->location(),
                               event->touch_id(),
                               event->time_stamp());
    return sequence->ProcessTouchEventForGesture(
        processed_event,
        ui::TOUCH_STATUS_UNKNOWN);
  }

  return sequence->ProcessTouchEventForGesture(
      *event,
      processed ? ui::TOUCH_STATUS_CONTINUE : ui::TOUCH_STATUS_UNKNOWN);
}

void GestureRecognizerAura::FlushTouchQueue(Window* window) {
  if (window_sequence_.count(window)) {
    delete window_sequence_[window];
    window_sequence_.erase(window);
  }

  if (event_queue_.count(window)) {
    delete event_queue_[window];
    event_queue_.erase(window);
  }

  int touch_id = -1;
  std::map<int, Window*>::iterator i;
  for (i = touch_id_target_.begin(); i != touch_id_target_.end(); ++i) {
    if (i->second == window)
      touch_id = i->first;
  }

  if (touch_id_target_.count(touch_id))
    touch_id_target_.erase(touch_id);

  for (i = touch_id_target_for_gestures_.begin();
       i != touch_id_target_for_gestures_.end();
       ++i) {
    if (i->second == window)
      touch_id = i->first;
  }

  if (touch_id_target_for_gestures_.count(touch_id))
    touch_id_target_for_gestures_.erase(touch_id);
}

// GestureRecognizer, static
GestureRecognizer* GestureRecognizer::Create() {
  return new GestureRecognizerAura();
}

}  // namespace aura
