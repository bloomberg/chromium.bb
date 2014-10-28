// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/event_monitor_mac.h"

#include "base/logging.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"

namespace views {

// static
EventMonitor* EventMonitor::Create(ui::EventHandler* event_handler) {
  return new EventMonitorMac(event_handler);
}

// static
gfx::Point EventMonitor::GetLastMouseLocation() {
  NSPoint mouseLocation = [NSEvent mouseLocation];
  // Flip coordinates to gfx (0,0 in top-left corner) using primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  mouseLocation.y = NSMaxY([screen frame]) - mouseLocation.y;
  return gfx::Point(mouseLocation.x, mouseLocation.y);
}

EventMonitorMac::EventMonitorMac(ui::EventHandler* event_handler) {
  DCHECK(event_handler);
  monitor_ = [NSEvent addLocalMonitorForEventsMatchingMask:NSAnyEventMask
      handler:^NSEvent*(NSEvent* event){
                  scoped_ptr<ui::Event> ui_event = ui::EventFromNative(event);
                  event_handler->OnEvent(ui_event.get());
                  return event;
              }];
}

EventMonitorMac::~EventMonitorMac() {
  [NSEvent removeMonitor:monitor_];
}

}  // namespace views
