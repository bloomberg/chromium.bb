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
                                                  bool want_moves) {
  // Pointer watchers cannot be added multiple times.
  DCHECK(!pointer_watchers_.HasObserver(watcher));
  // TODO(jamescook): Support adding pointer watchers with different
  // |want_moves| values by tracking whether the set as a whole wants moves.
  // This will involve sending observed move events to a subset of the
  // watchers. (crbug.com/627146)
  DCHECK(!HasPointerWatcher() || want_moves == pointer_watcher_want_moves_);
  pointer_watcher_want_moves_ = want_moves;

  bool had_watcher = HasPointerWatcher();
  pointer_watchers_.AddObserver(watcher);
  if (!had_watcher) {
    // First PointerWatcher added, start the watcher on the window server.
    window_tree_client_->StartPointerWatcher(want_moves);
  }
}

void PointerWatcherEventRouter::RemovePointerWatcher(PointerWatcher* watcher) {
  pointer_watchers_.RemoveObserver(watcher);
  if (!HasPointerWatcher()) {
    // Last PointerWatcher removed, stop the watcher on the window server.
    window_tree_client_->StopPointerWatcher();
    pointer_watcher_want_moves_ = false;
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
      PointerWatcher, pointer_watchers_,
      OnPointerEventObserved(event, location_in_screen, target_widget));
}

bool PointerWatcherEventRouter::HasPointerWatcher() {
  // Check to see if we really have any observers left. This doesn't use
  // base::ObserverList<>::might_have_observers() because that returns true
  // during iteration over the list even when the last observer is removed.
  base::ObserverList<PointerWatcher>::Iterator iterator(&pointer_watchers_);
  return !!iterator.GetNext();
}

void PointerWatcherEventRouter::OnWindowTreeCaptureChanged(
    ui::Window* gained_capture,
    ui::Window* lost_capture) {
  FOR_EACH_OBSERVER(PointerWatcher, pointer_watchers_, OnMouseCaptureChanged());
}

void PointerWatcherEventRouter::OnWillDestroyClient(
    ui::WindowTreeClient* client) {
  DCHECK_EQ(client, window_tree_client_);
  window_tree_client_->RemoveObserver(this);
  window_tree_client_ = nullptr;
}

}  // namespace views
