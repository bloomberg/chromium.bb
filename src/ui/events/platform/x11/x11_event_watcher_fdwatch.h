// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_X11_X11_EVENT_WATCHER_FDWATCH_H_
#define UI_EVENTS_PLATFORM_X11_X11_EVENT_WATCHER_FDWATCH_H_

#include "base/macros.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "ui/events/platform/x11/x11_event_source.h"

namespace ui {

// X11EventWatcher implementation which uses MessagePumpForUI::FdWatcher
// API to be notified about incoming XEvents.
class X11EventWatcherFdWatch : public X11EventWatcher,
                               public base::MessagePumpForUI::FdWatcher {
 public:
  explicit X11EventWatcherFdWatch(X11EventSource* source);
  ~X11EventWatcherFdWatch() override;

  // X11EventWatcher:
  void StartWatching() override;
  void StopWatching() override;

 private:
  // base::MessagePumpForUI::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  X11EventSource* event_source_;

  base::MessagePumpForUI::FdWatchController watcher_controller_;
  bool started_ = false;

  DISALLOW_COPY_AND_ASSIGN(X11EventWatcherFdWatch);
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_X11_X11_EVENT_WATCHER_FDWATCH_H_
