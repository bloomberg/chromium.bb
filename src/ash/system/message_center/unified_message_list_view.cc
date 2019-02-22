// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_list_view.h"

#include "ash/system/message_center/new_unified_message_center_view.h"
#include "ash/system/message_center/notification_swipe_control_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/auto_reset.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

using message_center::Notification;
using message_center::MessageCenter;
using message_center::MessageView;

namespace ash {

// Container view of notification and swipe control.
// All children of UnifiedMessageListView should be MessageViewContainer.
class UnifiedMessageListView::MessageViewContainer
    : public views::View,
      public MessageView::SlideObserver {
 public:
  explicit MessageViewContainer(MessageView* message_view)
      : message_view_(message_view),
        control_view_(new NotificationSwipeControlView(message_view)) {
    message_view_->AddSlideObserver(this);

    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(control_view_);
    AddChildView(message_view_);
  }

  ~MessageViewContainer() override = default;

  // Update the border and background corners based on if the notification is
  // at the top or the bottom.
  void UpdateBorder(bool is_top, bool is_bottom) {
    message_view_->SetBorder(
        is_bottom ? views::NullBorder()
                  : views::CreateSolidSidedBorder(
                        0, 0, kUnifiedNotificationSeparatorThickness, 0,
                        kUnifiedNotificationSeparatorColor));
    const int top_radius = is_top ? kUnifiedTrayCornerRadius : 0;
    const int bottom_radius = is_bottom ? kUnifiedTrayCornerRadius : 0;
    message_view_->UpdateCornerRadius(top_radius, bottom_radius);
    control_view_->UpdateCornerRadius(top_radius, bottom_radius);
  }

  // Collapses the notification if its state haven't changed manually by a user.
  void Collapse() {
    if (!message_view_->IsManuallyExpandedOrCollapsed())
      message_view_->SetExpanded(false);
  }

  std::string GetNotificationId() const {
    return message_view_->notification_id();
  }

  void UpdateWithNotification(const Notification& notification) {
    message_view_->UpdateWithNotification(notification);
  }

  void CloseSwipeControl() { message_view_->CloseSwipeControl(); }

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  // MessageView::SlideObserver:
  void OnSlideChanged(const std::string& notification_id) override {
    control_view_->UpdateButtonsVisibility();
  }

 private:
  MessageView* const message_view_;
  NotificationSwipeControlView* const control_view_;

  DISALLOW_COPY_AND_ASSIGN(MessageViewContainer);
};

UnifiedMessageListView::UnifiedMessageListView(
    NewUnifiedMessageCenterView* message_center_view)
    : message_center_view_(message_center_view) {
  MessageCenter::Get()->AddObserver(this);

  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
}

UnifiedMessageListView::~UnifiedMessageListView() {
  MessageCenter::Get()->RemoveObserver(this);
}

void UnifiedMessageListView::Init() {
  bool is_latest = true;
  for (auto* notification : MessageCenter::Get()->GetVisibleNotifications()) {
    auto* view = CreateMessageView(*notification);
    // Expand the latest notification, and collapse all other notifications.
    view->SetExpanded(is_latest && view->IsAutoExpandingAllowed());
    is_latest = false;
    AddChildViewAt(new MessageViewContainer(view), 0);
    MessageCenter::Get()->DisplayedNotification(
        notification->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
  }
  UpdateBorders();
}

int UnifiedMessageListView::GetLastNotificationHeight() const {
  if (!has_children())
    return 0;
  return child_at(child_count() - 1)->bounds().height();
}

void UnifiedMessageListView::ChildPreferredSizeChanged(views::View* child) {
  if (ignore_size_change_)
    return;
  PreferredSizeChanged();
}

void UnifiedMessageListView::PreferredSizeChanged() {
  views::View::PreferredSizeChanged();
  if (message_center_view_)
    message_center_view_->ListPreferredSizeChanged();
}

void UnifiedMessageListView::OnNotificationAdded(const std::string& id) {
  auto* notification = MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification)
    return;

  // Collapse all notifications before adding new one.
  CollapseAllNotifications();

  auto* view = CreateMessageView(*notification);
  // Expand the latest notification.
  view->SetExpanded(view->IsAutoExpandingAllowed());
  AddChildView(new MessageViewContainer(view));
  UpdateBorders();
  PreferredSizeChanged();
}

void UnifiedMessageListView::OnNotificationRemoved(const std::string& id,
                                                   bool by_user) {
  for (int i = 0; i < child_count(); ++i) {
    auto* view = GetContainer(i);
    if (view->GetNotificationId() == id) {
      delete view;
      break;
    }
  }

  UpdateBorders();
  PreferredSizeChanged();
}

void UnifiedMessageListView::OnNotificationUpdated(const std::string& id) {
  auto* notification = MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification)
    return;

  for (int i = 0; i < child_count(); ++i) {
    auto* view = GetContainer(i);
    if (view->GetNotificationId() == id) {
      view->UpdateWithNotification(*notification);
      break;
    }
  }

  PreferredSizeChanged();
}

void UnifiedMessageListView::OnSlideChanged(
    const std::string& notification_id) {
  // When the swipe control for |notification_id| is shown, hide all other swipe
  // controls.
  for (int i = 0; i < child_count(); ++i) {
    auto* view = GetContainer(i);
    if (view->GetNotificationId() == notification_id)
      continue;
    view->CloseSwipeControl();
  }
}

MessageView* UnifiedMessageListView::CreateMessageView(
    const Notification& notification) {
  auto* view = message_center::MessageViewFactory::Create(notification);
  view->SetIsNested();
  view->AddSlideObserver(this);
  message_center_view_->ConfigureMessageView(view);
  return view;
}

UnifiedMessageListView::MessageViewContainer*
UnifiedMessageListView::GetContainer(int index) {
  return static_cast<MessageViewContainer*>(child_at(index));
}

void UnifiedMessageListView::CollapseAllNotifications() {
  base::AutoReset<bool> auto_reset(&ignore_size_change_, true);
  for (int i = 0; i < child_count(); ++i)
    GetContainer(i)->Collapse();
}

void UnifiedMessageListView::UpdateBorders() {
  for (int i = 0; i < child_count(); ++i) {
    const bool is_top = i == 0;
    const bool is_bottom = i == child_count() - 1;
    GetContainer(i)->UpdateBorder(is_top, is_bottom);
  }
}

}  // namespace ash
