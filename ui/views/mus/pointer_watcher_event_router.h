// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_POINTER_WATCHER_EVENT_ROUTER_H_
#define UI_VIEWS_MUS_POINTER_WATCHER_EVENT_ROUTER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "services/ui/public/cpp/window_tree_client_observer.h"
#include "ui/views/mus/mus_export.h"

namespace ui {
class PointerEvent;
class WindowTreeClient;
}

namespace views {

class PointerWatcher;

// PointerWatcherEventRouter is responsible for maintaining the list of
// PointerWatchers and notifying appropriately. It is expected the owner of
// PointerWatcherEventRouter is a WindowTreeClientDelegate and calls
// OnPointerEventObserved().
class VIEWS_MUS_EXPORT PointerWatcherEventRouter
    : public NON_EXPORTED_BASE(ui::WindowTreeClientObserver) {
 public:
  explicit PointerWatcherEventRouter(ui::WindowTreeClient* client);
  ~PointerWatcherEventRouter() override;

  // Called by WindowTreeClientDelegate to notify PointerWatchers appropriately.
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              ui::Window* target);

  void AddPointerWatcher(PointerWatcher* watcher, bool want_moves);
  void RemovePointerWatcher(PointerWatcher* watcher);

 private:
  // Returns true if there is one or more watchers for this client.
  bool HasPointerWatcher();

  // ui::WindowTreeClientObserver:
  void OnWindowTreeCaptureChanged(ui::Window* gained_capture,
                                  ui::Window* lost_capture) override;
  void OnWillDestroyClient(ui::WindowTreeClient* client) override;

  ui::WindowTreeClient* window_tree_client_;
  base::ObserverList<PointerWatcher, true> pointer_watchers_;
  bool pointer_watcher_want_moves_ = false;

  DISALLOW_COPY_AND_ASSIGN(PointerWatcherEventRouter);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_POINTER_WATCHER_EVENT_ROUTER_H_
