// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/button_ink_drop_delegate.h"

#include "ui/events/event.h"
#include "ui/events/scoped_target_handler.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_factory.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/view.h"

namespace views {

ButtonInkDropDelegate::ButtonInkDropDelegate(InkDropHost* ink_drop_host,
                                             View* view)
    : target_handler_(new ui::ScopedTargetHandler(view, this)),
      ink_drop_host_(ink_drop_host),
      ink_drop_(InkDropFactory::CreateInkDrop(ink_drop_host_)) {}

ButtonInkDropDelegate::~ButtonInkDropDelegate() {
}

void ButtonInkDropDelegate::OnAction(InkDropState state) {
  ink_drop_->AnimateToState(state);
}

void ButtonInkDropDelegate::SnapToActivated() {
  ink_drop_->SnapToActivated();
}

void ButtonInkDropDelegate::SetHovered(bool is_hovered) {
  ink_drop_->SetHovered(is_hovered);
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler:

void ButtonInkDropDelegate::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
      SetHovered(true);
      break;
    case ui::ET_MOUSE_EXITED:
      SetHovered(false);
      break;
    default:
      return;
  }
}

void ButtonInkDropDelegate::OnGestureEvent(ui::GestureEvent* event) {
  InkDropState current_ink_drop_state = ink_drop_->GetTargetInkDropState();

  InkDropState ink_drop_state = InkDropState::HIDDEN;
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      ink_drop_state = InkDropState::ACTION_PENDING;
      // The ui::ET_GESTURE_TAP_DOWN event needs to be marked as handled so that
      // subsequent events for the gesture are sent to |this|.
      event->SetHandled();
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      ink_drop_state = InkDropState::ALTERNATE_ACTION_PENDING;
      break;
    case ui::ET_GESTURE_LONG_TAP:
      ink_drop_state = InkDropState::ALTERNATE_ACTION_TRIGGERED;
      break;
    case ui::ET_GESTURE_END:
      if (current_ink_drop_state == InkDropState::ACTIVATED)
        return;
    // Fall through to ui::ET_GESTURE_SCROLL_BEGIN case.
    case ui::ET_GESTURE_SCROLL_BEGIN:
      ink_drop_state = InkDropState::HIDDEN;
      break;
    default:
      return;
  }

  last_ink_drop_location_ = event->location();

  if (ink_drop_state == InkDropState::HIDDEN &&
      (current_ink_drop_state == InkDropState::ACTION_TRIGGERED ||
       current_ink_drop_state == InkDropState::ALTERNATE_ACTION_TRIGGERED ||
       current_ink_drop_state == InkDropState::DEACTIVATED)) {
    // These InkDropStates automatically transition to the HIDDEN state so we
    // don't make an explicit call. Explicitly animating to HIDDEN in this case
    // would prematurely pre-empt these animations.
    return;
  }
  ink_drop_->AnimateToState(ink_drop_state);
}

}  // namespace views
