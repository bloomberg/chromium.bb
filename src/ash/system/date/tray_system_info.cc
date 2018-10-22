// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/date/tray_system_info.h"

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/date/date_view.h"
#include "ash/system/date/system_info_default_view.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"

namespace ash {

namespace tray {

TimeTrayItemView::TimeTrayItemView(SystemTrayItem* owner, Shelf* shelf)
    : TrayItemView(owner), session_observer_(this) {
  tray::TimeView::ClockLayout clock_layout =
      shelf->IsHorizontalAlignment()
          ? tray::TimeView::ClockLayout::HORIZONTAL_CLOCK
          : tray::TimeView::ClockLayout::VERTICAL_CLOCK;
  time_view_ = new tray::TimeView(clock_layout,
                                  Shell::Get()->system_tray_model()->clock());
  AddChildView(time_view_);
}

TimeTrayItemView::~TimeTrayItemView() = default;

void TimeTrayItemView::UpdateAlignmentForShelf(Shelf* shelf) {
  tray::TimeView::ClockLayout clock_layout =
      shelf->IsHorizontalAlignment()
          ? tray::TimeView::ClockLayout::HORIZONTAL_CLOCK
          : tray::TimeView::ClockLayout::VERTICAL_CLOCK;
  time_view_->UpdateClockLayout(clock_layout);
}

void TimeTrayItemView::OnSessionStateChanged(
    session_manager::SessionState state) {
  time_view_->SetTextColorBasedOnSession(state);
}

}  // namespace tray

TraySystemInfo::TraySystemInfo(SystemTray* system_tray)
    : SystemTrayItem(system_tray, SystemTrayItemUmaType::UMA_DATE) {}

TraySystemInfo::~TraySystemInfo() = default;

const tray::TimeView* TraySystemInfo::GetTimeTrayForTesting() const {
  return tray_view_->time_view();
}

views::View* TraySystemInfo::CreateTrayView(LoginStatus status) {
  CHECK(tray_view_ == nullptr);
  tray_view_ = new tray::TimeTrayItemView(this, system_tray()->shelf());
  return tray_view_;
}

views::View* TraySystemInfo::CreateDefaultView(LoginStatus status) {
  default_view_ = new SystemInfoDefaultView(this);
  return default_view_;
}

void TraySystemInfo::OnTrayViewDestroyed() {
  tray_view_ = nullptr;
}

void TraySystemInfo::OnDefaultViewDestroyed() {
  default_view_ = nullptr;
}

void TraySystemInfo::UpdateAfterShelfAlignmentChange() {
  if (tray_view_)
    tray_view_->UpdateAlignmentForShelf(system_tray()->shelf());
}

}  // namespace ash
