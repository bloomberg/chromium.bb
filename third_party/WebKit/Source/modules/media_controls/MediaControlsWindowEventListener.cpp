// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/MediaControlsWindowEventListener.h"

#include "core/dom/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/elements/MediaControlCastButtonElement.h"
#include "modules/media_controls/elements/MediaControlPanelElement.h"
#include "modules/media_controls/elements/MediaControlTimelineElement.h"
#include "modules/media_controls/elements/MediaControlVolumeSliderElement.h"

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
    MediaControlsImpl* media_controls,
    Callback callback) {
  return new MediaControlsWindowEventListener(media_controls,
                                              std::move(callback));
}

MediaControlsWindowEventListener::MediaControlsWindowEventListener(
    MediaControlsImpl* media_controls,
    Callback callback)
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

  if (LocalDOMWindow* window = media_controls_->GetDocument().domWindow()) {
    window->addEventListener(EventTypeNames::click, this, true);
    window->addEventListener(EventTypeNames::scroll, this, true);

    if (LocalDOMWindow* outer_window = GetTopLocalDOMWindow(window)) {
      if (window != outer_window) {
        outer_window->addEventListener(EventTypeNames::click, this, true);
        outer_window->addEventListener(EventTypeNames::scroll, this, true);
      }
      outer_window->addEventListener(EventTypeNames::resize, this, true);
    }
  }

  // TODO(mlamouri): use more scalable solution to handle click as this could be
  // broken when new elements that swallow the click are added to the controls.
  media_controls_->panel_->addEventListener(EventTypeNames::click, this, false);
  media_controls_->timeline_->addEventListener(EventTypeNames::click, this,
                                               false);
  media_controls_->cast_button_->addEventListener(EventTypeNames::click, this,
                                                  false);
  media_controls_->volume_slider_->addEventListener(EventTypeNames::click, this,
                                                    false);

  is_active_ = true;
}

void MediaControlsWindowEventListener::Stop() {
  if (!is_active_)
    return;

  if (LocalDOMWindow* window = media_controls_->GetDocument().domWindow()) {
    window->removeEventListener(EventTypeNames::click, this, true);
    window->removeEventListener(EventTypeNames::scroll, this, true);

    if (LocalDOMWindow* outer_window = GetTopLocalDOMWindow(window)) {
      if (window != outer_window) {
        outer_window->removeEventListener(EventTypeNames::click, this, true);
        outer_window->removeEventListener(EventTypeNames::scroll, this, true);
      }
      outer_window->removeEventListener(EventTypeNames::resize, this, true);
    }

    is_active_ = false;
  }

  media_controls_->panel_->removeEventListener(EventTypeNames::click, this,
                                               false);
  media_controls_->timeline_->removeEventListener(EventTypeNames::click, this,
                                                  false);
  media_controls_->cast_button_->removeEventListener(EventTypeNames::click,
                                                     this, false);
  media_controls_->volume_slider_->removeEventListener(EventTypeNames::click,
                                                       this, false);

  is_active_ = false;
}

void MediaControlsWindowEventListener::handleEvent(
    ExecutionContext* execution_context,
    Event* event) {
  DCHECK(event->type() == EventTypeNames::click ||
         event->type() == EventTypeNames::scroll ||
         event->type() == EventTypeNames::resize);

  if (!is_active_)
    return;
  callback_();
}

void MediaControlsWindowEventListener::Trace(blink::Visitor* visitor) {
  EventListener::Trace(visitor);
  visitor->Trace(media_controls_);
}

}  // namespace blink
