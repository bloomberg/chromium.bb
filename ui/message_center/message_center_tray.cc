// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_tray.h"

#include "base/observer_list.h"
#include "base/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center_tray_delegate.h"

namespace message_center {
namespace {

// Menu commands
const int kToggleQuietMode = 0;
const int kEnableQuietModeHour = 1;
const int kEnableQuietModeDay = 2;

}

#if !defined(TOOLKIT_VIEWS)
MESSAGE_CENTER_EXPORT MessageCenterTrayDelegate* CreateMessageCenterTray() {
  NOTIMPLEMENTED();
  return NULL;
}
#endif

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

ui::MenuModel* MessageCenterTray::CreateQuietModeMenu() {
  ui::SimpleMenuModel* menu = new ui::SimpleMenuModel(this);

  menu->AddCheckItem(kToggleQuietMode,
                     l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_QUIET_MODE));
  menu->AddItem(kEnableQuietModeHour,
                l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_QUIET_MODE_1HOUR));
  menu->AddItem(kEnableQuietModeDay,
                l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_QUIET_MODE_1DAY));
  return menu;
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

bool MessageCenterTray::IsCommandIdChecked(int command_id) const {
  if (command_id != kToggleQuietMode)
    return false;
  return message_center()->quiet_mode();
}

bool MessageCenterTray::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool MessageCenterTray::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void MessageCenterTray::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == kToggleQuietMode) {
    bool in_quiet_mode = message_center()->quiet_mode();
    message_center()->notification_list()->SetQuietMode(!in_quiet_mode);
    return;
  }
  base::TimeDelta expires_in = command_id == kEnableQuietModeDay ?
      base::TimeDelta::FromDays(1):
      base::TimeDelta::FromHours(1);
  message_center()->notification_list()->EnterQuietModeWithExpire(expires_in);
}

void MessageCenterTray::NotifyMessageCenterTrayChanged() {
  delegate_->OnMessageCenterTrayChanged();
}

}  // namespace message_center
