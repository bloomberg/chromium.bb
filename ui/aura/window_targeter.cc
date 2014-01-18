// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_targeter.h"

#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/event_target.h"

namespace aura {

WindowTargeter::WindowTargeter() {}
WindowTargeter::~WindowTargeter() {}

bool WindowTargeter::WindowCanAcceptEvent(aura::Window* window,
                                          const ui::LocatedEvent& event) const {
  if (!window->IsVisible())
    return false;
  if (window->ignore_events())
    return false;
  client::EventClient* client = client::GetEventClient(window->GetRootWindow());
  if (client && !client->CanProcessEventsWithinSubtree(window))
    return false;

  Window* parent = window->parent();
  if (parent && parent->delegate_ && !parent->delegate_->
      ShouldDescendIntoChildForEventHandling(window, event.location())) {
    return false;
  }
  return true;
}

bool WindowTargeter::EventLocationInsideBounds(
    aura::Window* window, const ui::LocatedEvent& event) const {
  gfx::RectF bounds = window->bounds();
  if (window->layer())
    window->layer()->transform().TransformRect(&bounds);
  return bounds.Contains(event.location());
}

ui::EventTarget* WindowTargeter::FindTargetForEvent(ui::EventTarget* root,
                                                    ui::Event* event) {
  if (event->IsKeyEvent()) {
    Window* window = static_cast<Window*>(root);
    Window* root_window = window->GetRootWindow();
    const ui::KeyEvent& key = static_cast<const ui::KeyEvent&>(*event);
    if (key.key_code() == ui::VKEY_UNKNOWN)
      return NULL;
    client::EventClient* event_client = client::GetEventClient(root_window);
    client::FocusClient* focus_client = client::GetFocusClient(root_window);
    Window* focused_window = focus_client->GetFocusedWindow();
    if (event_client &&
        !event_client->CanProcessEventsWithinSubtree(focused_window)) {
      focus_client->FocusWindow(NULL);
      return NULL;
    }
    return focused_window ? focused_window : window;
  }
  return EventTargeter::FindTargetForEvent(root, event);
}

bool WindowTargeter::SubtreeShouldBeExploredForEvent(
    ui::EventTarget* root,
    const ui::LocatedEvent& event) {
  Window* window = static_cast<Window*>(root);
  if (!WindowCanAcceptEvent(window, event))
    return false;

  return EventLocationInsideBounds(window, event);
}

ui::EventTarget* WindowTargeter::FindTargetForLocatedEvent(
    ui::EventTarget* root,
    ui::LocatedEvent* event) {
  Window* window = static_cast<Window*>(root);
  if (!window->parent()) {
    Window* target = FindTargetInRootWindow(window, *event);
    if (target) {
      window->ConvertEventToTarget(target, event);
      return target;
    }
  }
  return EventTargeter::FindTargetForLocatedEvent(root, event);
}

Window* WindowTargeter::FindTargetInRootWindow(Window* root_window,
                                               const ui::LocatedEvent& event) {
  DCHECK_EQ(root_window, root_window->GetRootWindow());

  // Mouse events should be dispatched to the window that processed the
  // mouse-press events (if any).
  if (event.IsScrollEvent() || event.IsMouseEvent()) {
    WindowEventDispatcher* dispatcher = root_window->GetDispatcher();
    if (dispatcher->mouse_pressed_handler())
      return dispatcher->mouse_pressed_handler();
  }

  // All events should be directed towards the capture window (if any).
  Window* capture_window = client::GetCaptureWindow(root_window);
  if (capture_window)
    return capture_window;

  if (event.IsTouchEvent()) {
    // Query the gesture-recognizer to find targets for touch events.
    const ui::TouchEvent& touch = static_cast<const ui::TouchEvent&>(event);
    ui::GestureConsumer* consumer =
        ui::GestureRecognizer::Get()->GetTouchLockedTarget(touch);
    if (consumer)
      return static_cast<Window*>(consumer);
    consumer =
        ui::GestureRecognizer::Get()->GetTargetForLocation(
            event.location(), touch.source_device_id());
    if (consumer)
      return static_cast<Window*>(consumer);

    // If the initial touch is outside the root window, target the root.
    if (!root_window->bounds().Contains(event.location()))
      return root_window;
  }

  return NULL;
}

}  // namespace aura
