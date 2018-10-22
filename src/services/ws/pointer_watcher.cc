// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/pointer_watcher.h"

#include "services/ws/window_service.h"
#include "services/ws/window_tree.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ws {

// static
std::unique_ptr<ui::Event> PointerWatcher::CreateEventForClient(
    const ui::Event& event) {
  // Client code expects to get PointerEvents.
  if (event.IsMouseEvent())
    return std::make_unique<ui::PointerEvent>(*event.AsMouseEvent());
  if (event.IsTouchEvent())
    return std::make_unique<ui::PointerEvent>(*event.AsTouchEvent());
  return ui::Event::Clone(event);
}

PointerWatcher::PointerWatcher(WindowTree* tree) : tree_(tree) {
  tree->window_service()->env()->AddWindowEventDispatcherObserver(this);
}

PointerWatcher::~PointerWatcher() {
  tree_->window_service()->env()->RemoveWindowEventDispatcherObserver(this);
}

bool PointerWatcher::DoesEventMatch(const ui::Event& event) const {
  switch (event.type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_TOUCH_RELEASED:
      return true;

    case ui::ET_MOUSE_MOVED:
    case ui::ET_TOUCH_MOVED:
    case ui::ET_MOUSEWHEEL:
      return types_to_watch_ == TypesToWatch::kUpDownMoveWheel;

    default:
      break;
  }
  return false;
}

void PointerWatcher::ClearPendingEvent() {
  pending_event_.reset();
}

void PointerWatcher::OnWindowEventDispatcherStartedProcessing(
    aura::WindowEventDispatcher* dispatcher,
    const ui::Event& event) {
  if (!DoesEventMatch(event))
    return;

  // See comment above |pending_event_| for details on why the event isn't sent
  // immediately.
  pending_event_ = CreateEventForClient(event);
}

void PointerWatcher::OnWindowEventDispatcherFinishedProcessingEvent(
    aura::WindowEventDispatcher* dispatcher) {
  if (!pending_event_)
    return;

  tree_->SendPointerWatcherEventToClient(dispatcher->host()->GetDisplayId(),
                                         std::move(pending_event_));
}

}  // namespace ws
