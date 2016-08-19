// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/pointer_watcher_event_router.h"

#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/pointer_watcher.h"

namespace views {
namespace {

bool HasPointerWatcher(
    base::ObserverList<views::PointerWatcher, true>* observer_list) {
  if (!observer_list->might_have_observers())
    return false;

  // might_have_observers() returned true, see if there really are any
  // observers. The only way to truly know is to use an Iterator and see if it
  // has at least one observer.
  base::ObserverList<PointerWatcher>::Iterator iterator(observer_list);
  return !!iterator.GetNext();
}

}  // namespace

PointerWatcherEventRouter::PointerWatcherEventRouter(
    ui::WindowTreeClient* client)
    : window_tree_client_(client) {
  client->AddObserver(this);
}

PointerWatcherEventRouter::~PointerWatcherEventRouter() {
  if (window_tree_client_)
    window_tree_client_->RemoveObserver(this);
}

void PointerWatcherEventRouter::AddPointerWatcher(PointerWatcher* watcher,
                                                  bool wants_moves) {
  // Pointer watchers cannot be added multiple times.
  DCHECK(!move_watchers_.HasObserver(watcher));
  DCHECK(!non_move_watchers_.HasObserver(watcher));
  if (wants_moves) {
    move_watchers_.AddObserver(watcher);
    if (event_types_ != EventTypes::MOVE_EVENTS) {
      event_types_ = EventTypes::MOVE_EVENTS;
      const bool wants_moves = true;
      window_tree_client_->StartPointerWatcher(wants_moves);
    }
  } else {
    non_move_watchers_.AddObserver(watcher);
    if (event_types_ == EventTypes::NONE) {
      event_types_ = EventTypes::NON_MOVE_EVENTS;
      const bool wants_moves = false;
      window_tree_client_->StartPointerWatcher(wants_moves);
    }
  }
}

void PointerWatcherEventRouter::RemovePointerWatcher(PointerWatcher* watcher) {
  if (non_move_watchers_.HasObserver(watcher)) {
    non_move_watchers_.RemoveObserver(watcher);
  } else {
    DCHECK(move_watchers_.HasObserver(watcher));
    move_watchers_.RemoveObserver(watcher);
  }
  const EventTypes types = DetermineEventTypes();
  if (types == event_types_)
    return;

  event_types_ = types;
  switch (types) {
    case EventTypes::NONE:
      window_tree_client_->StopPointerWatcher();
      break;
    case EventTypes::NON_MOVE_EVENTS:
      window_tree_client_->StartPointerWatcher(false);
      break;
    case EventTypes::MOVE_EVENTS:
      // It isn't possible to remove an observer and transition to wanting move
      // events. This could only happen if there is a bug in the add logic.
      NOTREACHED();
      break;
  }
}

void PointerWatcherEventRouter::OnPointerEventObserved(
    const ui::PointerEvent& event,
    ui::Window* target) {
  Widget* target_widget = nullptr;
  if (target) {
    ui::Window* window = target;
    while (window && !target_widget) {
      target_widget = NativeWidgetMus::GetWidgetForWindow(target);
      window = window->parent();
    }
  }

  // The mojo input events type converter uses the event root_location field
  // to store screen coordinates. Screen coordinates really should be returned
  // separately. See http://crbug.com/608547
  gfx::Point location_in_screen = event.AsLocatedEvent()->root_location();
  FOR_EACH_OBSERVER(
      PointerWatcher, move_watchers_,
      OnPointerEventObserved(event, location_in_screen, target_widget));
  if (event.type() != ui::ET_POINTER_MOVED) {
    FOR_EACH_OBSERVER(
        PointerWatcher, non_move_watchers_,
        OnPointerEventObserved(event, location_in_screen, target_widget));
  }
}

PointerWatcherEventRouter::EventTypes
PointerWatcherEventRouter::DetermineEventTypes() {
  if (HasPointerWatcher(&move_watchers_))
    return EventTypes::MOVE_EVENTS;

  if (HasPointerWatcher(&non_move_watchers_))
    return EventTypes::NON_MOVE_EVENTS;

  return EventTypes::NONE;
}

void PointerWatcherEventRouter::OnWindowTreeCaptureChanged(
    ui::Window* gained_capture,
    ui::Window* lost_capture) {
  FOR_EACH_OBSERVER(PointerWatcher, move_watchers_, OnMouseCaptureChanged());
  FOR_EACH_OBSERVER(PointerWatcher, non_move_watchers_,
                    OnMouseCaptureChanged());
}

void PointerWatcherEventRouter::OnDidDestroyClient(
    ui::WindowTreeClient* client) {
  // We expect that all observers have been removed by this time.
  DCHECK_EQ(event_types_, EventTypes::NONE);
  DCHECK_EQ(client, window_tree_client_);
  window_tree_client_->RemoveObserver(this);
  window_tree_client_ = nullptr;
}

}  // namespace views
