// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_tray.h"

#include "base/observer_list.h"
#include "base/utf_string_conversions.h"
#include "ui/message_center/message_center_tray_delegate.h"

namespace message_center {

MessageCenterTray::MessageCenterTray(
    MessageCenterTrayDelegate* delegate,
    message_center::MessageCenter* message_center)
    : message_center_(message_center),
      message_center_visible_(false),
      popups_visible_(false),
      delegate_(delegate) {
  message_center_->AddObserver(this);
}

MessageCenterTray::~MessageCenterTray() {
  message_center_->RemoveObserver(this);
}

bool MessageCenterTray::ShowMessageCenterBubble() {
  if (message_center_visible_)
    return true;

  HidePopupBubble();

  message_center_visible_ = delegate_->ShowMessageCenter();
  message_center_->SetMessageCenterVisible(message_center_visible_);
  return message_center_visible_;
}

bool MessageCenterTray::HideMessageCenterBubble() {
  if (!message_center_visible_)
    return false;
  delegate_->HideMessageCenter();
  message_center_visible_ = false;
  message_center_->SetMessageCenterVisible(false);
  NotifyMessageCenterTrayChanged();
  return true;
}

void MessageCenterTray::ToggleMessageCenterBubble() {
  if (message_center_visible_)
    HideMessageCenterBubble();
  else
    ShowMessageCenterBubble();
}

void MessageCenterTray::ShowPopupBubble() {
  if (message_center_visible_) {
    // We don't want to show popups if the user is already looking at the
    // message center.  Instead, update it.
    delegate_->UpdateMessageCenter();
    return;
  }

  if (popups_visible_) {
    delegate_->UpdatePopups();
    NotifyMessageCenterTrayChanged();
    return;
  }

  if (!message_center_->HasPopupNotifications())
    return;

  popups_visible_ = delegate_->ShowPopups();
  NotifyMessageCenterTrayChanged();
}

bool MessageCenterTray::HidePopupBubble() {
  if (!popups_visible_)
    return false;

  delegate_->HidePopups();
  popups_visible_ = false;
  NotifyMessageCenterTrayChanged();

  return true;
}

void MessageCenterTray::OnMessageCenterChanged(bool new_notification) {
  if (message_center_visible_) {
    if (message_center_->NotificationCount() == 0)
      HideMessageCenterBubble();
    else
      delegate_->UpdateMessageCenter();
  }
  if (popups_visible_) {
    if (message_center_->NotificationCount() == 0)
      HidePopupBubble();
    else
      delegate_->UpdatePopups();
  }

  if (new_notification)
    ShowPopupBubble();

  NotifyMessageCenterTrayChanged();
}

void MessageCenterTray::NotifyMessageCenterTrayChanged() {
  delegate_->OnMessageCenterTrayChanged();
}

}  // namespace message_center
