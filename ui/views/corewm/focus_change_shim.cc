// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/focus_change_shim.h"

#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/base/events/event_target.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/corewm/focus_change_event.h"

namespace views {
namespace corewm {
namespace {
bool UseFocusController() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFocusController);
}
}

FocusChangeShim::FocusChangeShim(ui::EventTarget* target)
    : target_(target) {
  if (UseFocusController() && target_)
    target_->AddPreTargetHandler(this);
}

FocusChangeShim::~FocusChangeShim() {
  if (UseFocusController() && target_)
    target_->RemovePreTargetHandler(this);
}

void FocusChangeShim::OnWindowFocused(aura::Window* window) {
}

ui::EventResult FocusChangeShim::OnEvent(ui::Event* event) {
  if (event->type() == FocusChangeEvent::focus_changed_event_type()) {
    DCHECK(UseFocusController());
    OnWindowFocused(static_cast<aura::Window*>(event->target()));
  }
  return EventHandler::OnEvent(event);
}

}  // namespace corewm
}  // namespace views
