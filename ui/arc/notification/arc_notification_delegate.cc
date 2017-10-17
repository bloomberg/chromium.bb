// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_delegate.h"

#include "ui/arc/notification/arc_notification_content_view.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/arc/notification/arc_notification_view.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/message_view.h"

namespace arc {

ArcNotificationDelegate::ArcNotificationDelegate(
    base::WeakPtr<ArcNotificationItem> item)
    : item_(item) {
  DCHECK(item_);
}

ArcNotificationDelegate::~ArcNotificationDelegate() = default;

std::unique_ptr<message_center::MessageView>
ArcNotificationDelegate::CreateCustomMessageView(
    message_center::MessageCenterController* controller,
    const message_center::Notification& notification) {
  DCHECK(item_);
  DCHECK_EQ(item_->GetNotificationId(), notification.id());

  auto view = std::make_unique<ArcNotificationContentView>(item_.get());
  auto content_view_delegate = view->CreateContentViewDelegate();
  return std::make_unique<ArcNotificationView>(std::move(view),
                                               std::move(content_view_delegate),
                                               controller, notification);
}

void ArcNotificationDelegate::Close(bool by_user) {
  DCHECK(item_);
  item_->Close(by_user);
}

void ArcNotificationDelegate::Click() {
  DCHECK(item_);
  item_->Click();
}

bool ArcNotificationDelegate::SettingsClick() {
  DCHECK(item_);
  item_->OpenSettings();
  return true;
}

bool ArcNotificationDelegate::ShouldDisplaySettingsButton() {
  DCHECK(item_);
  return item_->IsOpeningSettingsSupported();
}

}  // namespace arc
