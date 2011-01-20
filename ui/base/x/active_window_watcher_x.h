// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_ACTIVE_WINDOW_WATCHER_X_H_
#define UI_BASE_X_ACTIVE_WINDOW_WATCHER_X_H_
#pragma once

#include <gdk/gdk.h>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/singleton.h"

namespace ui {

// This is a helper class that is used to keep track of which window the X
// window manager thinks is active. Add an Observer to listener for changes to
// the active window.
class ActiveWindowWatcherX {
 public:
  class Observer {
   public:
    // |active_window| will be NULL if the active window isn't one of Chrome's.
    virtual void ActiveWindowChanged(GdkWindow* active_window) = 0;

   protected:
    virtual ~Observer() {}
  };

  static ActiveWindowWatcherX* GetInstance();

  static void AddObserver(Observer* observer);
  static void RemoveObserver(Observer* observer);

 private:
  friend struct DefaultSingletonTraits<ActiveWindowWatcherX>;

  ActiveWindowWatcherX();
  ~ActiveWindowWatcherX();

  void Init();

  // Sends a notification out through the NotificationService that the active
  // window has changed.
  void NotifyActiveWindowChanged();

  // Callback for PropertyChange XEvents.
  static GdkFilterReturn OnWindowXEvent(GdkXEvent* xevent,
                                        GdkEvent* event,
                                        gpointer window_watcher);

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ActiveWindowWatcherX);
};

}  // namespace ui

#endif  // UI_BASE_X_ACTIVE_WINDOW_WATCHER_X_H_
