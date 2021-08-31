// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget.h"

#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_tray.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/status_area_overflow_button_tray.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "media/base/media_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// StatusAreaWidget::ScopedTrayBubbleCounter

StatusAreaWidget::ScopedTrayBubbleCounter::ScopedTrayBubbleCounter(
    StatusAreaWidget* status_area_widget)
    : status_area_widget_(status_area_widget->weak_ptr_factory_.GetWeakPtr()) {
  if (status_area_widget_->tray_bubble_count_ == 0) {
    status_area_widget_->shelf()
        ->shelf_layout_manager()
        ->OnShelfTrayBubbleVisibilityChanged(/*bubble_shown=*/true);
  }
  ++status_area_widget_->tray_bubble_count_;
}

StatusAreaWidget::ScopedTrayBubbleCounter::~ScopedTrayBubbleCounter() {
  // ScopedTrayBubbleCounter may live longer than StatusAreaWidget.
  if (!status_area_widget_)
    return;

  --status_area_widget_->tray_bubble_count_;
  if (status_area_widget_->tray_bubble_count_ == 0) {
    status_area_widget_->shelf()
        ->shelf_layout_manager()
        ->OnShelfTrayBubbleVisibilityChanged(/*bubble_shown=*/false);
  }

  DCHECK_GE(status_area_widget_->tray_bubble_count_, 0);
}

////////////////////////////////////////////////////////////////////////////////
// StatusAreaWidget

StatusAreaWidget::StatusAreaWidget(aura::Window* status_container, Shelf* shelf)
    : status_area_widget_delegate_(new StatusAreaWidgetDelegate(shelf)),
      shelf_(shelf) {
  DCHECK(status_container);
  DCHECK(shelf);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = status_area_widget_delegate_;
  params.name = "StatusAreaWidget";
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = status_container;
  Init(std::move(params));
  set_focus_on_creation(false);
  SetContentsView(status_area_widget_delegate_);
}

void StatusAreaWidget::Initialize() {
  DCHECK(!initialized_);

  // Create the child views, left to right.
  overflow_button_tray_ =
      std::make_unique<StatusAreaOverflowButtonTray>(shelf_);
  AddTrayButton(overflow_button_tray_.get());

  holding_space_tray_ = std::make_unique<HoldingSpaceTray>(shelf_);
  AddTrayButton(holding_space_tray_.get());

  logout_button_tray_ = std::make_unique<LogoutButtonTray>(shelf_);
  AddTrayButton(logout_button_tray_.get());

  dictation_button_tray_ = std::make_unique<DictationButtonTray>(shelf_);
  AddTrayButton(dictation_button_tray_.get());

  select_to_speak_tray_ = std::make_unique<SelectToSpeakTray>(shelf_);
  AddTrayButton(select_to_speak_tray_.get());

  ime_menu_tray_ = std::make_unique<ImeMenuTray>(shelf_);
  AddTrayButton(ime_menu_tray_.get());

  virtual_keyboard_tray_ = std::make_unique<VirtualKeyboardTray>(shelf_);
  AddTrayButton(virtual_keyboard_tray_.get());

  if (features::IsCaptureModeEnabled()) {
    stop_recording_button_tray_ =
        std::make_unique<StopRecordingButtonTray>(shelf_);
    AddTrayButton(stop_recording_button_tray_.get());
  }

  palette_tray_ = std::make_unique<PaletteTray>(shelf_);
  AddTrayButton(palette_tray_.get());

  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsForChromeOS)) {
    media_tray_ = std::make_unique<MediaTray>(shelf_);
    AddTrayButton(media_tray_.get());
  }

  if (chromeos::features::IsPhoneHubEnabled()) {
    phone_hub_tray_ = std::make_unique<PhoneHubTray>(shelf_);
    AddTrayButton(phone_hub_tray_.get());
  }

  unified_system_tray_ = std::make_unique<UnifiedSystemTray>(shelf_);
  AddTrayButton(unified_system_tray_.get());

  overview_button_tray_ = std::make_unique<OverviewButtonTray>(shelf_);
  AddTrayButton(overview_button_tray_.get());

  // Each tray_button's animation will be disabled for the life time of this
  // local `animation_disablers`, which means the closures to enable the
  // animation will be executed when it's out of this method (Initialize())
  // scope.
  std::list<base::ScopedClosureRunner> animation_disablers;

  // Initialize after all trays have been created.
  for (TrayBackgroundView* tray_button : tray_buttons_) {
    tray_button->Initialize();
    animation_disablers.push_back(tray_button->DisableShowAnimation());
  }

  EnsureTrayOrder();

  UpdateAfterLoginStatusChange(
      Shell::Get()->session_controller()->login_status());
  UpdateLayout(/*animate=*/false);

  Shell::Get()->session_controller()->AddObserver(this);

  // NOTE: Container may be hidden depending on login/display state.
  Show();

  initialized_ = true;
}

StatusAreaWidget::~StatusAreaWidget() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
StatusAreaWidget* StatusAreaWidget::ForWindow(aura::Window* window) {
  return Shelf::ForWindow(window)->status_area_widget();
}

void StatusAreaWidget::UpdateAfterLoginStatusChange(LoginStatus login_status) {
  if (login_status_ == login_status)
    return;
  login_status_ = login_status;

  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->UpdateAfterLoginStatusChange();
}

void StatusAreaWidget::SetSystemTrayVisibility(bool visible) {
  TrayBackgroundView* tray = unified_system_tray_.get();
  tray->SetVisiblePreferred(visible);
  if (visible) {
    Show();
  } else {
    tray->CloseBubble();
    Hide();
  }
}

void StatusAreaWidget::OnSessionStateChanged(
    session_manager::SessionState state) {
  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->UpdateBackground();
}

void StatusAreaWidget::UpdateCollapseState() {
  collapse_state_ = CalculateCollapseState();

  if (collapse_state_ == CollapseState::COLLAPSED) {
    CalculateButtonVisibilityForCollapsedState();
  } else {
    // All tray buttons are visible when the status area is not collapsible.
    // This is the most common state. They are also all visible when the status
    // area is expanded by the user.
    overflow_button_tray_->SetVisiblePreferred(collapse_state_ ==
                                               CollapseState::EXPANDED);
    for (TrayBackgroundView* tray_button : tray_buttons_) {
      tray_button->set_show_when_collapsed(true);
      tray_button->UpdateAfterStatusAreaCollapseChange();
    }
  }

  status_area_widget_delegate_->OnStatusAreaCollapseStateChanged(
      collapse_state_);
}

void StatusAreaWidget::LogVisiblePodCountMetric() {
  int visible_pod_count = 0;
  for (auto* tray_button : tray_buttons_) {
    if (tray_button == overflow_button_tray_.get() ||
        tray_button == overview_button_tray_.get() ||
        tray_button == unified_system_tray_.get() || !tray_button->GetVisible())
      continue;

    visible_pod_count += 1;
  }

  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    UMA_HISTOGRAM_COUNTS_100("ChromeOS.SystemTray.Tablet.ShelfPodCount",
                             visible_pod_count);
  } else {
    UMA_HISTOGRAM_COUNTS_100("ChromeOS.SystemTray.ShelfPodCount",
                             visible_pod_count);
  }
}

void StatusAreaWidget::CalculateTargetBounds() {
  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->CalculateTargetBounds();
  status_area_widget_delegate_->CalculateTargetBounds();

  gfx::Size status_size(status_area_widget_delegate_->GetTargetBounds().size());
  const gfx::Size shelf_size = shelf_->shelf_widget()->GetTargetBounds().size();
  const gfx::Point shelf_origin =
      shelf_->shelf_widget()->GetTargetBounds().origin();

  if (shelf_->IsHorizontalAlignment())
    status_size.set_height(shelf_size.height());
  else
    status_size.set_width(shelf_size.width());

  gfx::Point status_origin = shelf_->SelectValueForShelfAlignment(
      gfx::Point(0, 0),
      gfx::Point(shelf_size.width() - status_size.width(),
                 shelf_size.height() - status_size.height()),
      gfx::Point(0, shelf_size.height() - status_size.height()));
  if (shelf_->IsHorizontalAlignment() && !base::i18n::IsRTL())
    status_origin.set_x(shelf_size.width() - status_size.width());
  status_origin.Offset(shelf_origin.x(), shelf_origin.y());
  target_bounds_ = gfx::Rect(status_origin, status_size);
}

gfx::Rect StatusAreaWidget::GetTargetBounds() const {
  return target_bounds_;
}

void StatusAreaWidget::UpdateLayout(bool animate) {
  const LayoutInputs new_layout_inputs = GetLayoutInputs();
  if (layout_inputs_ == new_layout_inputs)
    return;

  if (!new_layout_inputs.should_animate)
    animate = false;

  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->UpdateLayout();
  status_area_widget_delegate_->UpdateLayout(animate);

  // Having a window which is visible but does not have an opacity is an
  // illegal state.
  ui::Layer* layer = GetNativeView()->layer();
  layer->SetOpacity(new_layout_inputs.opacity);
  if (new_layout_inputs.opacity)
    ShowInactive();
  else
    Hide();

  ui::ScopedLayerAnimationSettings animation_setter(layer->GetAnimator());
  animation_setter.SetTransitionDuration(
      animate ? ShelfConfig::Get()->shelf_animation_duration()
              : base::TimeDelta::FromMilliseconds(0));
  animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
  animation_setter.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  SetBounds(new_layout_inputs.bounds);
  layout_inputs_ = new_layout_inputs;
}

void StatusAreaWidget::UpdateTargetBoundsForGesture(int shelf_position) {
  if (shelf_->IsHorizontalAlignment())
    target_bounds_.set_y(shelf_position);
  else
    target_bounds_.set_x(shelf_position);
}

void StatusAreaWidget::HandleLocaleChange() {
  // Here we force the layer's bounds to be updated for text direction (if
  // needed).
  status_area_widget_delegate_->RemoveAllChildViews(/*delete_children=*/false);

  // The layout manager will be updated when shelf layout gets updated, which is
  // done by the shelf layout manager after `HandleLocaleChange()` gets called.
  status_area_widget_delegate_->SetLayoutManager(nullptr);
  for (auto* tray_button : tray_buttons_) {
    tray_button->HandleLocaleChange();
    status_area_widget_delegate_->AddChildView(tray_button);
  }
  EnsureTrayOrder();
}

void StatusAreaWidget::CalculateButtonVisibilityForCollapsedState() {
  if (!initialized_)
    return;

  DCHECK(collapse_state_ == CollapseState::COLLAPSED);

  bool force_collapsible = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshForceStatusAreaCollapsible);

  // If |stop_recording_button_tray_| is visible, make some space in tray for
  // it.
  const int stop_recording_button_width =
      stop_recording_button_tray_->visible_preferred()
          ? stop_recording_button_tray_->GetPreferredSize().width()
          : 0;

  // We update visibility of each tray button based on the available width.
  const int shelf_width =
      shelf_->shelf_widget()->GetClientAreaBoundsInScreen().width();
  const int available_width =
      (force_collapsible
           ? kStatusAreaForceCollapseAvailableWidth
           : shelf_width / 2 - kStatusAreaLeftPaddingForOverflow) -
      stop_recording_button_width;

  // First, reset all tray button to be hidden.
  overflow_button_tray_->ResetStateToCollapsed();
  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->set_show_when_collapsed(false);

  // Iterate backwards making tray buttons visible until |available_width| is
  // exceeded.
  TrayBackgroundView* previous_tray = nullptr;
  bool show_overflow_button = false;
  int used_width = 0;
  for (TrayBackgroundView* tray : base::Reversed(tray_buttons_)) {
    // Skip non-enabled tray buttons.
    if (!tray->visible_preferred())
      continue;
    // Skip |stop_recording_button_tray_| since it's always visible.
    if (tray == stop_recording_button_tray_.get())
      continue;

    // Show overflow button once available width is exceeded.
    int tray_width = tray->tray_container()->GetPreferredSize().width();
    if (used_width + tray_width > available_width) {
      show_overflow_button = true;

      // Maybe remove the last tray button to make rooom for the overflow tray.
      int overflow_button_width =
          overflow_button_tray_->GetPreferredSize().width();
      if (previous_tray && used_width + overflow_button_width > available_width)
        previous_tray->set_show_when_collapsed(false);
      break;
    }

    tray->set_show_when_collapsed(true);
    previous_tray = tray;
    used_width += tray_width;
  }

  // Skip |stop_recording_button_tray_| so it's always visible.
  if (stop_recording_button_tray_->visible_preferred())
    stop_recording_button_tray_->set_show_when_collapsed(true);

  overflow_button_tray_->SetVisiblePreferred(show_overflow_button);
  overflow_button_tray_->UpdateAfterStatusAreaCollapseChange();
  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->UpdateAfterStatusAreaCollapseChange();
}

void StatusAreaWidget::EnsureTrayOrder() {
  if (features::IsCaptureModeEnabled()) {
    status_area_widget_delegate_->ReorderChildView(
        stop_recording_button_tray_.get(), 1);
  }
}

StatusAreaWidget::CollapseState StatusAreaWidget::CalculateCollapseState()
    const {
  // The status area is only collapsible in tablet mode. Otherwise, we just show
  // all trays.
  if (!Shell::Get()->tablet_mode_controller())
    return CollapseState::NOT_COLLAPSIBLE;

  // An update may occur during initialization of the shelf, so just skip it.
  if (!initialized_)
    return CollapseState::NOT_COLLAPSIBLE;

  bool is_collapsible =
      Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      ShelfConfig::Get()->is_in_app();

  bool force_collapsible = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshForceStatusAreaCollapsible);

  is_collapsible |= force_collapsible;
  CollapseState state = CollapseState::NOT_COLLAPSIBLE;
  if (is_collapsible) {
    // Update the collapse state based on the previous overflow button state.
    state = overflow_button_tray_->state() ==
                    StatusAreaOverflowButtonTray::CLICK_TO_EXPAND
                ? CollapseState::COLLAPSED
                : CollapseState::EXPANDED;
  } else {
    state = CollapseState::NOT_COLLAPSIBLE;
  }

  if (state == CollapseState::COLLAPSED) {
    // We might not need to be collapsed, if there is enough space for all the
    // buttons.
    const int shelf_width =
        shelf_->shelf_widget()->GetClientAreaBoundsInScreen().width();
    const int available_width =
        force_collapsible ? kStatusAreaForceCollapseAvailableWidth
                          : shelf_width / 2 - kStatusAreaLeftPaddingForOverflow;
    int used_width = 0;
    for (TrayBackgroundView* tray : base::Reversed(tray_buttons_)) {
      // If we reach the final overflow tray button, then all the tray buttons
      // fit and there is no need for a collapse state.
      if (tray == overflow_button_tray_.get())
        return CollapseState::NOT_COLLAPSIBLE;

      // Skip non-enabled tray buttons.
      if (!tray->visible_preferred())
        continue;
      int tray_width = tray->tray_container()->GetPreferredSize().width();
      if (used_width + tray_width > available_width)
        break;

      used_width += tray_width;
    }
  }
  return state;
}

TrayBackgroundView* StatusAreaWidget::GetSystemTrayAnchor() const {
  // Use the target visibility of the layer instead of the visibility of the
  // view because the view is still visible when fading away, but we do not want
  // to anchor to this element in that case.
  if (overview_button_tray_->layer()->GetTargetVisibility())
    return overview_button_tray_.get();

  return unified_system_tray_.get();
}

gfx::Rect StatusAreaWidget::GetMediaTrayAnchorRect() const {
  if (!media_tray_)
    return gfx::Rect();

  // Calculate anchor rect of media tray bubble. This is required because the
  // bubble can be visible while the tray button is hidden. (e.g. when user
  // clicks the unpin button in the dialog, which will not close the dialog)
  bool found_media_tray = false;
  int offset = 0;

  // Accumulate the width/height of all visible tray buttons after media tray.
  for (views::View* tray_button : tray_buttons_) {
    if (tray_button == media_tray_.get()) {
      found_media_tray = true;
      continue;
    }

    if (!found_media_tray || !tray_button->GetVisible())
      continue;

    offset += shelf_->IsHorizontalAlignment() ? tray_button->width()
                                              : tray_button->height();
  }

  // Use system tray anchor view (system tray or overview button tray if
  // visible) to find media tray button's origin.
  gfx::Rect system_tray_bounds = GetSystemTrayAnchor()->GetBoundsInScreen();

  switch (shelf_->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      if (base::i18n::IsRTL()) {
        return gfx::Rect(system_tray_bounds.origin() + gfx::Vector2d(offset, 0),
                         gfx::Size());
      } else {
        return gfx::Rect(
            system_tray_bounds.top_right() - gfx::Vector2d(offset, 0),
            gfx::Size());
      }
    case ShelfAlignment::kLeft:
      return gfx::Rect(
          system_tray_bounds.bottom_right() - gfx::Vector2d(0, offset),
          gfx::Size());
    case ShelfAlignment::kRight:
      return gfx::Rect(
          system_tray_bounds.bottom_left() - gfx::Vector2d(0, offset),
          gfx::Size());
  }

  NOTREACHED();
  return gfx::Rect();
}

bool StatusAreaWidget::ShouldShowShelf() const {
  // If it has main bubble, return true.
  if (unified_system_tray_->IsBubbleShown())
    return true;

  // If it has a slider bubble, return false.
  if (unified_system_tray_->IsSliderBubbleShown())
    return false;

  // All other tray bubbles on the same display with status area widget will
  // force the shelf to be visible.
  return tray_bubble_count_ > 0;
}

bool StatusAreaWidget::IsMessageBubbleShown() const {
  return unified_system_tray_->IsBubbleShown();
}

void StatusAreaWidget::SchedulePaint() {
  for (TrayBackgroundView* tray_button : tray_buttons_)
    tray_button->SchedulePaint();
}

bool StatusAreaWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;
  if (active)
    status_area_widget_delegate_->SetPaneFocusAndFocusDefault();
  return true;
}

void StatusAreaWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsMouseWheelEvent()) {
    ui::MouseWheelEvent* mouse_wheel_event = event->AsMouseWheelEvent();
    shelf_->ProcessMouseWheelEvent(mouse_wheel_event);
    return;
  }

  // Clicking anywhere except the virtual keyboard tray icon should hide the
  // virtual keyboard.
  gfx::Point location = event->location();
  views::View::ConvertPointFromWidget(virtual_keyboard_tray_.get(), &location);
  if (event->type() == ui::ET_MOUSE_PRESSED &&
      !virtual_keyboard_tray_->HitTestPoint(location)) {
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  }
  views::Widget::OnMouseEvent(event);
}

void StatusAreaWidget::OnGestureEvent(ui::GestureEvent* event) {
  // Tapping anywhere except the virtual keyboard tray icon should hide the
  // virtual keyboard.
  gfx::Point location = event->location();
  views::View::ConvertPointFromWidget(virtual_keyboard_tray_.get(), &location);
  if (event->type() == ui::ET_GESTURE_TAP_DOWN &&
      !virtual_keyboard_tray_->HitTestPoint(location)) {
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  }
  views::Widget::OnGestureEvent(event);
}

void StatusAreaWidget::OnScrollEvent(ui::ScrollEvent* event) {
  shelf_->ProcessScrollEvent(event);
  if (!event->handled())
    views::Widget::OnScrollEvent(event);
}

void StatusAreaWidget::AddTrayButton(TrayBackgroundView* tray_button) {
  status_area_widget_delegate_->AddChildView(tray_button);
  tray_buttons_.push_back(tray_button);
}

StatusAreaWidget::LayoutInputs StatusAreaWidget::GetLayoutInputs() const {
  unsigned int child_visibility_bitmask = 0;
  DCHECK(tray_buttons_.size() <
         std::numeric_limits<decltype(child_visibility_bitmask)>::digits);
  for (unsigned int i = 0; i < tray_buttons_.size(); ++i) {
    if (tray_buttons_[i]->GetVisible())
      child_visibility_bitmask |= 1 << i;
  }

  bool should_animate = true;

  // Do not animate when tray items are added and removed (See
  // crbug.com/1067199).
  if (layout_inputs_) {
    const bool is_horizontal_alignment = shelf_->IsHorizontalAlignment();
    const gfx::Rect current_bounds = layout_inputs_->bounds;
    if ((is_horizontal_alignment &&
         current_bounds.width() != target_bounds_.width()) ||
        (!is_horizontal_alignment &&
         current_bounds.height() != target_bounds_.height())) {
      should_animate = false;
    }
  }

  return {target_bounds_, CalculateCollapseState(),
          shelf_->shelf_layout_manager()->GetOpacity(),
          child_visibility_bitmask, should_animate};
}

}  // namespace ash
