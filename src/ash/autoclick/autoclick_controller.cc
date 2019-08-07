// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/autoclick/autoclick_drag_event_rewriter.h"
#include "ash/autoclick/autoclick_ring_handler.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/accessibility_feature_disable_dialog.h"
#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/root_window_finder.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/timer/timer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The ratio of how long the gesture should take to begin after a dwell to the
// total amount of time of the dwell.
const float kStartGestureDelayRatio = 1 / 6.0;

bool IsModifierKey(const ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_SHIFT || key_code == ui::VKEY_LSHIFT ||
         key_code == ui::VKEY_CONTROL || key_code == ui::VKEY_LCONTROL ||
         key_code == ui::VKEY_RCONTROL || key_code == ui::VKEY_MENU ||
         key_code == ui::VKEY_LMENU || key_code == ui::VKEY_RMENU;
}

base::TimeDelta CalculateStartGestureDelay(base::TimeDelta total_delay) {
  return total_delay * kStartGestureDelayRatio;
}

}  // namespace

// static.
base::TimeDelta AutoclickController::GetDefaultAutoclickDelay() {
  return base::TimeDelta::FromMilliseconds(int64_t{kDefaultAutoclickDelayMs});
}

AutoclickController::AutoclickController()
    : delay_(GetDefaultAutoclickDelay()),
      autoclick_ring_handler_(std::make_unique<AutoclickRingHandler>()),
      drag_event_rewriter_(std::make_unique<AutoclickDragEventRewriter>()) {
  Shell::GetPrimaryRootWindow()->GetHost()->GetEventSource()->AddEventRewriter(
      drag_event_rewriter_.get());
  Shell::Get()->cursor_manager()->AddObserver(this);
  InitClickTimers();
  UpdateRingSize();
}

AutoclickController::~AutoclickController() {
  // Clean up UI.
  menu_bubble_controller_ = nullptr;
  CancelAutoclickAction();

  Shell::Get()->cursor_manager()->RemoveObserver(this);
  Shell::Get()->RemovePreTargetHandler(this);
  SetTapDownTarget(nullptr);
  Shell::GetPrimaryRootWindow()
      ->GetHost()
      ->GetEventSource()
      ->RemoveEventRewriter(drag_event_rewriter_.get());
}

float AutoclickController::GetStartGestureDelayRatioForTesting() {
  return kStartGestureDelayRatio;
}

void AutoclickController::SetTapDownTarget(aura::Window* target) {
  if (tap_down_target_ == target)
    return;

  if (tap_down_target_)
    tap_down_target_->RemoveObserver(this);
  tap_down_target_ = target;
  if (tap_down_target_)
    tap_down_target_->AddObserver(this);
}

void AutoclickController::SetEnabled(bool enabled,
                                     bool show_confirmation_dialog) {
  if (enabled_ == enabled)
    return;

  if (enabled) {
    Shell::Get()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);

    // Only create the bubble controller when needed. Most users will not enable
    // automatic clicks, so there's no need to use these unless the feature
    // is on.
    menu_bubble_controller_ = std::make_unique<AutoclickMenuBubbleController>();
    menu_bubble_controller_->ShowBubble(event_type_, menu_position_);
    enabled_ = enabled;
  } else {
    if (show_confirmation_dialog) {
      // If a dialog exists already, no need to show it again.
      if (disable_dialog_)
        return;
      // Show a confirmation dialog before disabling autoclick.
      auto* dialog = new AccessibilityFeatureDisableDialog(
          IDS_ASH_AUTOCLICK_DISABLE_CONFIRMATION_TITLE,
          IDS_ASH_AUTOCLICK_DISABLE_CONFIRMATION_BODY,
          // Callback for if the user accepts the dialog
          base::BindOnce([]() {
            // If they accept, actually disable autoclick.
            Shell::Get()->autoclick_controller()->SetEnabled(
                false, false /* do not show the dialog */);
          }),
          // Callback for if the user cancels the dialog - marks the
          // feature as enabled again in prefs.
          base::BindOnce([]() {
            AccessibilityController* controller =
                Shell::Get()->accessibility_controller();
            // If they cancel, ensure autoclick is enabled.
            controller->SetAutoclickEnabled(true);
          }));
      disable_dialog_ = dialog->GetWeakPtr();
    } else {
      Shell::Get()->RemovePreTargetHandler(this);
      menu_bubble_controller_ = nullptr;
      enabled_ = enabled;
    }
  }

  CancelAutoclickAction();
}

bool AutoclickController::IsEnabled() const {
  return enabled_;
}

void AutoclickController::SetAutoclickDelay(base::TimeDelta delay) {
  delay_ = delay;
  InitClickTimers();
  if (enabled_) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Accessibility.CrosAutoclickDelay", delay,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(3000), 50);
  }
}

void AutoclickController::SetAutoclickEventType(
    mojom::AutoclickEventType type) {
  if (menu_bubble_controller_)
    menu_bubble_controller_->SetEventType(type);
  if (event_type_ == type)
    return;
  CancelAutoclickAction();
  event_type_ = type;
}

void AutoclickController::SetMovementThreshold(int movement_threshold) {
  movement_threshold_ = movement_threshold;
  UpdateRingSize();
}

void AutoclickController::SetMenuPosition(
    mojom::AutoclickMenuPosition menu_position) {
  menu_position_ = menu_position;
  UpdateAutoclickMenuBoundsIfNeeded();
}

void AutoclickController::UpdateAutoclickMenuBoundsIfNeeded() {
  if (menu_bubble_controller_)
    menu_bubble_controller_->SetPosition(menu_position_);
}

void AutoclickController::CreateAutoclickRingWidget(
    const gfx::Point& point_in_screen) {
  aura::Window* target = ash::wm::GetRootWindowAt(point_in_screen);
  SetTapDownTarget(target);
  aura::Window* root_window = target->GetRootWindow();
  widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);
  widget_->Init(params);
  widget_->SetOpacity(1.f);
}

void AutoclickController::UpdateAutoclickRingWidget(
    views::Widget* widget,
    const gfx::Point& point_in_screen) {
  aura::Window* target = ash::wm::GetRootWindowAt(point_in_screen);
  SetTapDownTarget(target);
  aura::Window* root_window = target->GetRootWindow();
  if (widget->GetNativeView()->GetRootWindow() != root_window) {
    views::Widget::ReparentNativeView(
        widget->GetNativeView(),
        Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));
  }
}

void AutoclickController::DoAutoclickAction() {
  // The gesture_anchor_location_ is the position at which the animation is
  // anchored, and where the click should occur.
  aura::Window* root_window = wm::GetRootWindowAt(gesture_anchor_location_);
  DCHECK(root_window) << "Root window not found while attempting autoclick.";

  // But if the thing that would be acted upon is an autoclick menu button, do a
  // fake click instead of whatever other action type we would have done. This
  // ensures that no matter the autoclick setting, users can always change to
  // another autoclick setting. By using a fake click we avoid closing dialogs
  // and menus, allowing autoclick users to interact with those items.
  if (!DragInProgress() &&
      AutoclickMenuContainsPoint(gesture_anchor_location_)) {
    menu_bubble_controller_->ClickOnBubble(gesture_anchor_location_,
                                           mouse_event_flags_);
    // Reset UI.
    CancelAutoclickAction();
    return;
  }

  // Set the in-progress event type locally so that if the event type is updated
  // in the middle of this event being executed it doesn't change execution.
  mojom::AutoclickEventType in_progress_event_type = event_type_;
  RecordUserAction(in_progress_event_type);

  gfx::Point location_in_pixels(gesture_anchor_location_);
  ::wm::ConvertPointFromScreen(root_window, &location_in_pixels);
  aura::WindowTreeHost* host = root_window->GetHost();
  host->ConvertDIPToPixels(&location_in_pixels);

  bool drag_start =
      in_progress_event_type == mojom::AutoclickEventType::kDragAndDrop &&
      !drag_event_rewriter_->IsEnabled();
  bool drag_stop = DragInProgress();

  if (in_progress_event_type == mojom::AutoclickEventType::kLeftClick ||
      in_progress_event_type == mojom::AutoclickEventType::kRightClick ||
      in_progress_event_type == mojom::AutoclickEventType::kDoubleClick ||
      drag_start || drag_stop) {
    int button =
        in_progress_event_type == mojom::AutoclickEventType::kRightClick
            ? ui::EF_RIGHT_MOUSE_BUTTON
            : ui::EF_LEFT_MOUSE_BUTTON;

    ui::EventDispatchDetails details;
    if (!drag_stop) {
      // Left click, right click, double click, and beginning of a drag have
      // a pressed event next.
      ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, location_in_pixels,
                                 location_in_pixels, ui::EventTimeForNow(),
                                 mouse_event_flags_ | button, button);
      details = host->event_sink()->OnEventFromSource(&press_event);
      if (drag_start) {
        drag_event_rewriter_->SetEnabled(true);
        return;
      }
      if (details.dispatcher_destroyed) {
        OnActionCompleted(in_progress_event_type);
        return;
      }
    }
    if (drag_stop)
      drag_event_rewriter_->SetEnabled(false);
    ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, location_in_pixels,
                                 location_in_pixels, ui::EventTimeForNow(),
                                 mouse_event_flags_ | button, button);
    details = host->event_sink()->OnEventFromSource(&release_event);

    // Now a single click, or half the drag & drop, has been completed.
    if (in_progress_event_type != mojom::AutoclickEventType::kDoubleClick ||
        details.dispatcher_destroyed) {
      OnActionCompleted(in_progress_event_type);
      return;
    }

    ui::MouseEvent double_press_event(
        ui::ET_MOUSE_PRESSED, location_in_pixels, location_in_pixels,
        ui::EventTimeForNow(),
        mouse_event_flags_ | button | ui::EF_IS_DOUBLE_CLICK, button);
    ui::MouseEvent double_release_event(
        ui::ET_MOUSE_RELEASED, location_in_pixels, location_in_pixels,
        ui::EventTimeForNow(),
        mouse_event_flags_ | button | ui::EF_IS_DOUBLE_CLICK, button);
    details = host->event_sink()->OnEventFromSource(&double_press_event);
    if (details.dispatcher_destroyed) {
      OnActionCompleted(in_progress_event_type);
      return;
    }
    details = host->event_sink()->OnEventFromSource(&double_release_event);
    OnActionCompleted(in_progress_event_type);
  }
}

void AutoclickController::StartAutoclickGesture() {
  if (event_type_ == mojom::AutoclickEventType::kNoAction) {
    // If we are set to "no action" and the gesture wouldn't occur over
    // the autoclick menu, cancel and return early rather than starting the
    // gesture.
    if (!AutoclickMenuContainsPoint(gesture_anchor_location_)) {
      CancelAutoclickAction();
      return;
    }
    // Otherwise, go ahead and start the gesture.
  }
  // The anchor is always the point in the screen where the timer starts, and is
  // used to determine when the cursor has moved far enough to cancel the
  // autoclick.
  anchor_location_ = gesture_anchor_location_;
  autoclick_ring_handler_->StartGesture(
      delay_ - CalculateStartGestureDelay(delay_), anchor_location_,
      widget_.get());
  autoclick_timer_->Reset();
}

void AutoclickController::CancelAutoclickAction() {
  if (start_gesture_timer_)
    start_gesture_timer_->Stop();
  if (autoclick_timer_)
    autoclick_timer_->Stop();
  autoclick_ring_handler_->StopGesture();

  // If we are dragging, complete the drag, so as not to leave the UI in a
  // weird state.
  if (DragInProgress()) {
    DoAutoclickAction();
  }
  drag_event_rewriter_->SetEnabled(false);
  SetTapDownTarget(nullptr);
}

void AutoclickController::OnActionCompleted(
    mojom::AutoclickEventType completed_event_type) {
  // No need to change to left click if the setting is not enabled or the
  // event that just executed already was a left click.
  if (!revert_to_left_click_ ||
      event_type_ == mojom::AutoclickEventType::kLeftClick ||
      completed_event_type == mojom::AutoclickEventType::kLeftClick)
    return;
  // Change the preference, but set it locally so we do not reset any state when
  // AutoclickController::SetAutoclickEventType is called.
  event_type_ = mojom::AutoclickEventType::kLeftClick;
  Shell::Get()->accessibility_controller()->SetAutoclickEventType(event_type_);
}

void AutoclickController::InitClickTimers() {
  CancelAutoclickAction();
  base::TimeDelta start_gesture_delay = CalculateStartGestureDelay(delay_);
  if (autoclick_timer_ && autoclick_timer_->IsRunning())
    autoclick_timer_->Stop();
  if (start_gesture_timer_ && start_gesture_timer_->IsRunning())
    start_gesture_timer_->Stop();
  autoclick_timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, delay_ - start_gesture_delay,
      base::BindRepeating(&AutoclickController::DoAutoclickAction,
                          base::Unretained(this)));
  start_gesture_timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, start_gesture_delay,
      base::BindRepeating(&AutoclickController::StartAutoclickGesture,
                          base::Unretained(this)));
}

void AutoclickController::UpdateRingWidget(const gfx::Point& point_in_screen) {
  if (!widget_) {
    CreateAutoclickRingWidget(point_in_screen);
  } else {
    UpdateAutoclickRingWidget(widget_.get(), point_in_screen);
  }
}

void AutoclickController::UpdateRingSize() {
  autoclick_ring_handler_->SetSize(movement_threshold_);
}

bool AutoclickController::DragInProgress() const {
  return event_type_ == mojom::AutoclickEventType::kDragAndDrop &&
         drag_event_rewriter_->IsEnabled();
}

bool AutoclickController::AutoclickMenuContainsPoint(
    const gfx::Point& point) const {
  return menu_bubble_controller_ &&
         menu_bubble_controller_->ContainsPointInScreen(point);
}

void AutoclickController::RecordUserAction(
    mojom::AutoclickEventType event_type) const {
  switch (event_type) {
    case mojom::AutoclickEventType::kLeftClick:
      base::RecordAction(
          base::UserMetricsAction("Accessibility.Autoclick.LeftClick"));
      return;
    case mojom::AutoclickEventType::kRightClick:
      base::RecordAction(
          base::UserMetricsAction("Accessibility.Autoclick.RightClick"));
      return;
    case mojom::AutoclickEventType::kDoubleClick:
      base::RecordAction(
          base::UserMetricsAction("Accessibility.Autoclick.DoubleClick"));
      return;
    case mojom::AutoclickEventType::kDragAndDrop:
      // Only log drag-and-drop once per drag-and-drop. It takes two "dwells"
      // to complete a full drag-and-drop cycle, which could lead to double
      // the events logged.
      if (DragInProgress())
        return;
      base::RecordAction(
          base::UserMetricsAction("Accessibility.Autoclick.DragAndDrop"));
      return;
    case mojom::AutoclickEventType::kNoAction:
      // No action shouldn't have a UserAction, so we return null.
      return;
  }
}

void AutoclickController::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(event->target());
  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED)
    return;
  last_mouse_location_ = event->target()->GetScreenLocation(*event);
  if (!(event->flags() & ui::EF_IS_SYNTHESIZED) &&
      (event->type() == ui::ET_MOUSE_MOVED ||
       (event->type() == ui::ET_MOUSE_DRAGGED &&
        drag_event_rewriter_->IsEnabled()))) {
    mouse_event_flags_ = event->flags();
    // Update the point even if the animation is not currently being shown.
    UpdateRingWidget(last_mouse_location_);

    // The distance between the mouse location and the anchor location
    // must exceed a certain threshold to initiate a new autoclick countdown.
    // This ensures that mouse jitter caused by poor motor control does not
    // 1. initiate an unwanted autoclick from rest
    // 2. prevent the autoclick from ever occurring when the mouse
    //    arrives at the target.
    gfx::Vector2d delta = last_mouse_location_ - anchor_location_;
    if (delta.LengthSquared() >= movement_threshold_ * movement_threshold_) {
      anchor_location_ = last_mouse_location_;
      gesture_anchor_location_ = last_mouse_location_;
      // Stop all the timers, restarting the gesture timer only. This keeps
      // the animation from being drawn while the user is still moving quickly.
      start_gesture_timer_->Reset();
      if (autoclick_timer_) {
        autoclick_timer_->Stop();
      }
      autoclick_ring_handler_->StopGesture();
    } else if (start_gesture_timer_->IsRunning()) {
      // Keep track of where the gesture will be anchored.
      gesture_anchor_location_ = last_mouse_location_;
    } else if (autoclick_timer_->IsRunning() && !stabilize_click_position_) {
      // If we are not stabilizing the click position, update the gesture
      // center with each mouse move event.
      gesture_anchor_location_ = last_mouse_location_;
      autoclick_ring_handler_->SetGestureCenter(last_mouse_location_,
                                                widget_.get());
    }
  } else if (event->type() == ui::ET_MOUSE_PRESSED ||
             event->type() == ui::ET_MOUSE_RELEASED ||
             event->type() == ui::ET_MOUSEWHEEL) {
    CancelAutoclickAction();
  }
}

void AutoclickController::OnKeyEvent(ui::KeyEvent* event) {
  int modifier_mask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                      ui::EF_IS_EXTENDED_KEY;
  int new_modifiers = event->flags() & modifier_mask;
  mouse_event_flags_ = (mouse_event_flags_ & ~modifier_mask) | new_modifiers;

  if (!IsModifierKey(event->key_code()))
    CancelAutoclickAction();
}

void AutoclickController::OnTouchEvent(ui::TouchEvent* event) {
  CancelAutoclickAction();
}

void AutoclickController::OnGestureEvent(ui::GestureEvent* event) {
  CancelAutoclickAction();
}

void AutoclickController::OnScrollEvent(ui::ScrollEvent* event) {
  // A single tap can create a scroll event, so ignore scroll starts and
  // cancels but cancel autoclicks when scrolls actually occur.
  if (event->type() == ui::EventType::ET_SCROLL_FLING_START ||
      event->type() == ui::EventType::ET_SCROLL_FLING_CANCEL)
    return;

  CancelAutoclickAction();
}

void AutoclickController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(tap_down_target_, window);
  CancelAutoclickAction();
}

void AutoclickController::OnCursorVisibilityChanged(bool is_visible) {
  if (!menu_bubble_controller_)
    return;
  // TODO(katie): Check that the display which is fullscreen is the same as the
  // one containing the bubble, to determine whether to hide the bubble.
  // Currently just checking if the display under the mouse is fullscreen.
  aura::Window* window = wm::GetWindowForFullscreenModeInRoot(
      wm::GetRootWindowAt(last_mouse_location_));
  bool is_fullscreen = window != nullptr;

  // Hide the bubble when the cursor is gone in fullscreen mode.
  // Always show it otherwise.
  menu_bubble_controller_->SetBubbleVisibility(is_fullscreen ? is_visible
                                                             : true);
}

}  // namespace ash
