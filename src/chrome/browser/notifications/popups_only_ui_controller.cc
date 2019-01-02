// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/popups_only_ui_controller.h"

#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"

PopupsOnlyUiController::PopupsOnlyUiController(
    std::unique_ptr<Delegate> delegate)
    : message_center_(message_center::MessageCenter::Get()),
      delegate_(std::move(delegate)) {
  message_center_->AddObserver(this);
  message_center_->SetHasMessageCenterView(false);
}

PopupsOnlyUiController::~PopupsOnlyUiController() {
  message_center_->RemoveObserver(this);
}

void PopupsOnlyUiController::OnNotificationAdded(
    const std::string& notification_id) {
  ShowOrHidePopupBubbles();
}

void PopupsOnlyUiController::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  ShowOrHidePopupBubbles();
}

void PopupsOnlyUiController::OnNotificationUpdated(
    const std::string& notification_id) {
  ShowOrHidePopupBubbles();
}

void PopupsOnlyUiController::OnNotificationClicked(
    const std::string& notification_id,
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  if (popups_visible_)
    ShowOrHidePopupBubbles();
}

void PopupsOnlyUiController::ShowOrHidePopupBubbles() {
  if (popups_visible_ && !message_center_->HasPopupNotifications()) {
    if (delegate_)
      delegate_->HidePopups();
    popups_visible_ = false;
  } else if (!popups_visible_ && message_center_->HasPopupNotifications()) {
    if (delegate_)
      delegate_->ShowPopups();
    popups_visible_ = true;
  }
}
