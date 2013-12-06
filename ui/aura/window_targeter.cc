// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_targeter.h"

#include "ui/aura/client/event_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/event_target.h"

namespace aura {

WindowTargeter::WindowTargeter() {}
WindowTargeter::~WindowTargeter() {}

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
  return window->bounds().Contains(event.location());
}

}  // namespace aura
