// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_ash_notification_view.h"

#include "ash/capture_mode/capture_mode_util.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace ash {

CaptureModeAshNotificationView::CaptureModeAshNotificationView(
    const message_center::Notification& notification,
    CaptureModeType capture_type,
    bool shown_in_popup)
    : AshNotificationView(notification, shown_in_popup),
      capture_type_(capture_type) {
  // Creates the extra view which will depend on the type of the notification.
  if (!notification.image().IsEmpty())
    CreateExtraView();

  // Observes image container to make changes to the extra view if necessary.
  image_container_view()->AddObserver(this);
}

CaptureModeAshNotificationView::~CaptureModeAshNotificationView() = default;

// static
std::unique_ptr<message_center::MessageView>
CaptureModeAshNotificationView::CreateForImage(
    const message_center::Notification& notification,
    bool shown_in_popup) {
  return std::make_unique<CaptureModeAshNotificationView>(
      notification, CaptureModeType::kImage, shown_in_popup);
}

// static
std::unique_ptr<message_center::MessageView>
CaptureModeAshNotificationView::CreateForVideo(
    const message_center::Notification& notification,
    bool shown_in_popup) {
  return std::make_unique<CaptureModeAshNotificationView>(
      notification, CaptureModeType::kVideo, shown_in_popup);
}

void CaptureModeAshNotificationView::Layout() {
  AshNotificationView::Layout();
  if (!extra_view_)
    return;

  gfx::Rect extra_view_bounds = image_container_view()->GetContentsBounds();

  if (capture_type_ == CaptureModeType::kImage) {
    // The extra view in this case is a banner laid out at the bottom of the
    // image container.
    extra_view_bounds.set_y(extra_view_bounds.bottom() -
                            capture_mode_util::kBannerHeightDip);
    extra_view_bounds.set_height(capture_mode_util::kBannerHeightDip);
  } else {
    DCHECK_EQ(capture_type_, CaptureModeType::kVideo);
    // The extra view in this case is a play icon centered in the view.
    extra_view_bounds.ClampToCenteredSize(capture_mode_util::kPlayIconViewSize);
  }

  extra_view_->SetBoundsRect(extra_view_bounds);
}

void CaptureModeAshNotificationView::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  if (observed_view == image_container_view() &&
      starting_view == image_container_view()) {
    if (!image_container_view()->GetVisible())
      extra_view_ = nullptr;
    else if (image_container_view()->children().empty())
      CreateExtraView();
  }
}

void CaptureModeAshNotificationView::OnViewIsDeleting(View* observed_view) {
  DCHECK_EQ(observed_view, image_container_view());
  views::View::RemoveObserver(this);
}

void CaptureModeAshNotificationView::CreateExtraView() {
  DCHECK(image_container_view());
  DCHECK(!image_container_view()->children().empty());
  DCHECK(!extra_view_);
  extra_view_ = image_container_view()->AddChildView(
      capture_type_ == CaptureModeType::kImage
          ? capture_mode_util::CreateBannerView()
          : capture_mode_util::CreatePlayIconView());
}

}  // namespace ash
