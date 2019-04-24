// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/menu_button_event_handler.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/event_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

using base::TimeTicks;
using base::TimeDelta;

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// MenuButtonEventHandler::PressedLock
//
////////////////////////////////////////////////////////////////////////////////

MenuButtonEventHandler::PressedLock::PressedLock(
    MenuButtonEventHandler* menu_button_event_handler)
    : PressedLock(menu_button_event_handler, false, nullptr) {}

MenuButtonEventHandler::PressedLock::PressedLock(
    MenuButtonEventHandler* menu_button_event_handler,
    bool is_sibling_menu_show,
    const ui::LocatedEvent* event)
    : menu_button_event_handler_(
          menu_button_event_handler->weak_factory_.GetWeakPtr()) {
  menu_button_event_handler_->IncrementPressedLocked(is_sibling_menu_show,
                                                     event);
}

std::unique_ptr<MenuButtonEventHandler::PressedLock>
MenuButtonEventHandler::TakeLock() {
  return TakeLock(false, nullptr);
}

std::unique_ptr<MenuButtonEventHandler::PressedLock>
MenuButtonEventHandler::TakeLock(bool is_sibling_menu_show,
                                 const ui::LocatedEvent* event) {
  return std::make_unique<MenuButtonEventHandler::PressedLock>(
      this, is_sibling_menu_show, event);
}

MenuButtonEventHandler::PressedLock::~PressedLock() {
  if (menu_button_event_handler_)
    menu_button_event_handler_->DecrementPressedLocked();
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButtonEventHandler
//
////////////////////////////////////////////////////////////////////////////////

MenuButtonEventHandler::MenuButtonEventHandler(
    MenuButton* menu_button_parent,
    MenuButtonListener* menu_button_listener)
    : menu_button_parent_(menu_button_parent), listener_(menu_button_listener) {
  menu_button_parent_->AddPreTargetHandler(this);
}

MenuButtonEventHandler::~MenuButtonEventHandler() {
  menu_button_parent_->RemovePreTargetHandler(this);
}

bool MenuButtonEventHandler::OnMousePressed(const ui::MouseEvent& event) {
  if (menu_button_parent_->request_focus_on_press())
    menu_button_parent_->RequestFocus();
  if (menu_button_parent_->state() != Button::STATE_DISABLED &&
      menu_button_parent_->HitTestPoint(event.location()) &&
      menu_button_parent_->IsTriggerableEventType(event)) {
    if (menu_button_parent_->IsTriggerableEvent(event))
      return Activate(&event);
  }
  return true;
}

void MenuButtonEventHandler::OnMouseReleased(const ui::MouseEvent& event) {
  if (menu_button_parent_->state() != Button::STATE_DISABLED &&
      menu_button_parent_->IsTriggerableEvent(event) &&
      menu_button_parent_->HitTestPoint(event.location()) &&
      !menu_button_parent_->InDrag()) {
    Activate(&event);
  } else {
    menu_button_parent_->AnimateInkDrop(InkDropState::HIDDEN, &event);
    menu_button_parent_->LabelButton::OnMouseReleased(event);
  }
}

void MenuButtonEventHandler::OnMouseEntered(const ui::MouseEvent& event) {
  if (pressed_lock_count_ == 0)  // Ignore mouse movement if state is locked.
    menu_button_parent_->LabelButton::OnMouseEntered(event);
}

void MenuButtonEventHandler::OnMouseExited(const ui::MouseEvent& event) {
  if (pressed_lock_count_ == 0)  // Ignore mouse movement if state is locked.
    menu_button_parent_->LabelButton::OnMouseExited(event);
}

void MenuButtonEventHandler::OnMouseMoved(const ui::MouseEvent& event) {
  if (pressed_lock_count_ == 0)  // Ignore mouse movement if state is locked.
    menu_button_parent_->LabelButton::OnMouseMoved(event);
}

void MenuButtonEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  if (menu_button_parent_->state() != Button::STATE_DISABLED) {
    auto ref = weak_factory_.GetWeakPtr();
    if (menu_button_parent_->IsTriggerableEvent(*event) && !Activate(event)) {
      if (!ref)
        return;
      // When |Activate()| returns |false|, it means the click was handled by
      // a button listener and has handled the gesture event. So, there is no
      // need to further process the gesture event here. However, if the
      // listener didn't run menu code, we should make sure to reset our state.
      if (menu_button_parent_->state() == Button::STATE_HOVERED)
        menu_button_parent_->SetState(Button::STATE_NORMAL);
      return;
    }
    if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
      event->SetHandled();
      if (pressed_lock_count_ == 0)
        menu_button_parent_->SetState(Button::STATE_HOVERED);
    } else if (menu_button_parent_->state() == Button::STATE_HOVERED &&
               (event->type() == ui::ET_GESTURE_TAP_CANCEL ||
                event->type() == ui::ET_GESTURE_END) &&
               pressed_lock_count_ == 0) {
      menu_button_parent_->SetState(Button::STATE_NORMAL);
    }
  }
  menu_button_parent_->LabelButton::OnGestureEvent(event);
}

bool MenuButtonEventHandler::OnKeyPressed(const ui::KeyEvent& event) {
  switch (event.key_code()) {
    case ui::VKEY_SPACE:
      // Alt-space on windows should show the window menu.
      if (event.IsAltDown())
        break;
      FALLTHROUGH;
    case ui::VKEY_RETURN:
    case ui::VKEY_UP:
    case ui::VKEY_DOWN: {
      // WARNING: we may have been deleted by the time Activate returns.
      Activate(&event);
      // This is to prevent the keyboard event from being dispatched twice.  If
      // the keyboard event is not handled, we pass it to the default handler
      // which dispatches the event back to us causing the menu to get displayed
      // again. Return true to prevent this.
      return true;
    }
    default:
      break;
  }
  return false;
}

bool MenuButtonEventHandler::OnKeyReleased(const ui::KeyEvent& event) {
  // A MenuButton always activates the menu on key press.
  return false;
}

void MenuButtonEventHandler::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  menu_button_parent_->Button::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kPopUpButton;
  node_data->SetHasPopup(ax::mojom::HasPopup::kMenu);
  if (menu_button_parent_->enabled())
    node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kOpen);
}

bool MenuButtonEventHandler::ShouldEnterPushedState(const ui::Event& event) {
  return menu_button_parent_->IsTriggerableEventType(event);
}

void MenuButtonEventHandler::StateChanged(LabelButton::ButtonState old_state) {
  if (pressed_lock_count_ != 0) {
    // The button's state was changed while it was supposed to be locked in a
    // pressed state. This shouldn't happen, but conceivably could if a caller
    // tries to switch from enabled to disabled or vice versa while the button
    // is pressed.
    if (menu_button_parent_->state() == Button::STATE_NORMAL)
      should_disable_after_press_ = false;
    else if (menu_button_parent_->state() == Button::STATE_DISABLED)
      should_disable_after_press_ = true;
  } else {
    menu_button_parent_->LabelButtonStateChanged(old_state);
  }
}

void MenuButtonEventHandler::NotifyClick(const ui::Event& event) {
  // We don't forward events to the normal button listener, instead using the
  // MenuButtonListener.
  Activate(&event);
}

void MenuButtonEventHandler::IncrementPressedLocked(
    bool snap_ink_drop_to_activated,
    const ui::LocatedEvent* event) {
  ++pressed_lock_count_;
  if (increment_pressed_lock_called_)
    *(increment_pressed_lock_called_) = true;
  should_disable_after_press_ =
      menu_button_parent_->state() == Button::STATE_DISABLED;
  if (menu_button_parent_->state() != Button::STATE_PRESSED) {
    if (snap_ink_drop_to_activated) {
      menu_button_parent_->GetInkDrop()->SnapToActivated();
    } else
      menu_button_parent_->AnimateInkDrop(InkDropState::ACTIVATED, event);
  }
  menu_button_parent_->SetState(Button::STATE_PRESSED);
  menu_button_parent_->GetInkDrop()->SetHovered(false);
}

void MenuButtonEventHandler::DecrementPressedLocked() {
  --pressed_lock_count_;
  DCHECK_GE(pressed_lock_count_, 0);

  // If this was the last lock, manually reset state to the desired state.
  if (pressed_lock_count_ == 0) {
    menu_closed_time_ = TimeTicks::Now();
    LabelButton::ButtonState desired_state = Button::STATE_NORMAL;
    if (should_disable_after_press_) {
      desired_state = Button::STATE_DISABLED;
      should_disable_after_press_ = false;
    } else if (menu_button_parent_->GetWidget() &&
               !menu_button_parent_->GetWidget()->dragged_view() &&
               menu_button_parent_->ShouldEnterHoveredState()) {
      desired_state = Button::STATE_HOVERED;
      menu_button_parent_->GetInkDrop()->SetHovered(true);
    }
    menu_button_parent_->SetState(desired_state);
    // The widget may be null during shutdown. If so, it doesn't make sense to
    // try to add an ink drop effect.
    if (menu_button_parent_->GetWidget() &&
        menu_button_parent_->state() != Button::STATE_PRESSED)
      menu_button_parent_->AnimateInkDrop(InkDropState::DEACTIVATED,
                                          nullptr /* event */);
  }
}

int MenuButtonEventHandler::GetMaximumScreenXCoordinate() {
  if (!menu_button_parent_->GetWidget()) {
    NOTREACHED();
    return 0;
  }

  gfx::Rect monitor_bounds =
      menu_button_parent_->GetWidget()->GetWorkAreaBoundsInScreen();
  return monitor_bounds.right() - 1;
}

bool MenuButtonEventHandler::Activate(const ui::Event* event) {
  if (listener_) {
    gfx::Rect lb = menu_button_parent_->GetLocalBounds();

    // Offset of the associated menu position.
    constexpr gfx::Vector2d kMenuOffset{-2, -4};

    // The position of the menu depends on whether or not the locale is
    // right-to-left.
    gfx::Point menu_position(lb.right(), lb.bottom());
    if (base::i18n::IsRTL())
      menu_position.set_x(lb.x());

    View::ConvertPointToScreen(menu_button_parent_, &menu_position);
    if (base::i18n::IsRTL())
      menu_position.Offset(-kMenuOffset.x(), kMenuOffset.y());
    else
      menu_position += kMenuOffset;

    int max_x_coordinate = GetMaximumScreenXCoordinate();
    if (max_x_coordinate && max_x_coordinate <= menu_position.x())
      menu_position.set_x(max_x_coordinate - 1);

    // We're about to show the menu from a mouse press. By showing from the
    // mouse press event we block RootView in mouse dispatching. This also
    // appears to cause RootView to get a mouse pressed BEFORE the mouse
    // release is seen, which means RootView sends us another mouse press no
    // matter where the user pressed. To force RootView to recalculate the
    // mouse target during the mouse press we explicitly set the mouse handler
    // to NULL.
    static_cast<internal::RootView*>(
        menu_button_parent_->GetWidget()->GetRootView())
        ->SetMouseHandler(nullptr);

    DCHECK(increment_pressed_lock_called_ == nullptr);
    // Observe if IncrementPressedLocked() was called so we can trigger the
    // correct ink drop animations.
    bool increment_pressed_lock_called = false;
    increment_pressed_lock_called_ = &increment_pressed_lock_called;

    // Allow for OnMenuButtonClicked() to delete this.
    auto ref = weak_factory_.GetWeakPtr();

    // We don't set our state here. It's handled in the MenuController code or
    // by our click listener.
    listener_->OnMenuButtonClicked(menu_button_parent_, menu_position, event);

    if (!ref) {
      // The menu was deleted while showing. Don't attempt any processing.
      return false;
    }

    increment_pressed_lock_called_ = nullptr;

    if (!increment_pressed_lock_called && pressed_lock_count_ == 0) {
      menu_button_parent_->AnimateInkDrop(InkDropState::ACTION_TRIGGERED,
                                          ui::LocatedEvent::FromIfValid(event));
    }

    // We must return false here so that the RootView does not get stuck
    // sending all mouse pressed events to us instead of the appropriate
    // target.
    return false;
  }

  menu_button_parent_->AnimateInkDrop(InkDropState::HIDDEN,
                                      ui::LocatedEvent::FromIfValid(event));
  return true;
}

bool MenuButtonEventHandler::IsTriggerableEventType(const ui::Event& event) {
  if (event.IsMouseEvent()) {
    const ui::MouseEvent& mouseev = static_cast<const ui::MouseEvent&>(event);
    // Active on left mouse button only, to prevent a menu from being activated
    // when a right-click would also activate a context menu.
    if (!mouseev.IsOnlyLeftMouseButton())
      return false;
    // If dragging is supported activate on release, otherwise activate on
    // pressed.
    ui::EventType active_on =
        menu_button_parent_->GetDragOperations(mouseev.location()) ==
                ui::DragDropTypes::DRAG_NONE
            ? ui::ET_MOUSE_PRESSED
            : ui::ET_MOUSE_RELEASED;
    return event.type() == active_on;
  }

  return event.type() == ui::ET_GESTURE_TAP;
}

// TODO (cyan): Move all of the OnMouse.* methods here.
// This gets more tricky because certain parts are still expected to be called
// in the View::base class.
void MenuButtonEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
      if ((event->flags() &
           (ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON |
            ui::EF_MIDDLE_MOUSE_BUTTON)) == 0) {
        OnMouseMoved(*event);
        return;
      }
      FALLTHROUGH;
    case ui::ET_MOUSE_ENTERED:
      if (event->flags() & ui::EF_TOUCH_ACCESSIBILITY)
        menu_button_parent_->NotifyAccessibilityEvent(ax::mojom::Event::kHover,
                                                      true);
      OnMouseEntered(*event);
      break;
    case ui::ET_MOUSE_EXITED:
      OnMouseExited(*event);
      break;
    default:
      return;
  }
}

bool MenuButtonEventHandler::IsTriggerableEvent(const ui::Event& event) {
  return menu_button_parent_->IsTriggerableEventType(event) &&
         (TimeTicks::Now() - menu_closed_time_).InMilliseconds() >=
             kMinimumMsBetweenButtonClicks;
}

}  // namespace views
