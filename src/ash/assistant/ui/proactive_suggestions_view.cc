// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/proactive_suggestions_view.h"

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

ProactiveSuggestionsView::ProactiveSuggestionsView(
    AssistantViewDelegate* delegate)
    : views::Button(/*listener=*/this),
      delegate_(delegate),
      proactive_suggestions_(
          delegate_->GetSuggestionsModel()->GetProactiveSuggestions()),
      keyboard_workspace_occluded_bounds_(
          keyboard::KeyboardUIController::Get()
              ->GetWorkspaceOccludedBoundsInScreen()) {
  DCHECK(proactive_suggestions_);

  // We need to observe both the screen and the keyboard states to allow us to
  // dynamically re-bound in response to changes in the usable work area.
  display::Screen::GetScreen()->AddObserver(this);
  keyboard::KeyboardUIController::Get()->AddObserver(this);
}

ProactiveSuggestionsView::~ProactiveSuggestionsView() {
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);

  if (GetWidget() && GetWidget()->GetNativeWindow())
    GetWidget()->GetNativeWindow()->RemoveObserver(this);
}

void ProactiveSuggestionsView::Init() {
  // By default, a Button will add a FocusRing to its view hierarchy to indicate
  // focus. The widget for this view is not activatable, and this view cannot be
  // focused, but the presence of the FocusRing leads to unexpected behavior
  // when checking against children(), so it's better that we don't install it.
  SetInstallFocusRingOnFocus(false);

  InitLayout();
  InitWidget();
  InitWindow();
  UpdateBounds();
}

const char* ProactiveSuggestionsView::GetClassName() const {
  return "ProactiveSuggestionsView";
}

void ProactiveSuggestionsView::PreferredSizeChanged() {
  if (GetWidget())
    UpdateBounds();
}

void ProactiveSuggestionsView::OnMouseEntered(const ui::MouseEvent& event) {
  views::Button::OnMouseEntered(event);
  delegate_->OnProactiveSuggestionsViewHoverChanged(/*is_hovering=*/true);
}

void ProactiveSuggestionsView::OnMouseExited(const ui::MouseEvent& event) {
  views::Button::OnMouseExited(event);
  delegate_->OnProactiveSuggestionsViewHoverChanged(/*is_hovering=*/false);
}

void ProactiveSuggestionsView::OnGestureEvent(ui::GestureEvent* event) {
  views::Button::OnGestureEvent(event);
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_CANCEL:
      delegate_->OnProactiveSuggestionsViewHoverChanged(/*is_hovering=*/false);
      break;
    case ui::ET_GESTURE_TAP_DOWN:
      delegate_->OnProactiveSuggestionsViewHoverChanged(/*is_hovering=*/true);
      break;
    default:
      // No action necessary.
      break;
  }
}

void ProactiveSuggestionsView::OnWidgetClosing(views::Widget* widget) {
  widget->RemoveObserver(this);

  // When closing, the proactive suggestions window fades out.
  wm::SetWindowVisibilityAnimationType(
      widget->GetNativeWindow(),
      wm::WindowVisibilityAnimationType::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
}

void ProactiveSuggestionsView::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
}

void ProactiveSuggestionsView::OnWindowVisibilityChanging(aura::Window* window,
                                                          bool visible) {
  // If the widget is marked as closed, we are in a closing sequence and we
  // don't need to update the window animation as the appropriate animation for
  // close has already been set in OnWidgetClosing().
  if (GetWidget()->IsClosed())
    return;

  // If showing/hiding, the proactive suggestions window translates vertically.
  wm::SetWindowVisibilityAnimationType(
      window, wm::WindowVisibilityAnimationType::
                  WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);

  // The vertical position is equal to the distance from the top of the
  // proactive suggestions view to the bottom of the screen. This gives the
  // effect of animating the proactive suggestions window up from offscreen.
  wm::SetWindowVisibilityAnimationVerticalPosition(
      window, display::Screen::GetScreen()
                      ->GetDisplayNearestWindow(window)
                      .bounds()
                      .bottom() -
                  GetBoundsInScreen().y());
}

void ProactiveSuggestionsView::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // When the keyboard is showing, display metric changes to the usable work
  // area are handled by OnKeyboardOccludedBoundsChanged. This accounts for
  // inconsistencies between the virtual keyboard and the A11Y keyboard.
  if (!keyboard_workspace_occluded_bounds_.IsEmpty())
    return;

  // We only respond to events that occur in the same display as our view.
  auto* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  if (root_window == delegate_->GetRootWindowForDisplayId(display.id()))
    UpdateBounds();
}

void ProactiveSuggestionsView::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& new_bounds_in_screen) {
  if (!new_bounds_in_screen.IsEmpty()) {
    // We only respond to events that occur in the same display as our view.
    auto* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
    if (root_window != delegate_->GetRootWindowForDisplayId(
                           display::Screen::GetScreen()
                               ->GetDisplayMatching(new_bounds_in_screen)
                               .id())) {
      return;
    }
  }

  keyboard_workspace_occluded_bounds_ = new_bounds_in_screen;
  UpdateBounds();
}

void ProactiveSuggestionsView::ShowWhenReady() {
  GetWidget()->ShowInactive();
}

void ProactiveSuggestionsView::Hide() {
  GetWidget()->Hide();
}

void ProactiveSuggestionsView::Close() {
  GetWidget()->Close();
}

void ProactiveSuggestionsView::InitWidget() {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.context = delegate_->GetRootWindowForNewWindows();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetContentsView(this);

  // We observe the |widget| in order to modify animation behavior of its window
  // prior to closing. This is necessary due to the fact that we utilize a
  // different animation on widget close than we do for widget show and hide.
  widget->AddObserver(this);
}

void ProactiveSuggestionsView::InitWindow() {
  auto* window = GetWidget()->GetNativeWindow();

  // Initialize the transition duration of the entry/exit animations.
  constexpr int kAnimationDurationMs = 350;
  wm::SetWindowVisibilityAnimationDuration(
      window, base::TimeDelta::FromMilliseconds(kAnimationDurationMs));

  // There is no window property support for modifying entry/exit animation
  // tween type so we set our desired value directly on the LayerAnimator.
  window->layer()->GetAnimator()->set_tween_type(
      gfx::Tween::Type::FAST_OUT_SLOW_IN);

  // We observe the window in order to modify animation behavior prior to window
  // visibility changes. This needs to be done dynamically as bounds are not
  // fully initialized yet for calculating offset position.
  window->AddObserver(this);
}

void ProactiveSuggestionsView::UpdateBounds() {
  const gfx::Rect screen_bounds =
      GetWidget()->GetNativeWindow()->GetRootWindow()->GetBoundsInScreen();

  // Calculate the usable work area, accounting for keyboard state.
  gfx::Rect usable_work_area;
  if (keyboard_workspace_occluded_bounds_.height()) {
    // The virtual keyboard, unlike the A11Y keyboard, doesn't affect display
    // work area, so we need to manually calculate usable work area.
    usable_work_area = gfx::Rect(
        screen_bounds.x(), screen_bounds.y(), screen_bounds.width(),
        screen_bounds.height() - keyboard_workspace_occluded_bounds_.height());
  } else {
    // When no keyboard is present, we can use the display's calculation of
    // usable work area.
    usable_work_area = display::Screen::GetScreen()
                           ->GetDisplayMatching(screen_bounds)
                           .work_area();
  }

  // Inset the usable work area so that we have a margin between the our view
  // and other system UI (e.g. the shelf).
  usable_work_area.Inset(kMarginDip, kMarginDip);

  // The view is bottom-left aligned.
  const gfx::Size size = GetPreferredSize();
  const int left = usable_work_area.x();
  const int top = usable_work_area.bottom() - size.height();
  GetWidget()->SetBounds(gfx::Rect(left, top, size.width(), size.height()));
}

}  // namespace ash
