// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/event_monitor_aura.h"

#include "base/logging.h"
#include "ui/aura/env.h"

namespace views {

// static
EventMonitor* EventMonitor::Create(ui::EventHandler* event_handler) {
  return new EventMonitorAura(event_handler);
}

// static
gfx::Point EventMonitor::GetLastMouseLocation() {
  return aura::Env::GetInstance()->last_mouse_location();
}

EventMonitorAura::EventMonitorAura(ui::EventHandler* event_handler)
    : event_handler_(event_handler) {
  DCHECK(event_handler_);
  aura::Env::GetInstance()->AddPreTargetHandler(event_handler_);
}

EventMonitorAura::~EventMonitorAura() {
  aura::Env::GetInstance()->RemovePreTargetHandler(event_handler_);
}

}  // namespace views
