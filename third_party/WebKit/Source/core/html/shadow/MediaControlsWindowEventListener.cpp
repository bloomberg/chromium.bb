// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/shadow/MediaControlsWindowEventListener.h"

#include "core/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/html/media/MediaControls.h"
#include "core/html/shadow/MediaControlElements.h"

namespace blink {

namespace {

// Helper returning the top DOMWindow as a LocalDOMWindow. Returns nullptr if it
// is not a LocalDOMWindow.
// This does not work with OOPIF.
LocalDOMWindow* GetTopLocalDOMWindow(LocalDOMWindow* window) {
  if (!window->top() || !window->top()->IsLocalDOMWindow())
    return nullptr;
  return static_cast<LocalDOMWindow*>(window->top());
}

}  // anonymous namespace

MediaControlsWindowEventListener* MediaControlsWindowEventListener::Create(
    MediaControls* media_controls,
    std::unique_ptr<Callback> callback) {
  return new MediaControlsWindowEventListener(media_controls,
                                              std::move(callback));
}

MediaControlsWindowEventListener::MediaControlsWindowEventListener(
    MediaControls* media_controls,
    std::unique_ptr<Callback> callback)
    : EventListener(kCPPEventListenerType),
      media_controls_(media_controls),
      callback_(std::move(callback)),
      is_active_(false) {
  DCHECK(callback_);
}

bool MediaControlsWindowEventListener::operator==(
    const EventListener& other) const {
  return this == &other;
}

void MediaControlsWindowEventListener::Start() {
  if (is_active_)
    return;

  if (LocalDOMWindow* window = media_controls_->OwnerDocument().domWindow()) {
    window->addEventListener(EventTypeNames::click, this, true);

    if (LocalDOMWindow* outer_window = GetTopLocalDOMWindow(window)) {
      if (window != outer_window)
        outer_window->addEventListener(EventTypeNames::click, this, true);
      outer_window->addEventListener(EventTypeNames::resize, this, true);
    }
  }

  media_controls_->PanelElement()->addEventListener(EventTypeNames::click, this,
                                                    false);
  media_controls_->TimelineElement()->addEventListener(EventTypeNames::click,
                                                       this, false);
  media_controls_->CastButtonElement()->addEventListener(EventTypeNames::click,
                                                         this, false);
  media_controls_->VolumeSliderElement()->addEventListener(
      EventTypeNames::click, this, false);

  is_active_ = true;
}

void MediaControlsWindowEventListener::Stop() {
  if (!is_active_)
    return;

  if (LocalDOMWindow* window = media_controls_->OwnerDocument().domWindow()) {
    window->removeEventListener(EventTypeNames::click, this, true);

    if (LocalDOMWindow* outer_window = GetTopLocalDOMWindow(window)) {
      if (window != outer_window)
        outer_window->removeEventListener(EventTypeNames::click, this, true);
      outer_window->removeEventListener(EventTypeNames::resize, this, true);
    }

    is_active_ = false;
  }

  media_controls_->PanelElement()->removeEventListener(EventTypeNames::click,
                                                       this, false);
  media_controls_->TimelineElement()->removeEventListener(EventTypeNames::click,
                                                          this, false);
  media_controls_->CastButtonElement()->removeEventListener(
      EventTypeNames::click, this, false);
  media_controls_->VolumeSliderElement()->removeEventListener(
      EventTypeNames::click, this, false);

  is_active_ = false;
}

void MediaControlsWindowEventListener::handleEvent(
    ExecutionContext* execution_context,
    Event* event) {
  DCHECK(event->type() == EventTypeNames::click ||
         event->type() == EventTypeNames::resize);

  if (!is_active_)
    return;
  (*callback_.get())();
}

DEFINE_TRACE(MediaControlsWindowEventListener) {
  EventListener::Trace(visitor);
  visitor->Trace(media_controls_);
}

}  // namespace blink
