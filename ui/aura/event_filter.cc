// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event_filter.h"

#include "ui/aura/event.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/window.h"

namespace aura {

EventFilter::EventFilter(Window* owner) : owner_(owner) {
}

EventFilter::~EventFilter() {
}

bool EventFilter::OnMouseEvent(Window* target, MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    // TODO(beng): some windows (e.g. disabled ones, tooltips, etc) may not be
    //             focusable.
    target->GetFocusManager()->SetFocusedWindow(target);
  }
  return false;
}

}  // namespace aura
