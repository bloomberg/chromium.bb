// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_menu_view.h"

#include "ash/app_menu/notification_item_view.h"
#include "ash/app_menu/notification_menu_header_view.h"
#include "ash/app_menu/notification_overflow_view.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_separator.h"

namespace ash {

NotificationMenuView::NotificationMenuView(
    Delegate* notification_item_view_delegate,
    message_center::SlideOutController::Delegate* slide_out_controller_delegate,
    const std::string& app_id)
    : app_id_(app_id),
      notification_item_view_delegate_(notification_item_view_delegate),
      slide_out_controller_delegate_(slide_out_controller_delegate),
      double_separator_(
          new views::MenuSeparator(ui::MenuSeparatorType::DOUBLE_SEPARATOR)),
      header_view_(new NotificationMenuHeaderView()) {
  DCHECK(notification_item_view_delegate_);
  DCHECK(slide_out_controller_delegate_);
  DCHECK(!app_id_.empty())
      << "Only context menus for applications can show notifications.";

  AddChildView(double_separator_);
  AddChildView(header_view_);
}

NotificationMenuView::~NotificationMenuView() = default;

bool NotificationMenuView::IsEmpty() const {
  return notification_item_views_.empty();
}

gfx::Size NotificationMenuView::CalculatePreferredSize() const {
  return gfx::Size(
      views::MenuConfig::instance().touchable_menu_width,
      double_separator_->GetPreferredSize().height() +
          header_view_->GetPreferredSize().height() +
          kNotificationItemViewHeight +
          (overflow_view_ ? overflow_view_->GetPreferredSize().height() : 0));
}

void NotificationMenuView::Layout() {
  int y = 0;
  double_separator_->SetBoundsRect(
      gfx::Rect(gfx::Point(0, y),
                gfx::Size(views::MenuConfig::instance().touchable_menu_width,
                          double_separator_->GetPreferredSize().height())));
  y += double_separator_->GetPreferredSize().height();

  header_view_->SetBoundsRect(
      gfx::Rect(gfx::Point(0, y), header_view_->GetPreferredSize()));
  y += header_view_->GetPreferredSize().height();

  if (!notification_item_views_.empty()) {
    notification_item_views_.front()->SetBoundsRect(
        gfx::Rect(gfx::Point(0, y),
                  notification_item_views_.front()->GetPreferredSize()));
    y += notification_item_views_.front()->GetPreferredSize().height();
  }

  if (overflow_view_) {
    overflow_view_->SetBoundsRect(
        gfx::Rect(gfx::Point(0, y), overflow_view_->GetPreferredSize()));
  }
}

void NotificationMenuView::AddNotificationItemView(
    const message_center::Notification& notification) {
  NotificationItemView* old_displayed_notification_item_view =
      notification_item_views_.empty() ? nullptr
                                       : notification_item_views_.front().get();

  notification_item_views_.push_front(std::make_unique<NotificationItemView>(
      notification_item_view_delegate_, slide_out_controller_delegate_,
      notification.title(), notification.message(), notification.icon(),
      notification.id()));
  notification_item_views_.front()->set_owned_by_client();
  AddChildView(notification_item_views_.front().get());

  header_view_->UpdateCounter(notification_item_views_.size());

  if (!old_displayed_notification_item_view)
    return;

  // Push |old_displayed_notification_item_view| to |overflow_view_|.
  RemoveChildView(old_displayed_notification_item_view);

  const bool overflow_view_created = !overflow_view_;
  if (!overflow_view_) {
    overflow_view_ = std::make_unique<NotificationOverflowView>();
    overflow_view_->set_owned_by_client();
    AddChildView(overflow_view_.get());
  }
  overflow_view_->AddIcon(
      old_displayed_notification_item_view->proportional_image_view(),
      old_displayed_notification_item_view->notification_id());

  if (overflow_view_created) {
    PreferredSizeChanged();
    // OnOverflowAddedOrRemoved must be called after PreferredSizeChange to
    // ensure that enough room is allocated for the overflow view.
    notification_item_view_delegate_->OnOverflowAddedOrRemoved();
  }
  Layout();
}
void NotificationMenuView::UpdateNotificationItemView(
    const message_center::Notification& notification) {
  // Find the view which corresponds to |notification|.
  auto notification_iter = std::find_if(
      notification_item_views_.begin(), notification_item_views_.end(),
      [&notification](
          const std::unique_ptr<NotificationItemView>& notification_item_view) {
        return notification_item_view->notification_id() == notification.id();
      });

  if (notification_iter == notification_item_views_.end())
    return;

  (*notification_iter)
      ->UpdateContents(notification.title(), notification.message(),
                       notification.icon());
}

void NotificationMenuView::OnNotificationRemoved(
    const std::string& notification_id) {
  // Find the view which corresponds to |notification_id|.
  auto notification_iter = std::find_if(
      notification_item_views_.begin(), notification_item_views_.end(),
      [&notification_id](
          const std::unique_ptr<NotificationItemView>& notification_item_view) {
        return notification_item_view->notification_id() == notification_id;
      });
  if (notification_iter == notification_item_views_.end())
    return;

  // Erase the notification from |notification_item_views_| and
  // |overflow_view_|.
  notification_item_views_.erase(notification_iter);
  if (overflow_view_)
    overflow_view_->RemoveIcon(notification_id);
  header_view_->UpdateCounter(notification_item_views_.size());

  // Display the next notification.
  if (!notification_item_views_.empty()) {
    AddChildView(notification_item_views_.front().get());
    if (overflow_view_) {
      overflow_view_->RemoveIcon(
          notification_item_views_.front()->notification_id());
    }
  }

  if (overflow_view_ && overflow_view_->is_empty()) {
    overflow_view_.reset();
    PreferredSizeChanged();
    notification_item_view_delegate_->OnOverflowAddedOrRemoved();
  }
}

ui::Layer* NotificationMenuView::GetSlideOutLayer() {
  if (notification_item_views_.empty())
    return nullptr;
  return notification_item_views_.front()->layer();
}

const std::string& NotificationMenuView::GetDisplayedNotificationID() {
  DCHECK(!notification_item_views_.empty());
  return notification_item_views_.front()->notification_id();
}

}  // namespace ash
