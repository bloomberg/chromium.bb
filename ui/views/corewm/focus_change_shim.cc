// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/focus_change_shim.h"

#include "ui/aura/window.h"
#include "ui/base/events/event_target.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/corewm/focus_change_event.h"

namespace views {
namespace corewm {

FocusChangeShim::FocusChangeShim(ui::EventTarget* target)
    : target_(target) {
  if (views::corewm::UseFocusController() && target_)
    target_->AddPreTargetHandler(this);
}

FocusChangeShim::~FocusChangeShim() {
  if (views::corewm::UseFocusController() && target_)
    target_->RemovePreTargetHandler(this);
}

void FocusChangeShim::OnWindowFocused(aura::Window* window) {
}

void FocusChangeShim::OnEvent(ui::Event* event) {
  if (event->type() == FocusChangeEvent::focus_changed_event_type()) {
    DCHECK(views::corewm::UseFocusController());
    OnWindowFocused(static_cast<aura::Window*>(event->target()));
  }
  EventHandler::OnEvent(event);
}

}  // namespace corewm
}  // namespace views
