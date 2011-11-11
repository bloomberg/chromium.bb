// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_ACTIVE_WINDOW_WATCHER_X_H_
#define UI_BASE_X_ACTIVE_WINDOW_WATCHER_X_H_
#pragma once

#include <gdk/gdk.h>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/ui_export.h"

namespace ui {

// This is a helper class that is used to keep track of which window the X
// window manager thinks is active. Add an Observer to listener for changes to
// the active window.
class UI_EXPORT ActiveWindowWatcherX {
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

  // Checks if the WM supports the active window property. Note that the return
  // value can change, especially during system startup.
  static bool WMSupportsActivation();

 private:
  friend struct DefaultSingletonTraits<ActiveWindowWatcherX>;

  ActiveWindowWatcherX();
  ~ActiveWindowWatcherX();

  void Init();

  // Sends a notification out through the NotificationService that the active
  // window has changed.
  void NotifyActiveWindowChanged();

  // Callback for PropertyChange XEvents.
  CHROMEG_CALLBACK_1(ActiveWindowWatcherX, GdkFilterReturn,
                     OnWindowXEvent, GdkXEvent*, GdkEvent*);

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ActiveWindowWatcherX);
};

}  // namespace ui

#endif  // UI_BASE_X_ACTIVE_WINDOW_WATCHER_X_H_
