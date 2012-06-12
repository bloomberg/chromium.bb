// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MONITOR_CHANGE_OBSERVER_X11_H
#define UI_AURA_MONITOR_CHANGE_OBSERVER_X11_H
#pragma once

#include <X11/Xlib.h>

// Xlib.h defines RootWindow.
#undef RootWindow

#include "base/basictypes.h"
#include "base/message_loop.h"

namespace aura {
namespace internal {

// An object that observes changes in monitor configuration and
// update MonitorManagers.
class MonitorChangeObserverX11 : public MessageLoop::Dispatcher {
 public:
  MonitorChangeObserverX11();
  virtual ~MonitorChangeObserverX11();

  // Overridden from Dispatcher overrides:
  virtual bool Dispatch(const base::NativeEvent& xev) OVERRIDE;

  // Reads display configurations from the system and notifies
  // |monitor_manager_| about the change.
  void NotifyDisplayChange();

 private:
  Display* xdisplay_;

  ::Window x_root_window_;

  int xrandr_event_base_;

  DISALLOW_COPY_AND_ASSIGN(MonitorChangeObserverX11);
};

}  // namespace internal
}  // namespace aura

#endif  // UI_AURA_MONITOR_CHANGE_OBSERVER_X11_H
