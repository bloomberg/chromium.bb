// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/gestures/gesture_recognizer_aura.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "ui/aura/event.h"
#include "ui/aura/gestures/gesture_sequence.h"
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

GestureRecognizerAura::GestureRecognizerAura(RootWindow* root_window)
    : default_sequence_(CreateSequence(root_window)),
      root_window_(root_window) {
}

GestureRecognizerAura::~GestureRecognizerAura() {
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerAura, protected:

GestureSequence* GestureRecognizerAura::CreateSequence(
    RootWindow* root_window) {
  return new GestureSequence(root_window);
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerAura, private:

GestureSequence::Gestures* GestureRecognizerAura::ProcessTouchEventForGesture(
    const TouchEvent& event,
    ui::TouchStatus status) {
  return default_sequence_->ProcessTouchEventForGesture(event, status);
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
  if (!event_queue_[window]) {
    LOG(ERROR) << "Trying to advance an empty gesture queue for " << window;
    return NULL;
  }

  ScopedPop pop(event_queue_[window]);
  TouchEvent* event = event_queue_[window]->front();

  GestureSequence* sequence = window_sequence_[window];
  if (!sequence) {
    sequence = new GestureSequence(root_window_);
    window_sequence_[window] = sequence;
  }

  return sequence->ProcessTouchEventForGesture(*event,
      processed ? ui::TOUCH_STATUS_CONTINUE : ui::TOUCH_STATUS_UNKNOWN);
}

void GestureRecognizerAura::FlushTouchQueue(Window* window) {
  if (window_sequence_[window]) {
    delete window_sequence_[window];
    window_sequence_.erase(window);
  }

  if (event_queue_[window]) {
    delete event_queue_[window];
    event_queue_.erase(window);
  }
}

// GestureRecognizer, static
GestureRecognizer* GestureRecognizer::Create(RootWindow* root_window) {
  return new GestureRecognizerAura(root_window);
}

}  // namespace aura
