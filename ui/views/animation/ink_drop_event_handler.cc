// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_event_handler.h"

#include <memory>

#include "ui/events/scoped_target_handler.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/view.h"

namespace views {
InkDropEventHandler::InkDropEventHandler(View* host_view, Delegate* delegate)
    : target_handler_(
          std::make_unique<ui::ScopedTargetHandler>(host_view, this)),
      host_view_(host_view),
      delegate_(delegate) {}

InkDropEventHandler::~InkDropEventHandler() = default;

void InkDropEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  if (!host_view_->enabled() || !delegate_->SupportsGestureEvents())
    return;

  InkDropState current_ink_drop_state =
      delegate_->GetInkDrop()->GetTargetInkDropState();

  InkDropState ink_drop_state = InkDropState::HIDDEN;
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      if (current_ink_drop_state == InkDropState::ACTIVATED)
        return;
      ink_drop_state = InkDropState::ACTION_PENDING;
      // The ui::ET_GESTURE_TAP_DOWN event needs to be marked as handled so
      // that subsequent events for the gesture are sent to |this|.
      event->SetHandled();
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      if (current_ink_drop_state == InkDropState::ACTIVATED)
        return;
      ink_drop_state = InkDropState::ALTERNATE_ACTION_PENDING;
      break;
    case ui::ET_GESTURE_LONG_TAP:
      ink_drop_state = InkDropState::ALTERNATE_ACTION_TRIGGERED;
      break;
    case ui::ET_GESTURE_END:
    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_TAP_CANCEL:
      if (current_ink_drop_state == InkDropState::ACTIVATED)
        return;
      ink_drop_state = InkDropState::HIDDEN;
      break;
    default:
      return;
  }

  if (ink_drop_state == InkDropState::HIDDEN &&
      (current_ink_drop_state == InkDropState::ACTION_TRIGGERED ||
       current_ink_drop_state == InkDropState::ALTERNATE_ACTION_TRIGGERED ||
       current_ink_drop_state == InkDropState::DEACTIVATED ||
       current_ink_drop_state == InkDropState::HIDDEN)) {
    // These InkDropStates automatically transition to the HIDDEN state so we
    // don't make an explicit call. Explicitly animating to HIDDEN in this
    // case would prematurely pre-empt these animations.
    return;
  }
  delegate_->AnimateInkDrop(ink_drop_state, event);
}

void InkDropEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
      delegate_->GetInkDrop()->SetHovered(true);
      break;
    case ui::ET_MOUSE_EXITED:
      delegate_->GetInkDrop()->SetHovered(false);
      break;
    case ui::ET_MOUSE_DRAGGED:
      delegate_->GetInkDrop()->SetHovered(
          host_view_->GetLocalBounds().Contains(event->location()));
      break;
    default:
      break;
  }
}

}  // namespace views
