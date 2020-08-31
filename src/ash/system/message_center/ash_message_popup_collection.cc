// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_message_popup_collection.h"

#include "ash/focus_cycler.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/message_center/fullscreen_notification_blocker.h"
#include "ash/system/message_center/metrics_utils.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/i18n/rtl.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

const int kToastMarginX = 7;

}  // namespace

const char AshMessagePopupCollection::kMessagePopupWidgetName[] =
    "ash/message_center/MessagePopup";

AshMessagePopupCollection::AshMessagePopupCollection(Shelf* shelf)
    : screen_(nullptr), shelf_(shelf), tray_bubble_height_(0) {
  set_inverse();
  shelf_->AddObserver(this);
}

AshMessagePopupCollection::~AshMessagePopupCollection() {
  if (screen_)
    screen_->RemoveObserver(this);
  shelf_->RemoveObserver(this);
  for (views::Widget* widget : tracked_widgets_)
    widget->RemoveObserver(this);
}

void AshMessagePopupCollection::StartObserving(
    display::Screen* screen,
    const display::Display& display) {
  screen_ = screen;
  work_area_ = display.work_area();
  screen->AddObserver(this);
  if (tray_bubble_height_ > 0)
    UpdateWorkArea();
}

void AshMessagePopupCollection::SetTrayBubbleHeight(int height) {
  const int old_tray_bubble_height = tray_bubble_height_;

  tray_bubble_height_ = height;

  // If the shelf is shown during auto-hide state, the distance from the edge
  // should be reduced by the height of shelf's shown height.
  if (shelf_->GetVisibilityState() == SHELF_AUTO_HIDE &&
      shelf_->GetAutoHideState() == SHELF_AUTO_HIDE_SHOWN) {
    tray_bubble_height_ -= ShelfConfig::Get()->shelf_size();
  }

  if (tray_bubble_height_ > 0)
    tray_bubble_height_ += message_center::kMarginBetweenPopups;
  else
    tray_bubble_height_ = 0;

  if (old_tray_bubble_height != tray_bubble_height_)
    ResetBounds();
}

int AshMessagePopupCollection::GetToastOriginX(
    const gfx::Rect& toast_bounds) const {
  // In Ash, RTL UI language mirrors the whole ash layout, so the toast
  // widgets should be at the bottom-left instead of bottom right.
  if (base::i18n::IsRTL())
    return work_area_.x() + kToastMarginX;

  if (IsFromLeft())
    return work_area_.x() + kToastMarginX;
  return work_area_.right() - kToastMarginX - toast_bounds.width();
}

int AshMessagePopupCollection::GetBaseline() const {
  gfx::Insets tray_bubble_insets = GetTrayBubbleInsets();
  return work_area_.bottom() - tray_bubble_insets.bottom() -
         tray_bubble_height_;
}

gfx::Rect AshMessagePopupCollection::GetWorkArea() const {
  gfx::Rect work_area_without_tray_bubble = work_area_;
  work_area_without_tray_bubble.set_height(
      work_area_without_tray_bubble.height() - tray_bubble_height_);
  return work_area_without_tray_bubble;
}

bool AshMessagePopupCollection::IsTopDown() const {
  return false;
}

bool AshMessagePopupCollection::IsFromLeft() const {
  return GetAlignment() == ShelfAlignment::kLeft;
}

bool AshMessagePopupCollection::RecomputeAlignment(
    const display::Display& display) {
  // Nothing needs to be done.
  return false;
}

void AshMessagePopupCollection::ConfigureWidgetInitParamsForContainer(
    views::Widget* widget,
    views::Widget::InitParams* init_params) {
  init_params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  init_params->shadow_elevation = ::wm::kShadowElevationInactiveWindow;
  // On ash, popups go in the status container.
  init_params->parent = shelf_->GetWindow()->GetRootWindow()->GetChildById(
      kShellWindowId_ShelfContainer);

  // Make the widget activatable so it can receive focus when cycling through
  // windows (i.e. pressing ctrl + forward/back).
  init_params->activatable = views::Widget::InitParams::ACTIVATABLE_YES;
  init_params->name = kMessagePopupWidgetName;
  Shell::Get()->focus_cycler()->AddWidget(widget);
  widget->AddObserver(this);
  tracked_widgets_.insert(widget);
}

bool AshMessagePopupCollection::IsPrimaryDisplayForNotification() const {
  return screen_ &&
         GetCurrentDisplay().id() == screen_->GetPrimaryDisplay().id();
}

bool AshMessagePopupCollection::BlockForMixedFullscreen(
    const message_center::Notification& notification) const {
  return FullscreenNotificationBlocker::BlockForMixedFullscreen(
      notification, RootWindowController::ForWindow(shelf_->GetWindow())
                        ->IsInFullscreenMode());
}

void AshMessagePopupCollection::NotifyPopupAdded(
    message_center::MessagePopupView* popup) {
  MessagePopupCollection::NotifyPopupAdded(popup);
  popup->message_view()->AddObserver(this);
  metrics_utils::LogPopupShown(popup->message_view()->notification_id());
}

void AshMessagePopupCollection::NotifyPopupClosed(
    message_center::MessagePopupView* popup) {
  MessagePopupCollection::NotifyPopupClosed(popup);
  popup->message_view()->RemoveObserver(this);
}

void AshMessagePopupCollection::OnSlideOut(const std::string& notification_id) {
  metrics_utils::LogClosedByUser(notification_id, /*is_swipe=*/true,
                                 /*is_popup=*/true);
}

void AshMessagePopupCollection::OnCloseButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogClosedByUser(notification_id, /*is_swipe=*/false,
                                 /*is_popup=*/true);
}

void AshMessagePopupCollection::OnSettingsButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogSettingsShown(notification_id, /*is_slide_controls=*/false,
                                  /*is_popup=*/true);
}

void AshMessagePopupCollection::OnSnoozeButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogSnoozed(notification_id, /*is_slide_controls=*/false,
                            /*is_popup=*/true);
}

ShelfAlignment AshMessagePopupCollection::GetAlignment() const {
  return shelf_->alignment();
}

display::Display AshMessagePopupCollection::GetCurrentDisplay() const {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(
      shelf_->GetWindow());
}

void AshMessagePopupCollection::UpdateWorkArea() {
  gfx::Rect new_work_area =
      WorkAreaInsets::ForWindow(shelf_->GetWindow()->GetRootWindow())
          ->user_work_area_bounds();
  if (work_area_ == new_work_area)
    return;

  work_area_ = new_work_area;
  ResetBounds();
}

///////////////////////////////////////////////////////////////////////////////
// ShelfObserver:

void AshMessagePopupCollection::OnShelfWorkAreaInsetsChanged() {
  UpdateWorkArea();
}

///////////////////////////////////////////////////////////////////////////////
// display::DisplayObserver:

void AshMessagePopupCollection::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (GetCurrentDisplay().id() == display.id())
    UpdateWorkArea();
}

///////////////////////////////////////////////////////////////////////////////
// views::WidgetObserver:

void AshMessagePopupCollection::OnWidgetClosing(views::Widget* widget) {
  Shell::Get()->focus_cycler()->RemoveWidget(widget);
  widget->RemoveObserver(this);
  tracked_widgets_.erase(widget);
}

void AshMessagePopupCollection::OnWidgetActivationChanged(views::Widget* widget,
                                                          bool active) {
  // Note: Each pop-up is contained in it's own widget and we need to manually
  // focus the contained MessageView when the widget is activated through the
  // FocusCycler.
  if (active && Shell::Get()->focus_cycler()->widget_activating() == widget)
    widget->GetFocusManager()->SetFocusedView(widget->GetContentsView());
}

}  // namespace ash
