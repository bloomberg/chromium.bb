// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/event_monitor_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"

namespace views {

// static
scoped_ptr<EventMonitor> EventMonitor::CreateApplicationMonitor(
    ui::EventHandler* event_handler) {
  return make_scoped_ptr(new EventMonitorMac(event_handler, nullptr));
}

// static
scoped_ptr<EventMonitor> EventMonitor::CreateWindowMonitor(
    ui::EventHandler* event_handler,
    gfx::NativeWindow target_window) {
  return make_scoped_ptr(new EventMonitorMac(event_handler, target_window));
}

// static
gfx::Point EventMonitor::GetLastMouseLocation() {
  NSPoint mouseLocation = [NSEvent mouseLocation];
  // Flip coordinates to gfx (0,0 in top-left corner) using primary screen.
  NSScreen* screen = [[NSScreen screens] firstObject];
  mouseLocation.y = NSMaxY([screen frame]) - mouseLocation.y;
  return gfx::Point(mouseLocation.x, mouseLocation.y);
}

EventMonitorMac::EventMonitorMac(ui::EventHandler* event_handler,
                                 gfx::NativeWindow target_window) {
  DCHECK(event_handler);
  monitor_ = [NSEvent addLocalMonitorForEventsMatchingMask:NSAnyEventMask
      handler:^NSEvent*(NSEvent* event) {
          if (!target_window || [event window] == target_window) {
            scoped_ptr<ui::Event> ui_event = ui::EventFromNative(event);
            if (ui_event)
              event_handler->OnEvent(ui_event.get());
          }
          return event;
      }];
}

EventMonitorMac::~EventMonitorMac() {
  [NSEvent removeMonitor:monitor_];
}

}  // namespace views
