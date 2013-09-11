// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_tray.h"

#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/message_center_types.h"

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

  HidePopupBubbleInternal();

  message_center_visible_ = delegate_->ShowMessageCenter();
  message_center_->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);
  NotifyMessageCenterTrayChanged();
  return message_center_visible_;
}

bool MessageCenterTray::HideMessageCenterBubble() {
  if (!message_center_visible_)
    return false;
  delegate_->HideMessageCenter();
  MarkMessageCenterHidden();
  return true;
}

void MessageCenterTray::MarkMessageCenterHidden() {
  if (!message_center_visible_)
    return;
  message_center_visible_ = false;

  // Some notifications (like system ones) should appear as popups again
  // after the message center is closed.
  if (message_center_->HasPopupNotifications()) {
    ShowPopupBubble();
    return;
  }

  message_center_->SetVisibility(message_center::VISIBILITY_TRANSIENT);

  NotifyMessageCenterTrayChanged();
}

void MessageCenterTray::ToggleMessageCenterBubble() {
  if (message_center_visible_)
    HideMessageCenterBubble();
  else
    ShowMessageCenterBubble();
}

void MessageCenterTray::ShowPopupBubble() {
  if (message_center_visible_)
    return;

  if (popups_visible_) {
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
  HidePopupBubbleInternal();
  NotifyMessageCenterTrayChanged();

  return true;
}

void MessageCenterTray::HidePopupBubbleInternal() {
  if (!popups_visible_)
    return;

  delegate_->HidePopups();
  popups_visible_ = false;
}

void MessageCenterTray::ShowNotifierSettingsBubble() {
  if (popups_visible_)
    HidePopupBubbleInternal();

  message_center_visible_ = delegate_->ShowNotifierSettings();
  message_center_->SetVisibility(message_center::VISIBILITY_SETTINGS);

  NotifyMessageCenterTrayChanged();
}

void MessageCenterTray::OnNotificationAdded(
    const std::string& notification_id) {
  OnMessageCenterChanged();
}

void MessageCenterTray::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  OnMessageCenterChanged();
}

void MessageCenterTray::OnNotificationUpdated(
    const std::string& notification_id) {
  OnMessageCenterChanged();
}

void MessageCenterTray::OnNotificationClicked(
    const std::string& notification_id) {
  if (popups_visible_)
    OnMessageCenterChanged();
}

void MessageCenterTray::OnNotificationButtonClicked(
    const std::string& notification_id,
    int button_index) {
  if (popups_visible_)
    OnMessageCenterChanged();
}

void MessageCenterTray::OnNotificationDisplayed(
    const std::string& notification_id) {
  NotifyMessageCenterTrayChanged();
}

void MessageCenterTray::OnQuietModeChanged(bool in_quiet_mode) {
  NotifyMessageCenterTrayChanged();
}

void MessageCenterTray::OnMessageCenterChanged() {
  if (message_center_visible_ && message_center_->NotificationCount() == 0)
    HideMessageCenterBubble();

  if (popups_visible_ && !message_center_->HasPopupNotifications())
    HidePopupBubbleInternal();
  else if (!popups_visible_ && message_center_->HasPopupNotifications())
    ShowPopupBubble();

  NotifyMessageCenterTrayChanged();
}

void MessageCenterTray::NotifyMessageCenterTrayChanged() {
  delegate_->OnMessageCenterTrayChanged();
}

}  // namespace message_center
