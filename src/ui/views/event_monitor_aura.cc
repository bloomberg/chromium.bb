// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/event_monitor_aura.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/events/event_target.h"

namespace views {

// static
std::unique_ptr<EventMonitor> EventMonitor::CreateApplicationMonitor(
    ui::EventHandler* event_handler,
    gfx::NativeWindow context) {
  aura::Env* env = context->env();
  return std::make_unique<EventMonitorAura>(env, event_handler, env);
}

// static
std::unique_ptr<EventMonitor> EventMonitor::CreateWindowMonitor(
    ui::EventHandler* event_handler,
    gfx::NativeWindow target_window) {
  return std::make_unique<EventMonitorAura>(target_window->env(), event_handler,
                                            target_window);
}

EventMonitorAura::EventMonitorAura(aura::Env* env,
                                   ui::EventHandler* event_handler,
                                   ui::EventTarget* event_target)
    : env_(env), event_handler_(event_handler), event_target_(event_target) {
  DCHECK(env_);
  DCHECK(event_handler_);
  DCHECK(event_target_);
  event_target_->AddPreTargetHandler(event_handler_);
}

EventMonitorAura::~EventMonitorAura() {
  event_target_->RemovePreTargetHandler(event_handler_);
}

gfx::Point EventMonitorAura::GetLastMouseLocation() {
  return env_->last_mouse_location();
}

}  // namespace views
