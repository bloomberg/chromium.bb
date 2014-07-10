// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/desktop_popup_alignment_delegate.h"

#include "ui/gfx/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/message_popup_collection.h"

namespace message_center {

DesktopPopupAlignmentDelegate::DesktopPopupAlignmentDelegate()
    : alignment_(POPUP_ALIGNMENT_BOTTOM | POPUP_ALIGNMENT_RIGHT),
      display_id_(gfx::Display::kInvalidDisplayID),
      screen_(NULL) {
}

DesktopPopupAlignmentDelegate::~DesktopPopupAlignmentDelegate() {
  if (screen_)
    screen_->RemoveObserver(this);
}

void DesktopPopupAlignmentDelegate::StartObserving(gfx::Screen* screen) {
  if (screen_ || !screen)
    return;

  screen_ = screen;
  screen_->AddObserver(this);
  gfx::Display display = screen_->GetPrimaryDisplay();
  display_id_ = display.id();
  RecomputeAlignment(display);
}

int DesktopPopupAlignmentDelegate::GetToastOriginX(
    const gfx::Rect& toast_bounds) const {
  if (IsFromLeft())
    return work_area_.x() + kMarginBetweenItems;
  return work_area_.right() - kMarginBetweenItems - toast_bounds.width();
}

int DesktopPopupAlignmentDelegate::GetBaseLine() const {
  return IsTopDown()
      ? work_area_.y() + kMarginBetweenItems
      : work_area_.bottom() - kMarginBetweenItems;
}

int DesktopPopupAlignmentDelegate::GetWorkAreaBottom() const {
  return work_area_.bottom();
}

bool DesktopPopupAlignmentDelegate::IsTopDown() const {
  return (alignment_ & POPUP_ALIGNMENT_TOP) != 0;
}

bool DesktopPopupAlignmentDelegate::IsFromLeft() const {
  return (alignment_ & POPUP_ALIGNMENT_LEFT) != 0;
}

void DesktopPopupAlignmentDelegate::RecomputeAlignment(
    const gfx::Display& display) {
  if (work_area_ == display.work_area())
    return;

  work_area_ = display.work_area();

  // If the taskbar is at the top, render notifications top down. Some platforms
  // like Gnome can have taskbars at top and bottom. In this case it's more
  // likely that the systray is on the top one.
  alignment_ = work_area_.y() > display.bounds().y() ? POPUP_ALIGNMENT_TOP
                                                     : POPUP_ALIGNMENT_BOTTOM;

  // If the taskbar is on the left show the notifications on the left. Otherwise
  // show it on right since it's very likely that the systray is on the right if
  // the taskbar is on the top or bottom.
  // Since on some platforms like Ubuntu Unity there's also a launcher along
  // with a taskbar (panel), we need to check that there is really nothing at
  // the top before concluding that the taskbar is at the left.
  alignment_ |= (work_area_.x() > display.bounds().x() &&
                 work_area_.y() == display.bounds().y())
      ? POPUP_ALIGNMENT_LEFT
      : POPUP_ALIGNMENT_RIGHT;
}

void DesktopPopupAlignmentDelegate::OnDisplayAdded(
    const gfx::Display& new_display) {
}

void DesktopPopupAlignmentDelegate::OnDisplayRemoved(
    const gfx::Display& old_display) {
}

void DesktopPopupAlignmentDelegate::OnDisplayMetricsChanged(
    const gfx::Display& display,
    uint32_t metrics) {
  if (display.id() == display_id_) {
    RecomputeAlignment(display);
    DoUpdateIfPossible();
  }
}

}  // namespace message_center
