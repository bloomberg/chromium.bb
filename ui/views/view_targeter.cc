// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_targeter.h"

#include "ui/events/event_target.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace views {

ViewTargeter::ViewTargeter() {}
ViewTargeter::~ViewTargeter() {}

ui::EventTarget* ViewTargeter::FindTargetForEvent(ui::EventTarget* root,
                                                  ui::Event* event) {
  View* view = static_cast<View*>(root);
  if (event->IsKeyEvent())
    return FindTargetForKeyEvent(view, *static_cast<ui::KeyEvent*>(event));

  NOTREACHED() << "ViewTargeter does not yet support this event type.";
  return NULL;
}

ui::EventTarget* ViewTargeter::FindNextBestTarget(
    ui::EventTarget* previous_target,
    ui::Event* event) {
  return previous_target->GetParentTarget();
}

View* ViewTargeter::FindTargetForKeyEvent(View* view, const ui::KeyEvent& key) {
  if (view->GetFocusManager())
    return view->GetFocusManager()->GetFocusedView();
  return NULL;
}

}  // namespace aura
