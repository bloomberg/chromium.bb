// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/activation_change_shim.h"

#include "ui/aura/window.h"
#include "ui/base/events/event_target.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/corewm/focus_change_event.h"

namespace views {
namespace corewm {

ActivationChangeShim::ActivationChangeShim(ui::EventTarget* target)
    : target_(target) {
  if (UseFocusController() && target_)
    target_->AddPreTargetHandler(this);
}

ActivationChangeShim::~ActivationChangeShim() {
  if (UseFocusController() && target_)
    target_->RemovePreTargetHandler(this);
}

void ActivationChangeShim::OnWindowActivated(aura::Window* active,
                                             aura::Window* old_active) {
}

void ActivationChangeShim::OnEvent(ui::Event* event) {
  if (event->type() == FocusChangeEvent::activation_changed_event_type()) {
    DCHECK(UseFocusController());
    FocusChangeEvent* fce = static_cast<FocusChangeEvent*>(event);
    OnWindowActivated(static_cast<aura::Window*>(event->target()),
                      static_cast<aura::Window*>(fce->last_focus()));
  }
  EventHandler::OnEvent(event);
}

}  // namespace corewm
}  // namespace views
