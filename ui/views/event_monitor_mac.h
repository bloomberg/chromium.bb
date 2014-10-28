// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENT_MONITOR_MAC_H_
#define UI_VIEWS_EVENT_MONITOR_MAC_H_

#include "base/macros.h"
#include "ui/views/event_monitor.h"

#import <Cocoa/Cocoa.h>

namespace views {

class EventMonitorMac : public EventMonitor {
 public:
  explicit EventMonitorMac(ui::EventHandler* event_handler);
  virtual ~EventMonitorMac();

 private:
  id monitor_;

  DISALLOW_COPY_AND_ASSIGN(EventMonitorMac);
};

}  // namespace views

#endif  // UI_VIEWS_EVENT_MONITOR_MAC_H_
