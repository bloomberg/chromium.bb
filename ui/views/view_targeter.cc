// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_targeter.h"

#include "ui/events/event_target.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {

ViewTargeter::ViewTargeter(ViewTargeterDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

ViewTargeter::~ViewTargeter() {}

bool ViewTargeter::DoesIntersectRect(const View* target,
                                     const gfx::Rect& rect) const {
  return delegate_->DoesIntersectRect(target, rect);
}

View* ViewTargeter::TargetForRect(View* root, const gfx::Rect& rect) const {
  return delegate_->TargetForRect(root, rect);
}

ui::EventTarget* ViewTargeter::FindTargetForEvent(ui::EventTarget* root,
                                                  ui::Event* event) {
  View* view = static_cast<View*>(root);

  if (event->IsKeyEvent())
    return FindTargetForKeyEvent(view, *static_cast<ui::KeyEvent*>(event));

  if (event->IsScrollEvent()) {
    return FindTargetForScrollEvent(view,
                                    *static_cast<ui::ScrollEvent*>(event));
  }

  NOTREACHED() << "ViewTargeter does not yet support this event type.";
  return NULL;
}

ui::EventTarget* ViewTargeter::FindNextBestTarget(
    ui::EventTarget* previous_target,
    ui::Event* event) {
  return previous_target->GetParentTarget();
}

bool ViewTargeter::SubtreeCanAcceptEvent(
    ui::EventTarget* target,
    const ui::LocatedEvent& event) const {
  NOTREACHED();
  return false;
}

bool ViewTargeter::EventLocationInsideBounds(
    ui::EventTarget* target,
    const ui::LocatedEvent& event) const {
  NOTREACHED();
  return false;
}

View* ViewTargeter::FindTargetForKeyEvent(View* root, const ui::KeyEvent& key) {
  if (root->GetFocusManager())
    return root->GetFocusManager()->GetFocusedView();
  return NULL;
}

View* ViewTargeter::FindTargetForScrollEvent(View* root,
                                             const ui::ScrollEvent& scroll) {
  gfx::Rect rect(scroll.location(), gfx::Size(1, 1));
  return root->GetEffectiveViewTargeter()->TargetForRect(root, rect);
}

}  // namespace views
