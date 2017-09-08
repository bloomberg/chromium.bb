// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_tray.h"

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/strings/grit/ui_strings.h"

namespace message_center {

namespace {

// Menu constants
const int kTogglePermissionCommand = 0;

#if defined(OS_CHROMEOS)
const int kShowSettingsCommand = 1;
#endif

// The model of the context menu for a notification card.
class NotificationMenuModel : public ui::SimpleMenuModel,
                              public ui::SimpleMenuModel::Delegate {
 public:
  NotificationMenuModel(MessageCenterTray* tray,
                        const NotifierId& notifier_id,
                        const base::string16& display_source);
  ~NotificationMenuModel() override;

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  MessageCenterTray* tray_;
  NotifierId notifier_id_;
  DISALLOW_COPY_AND_ASSIGN(NotificationMenuModel);
};

NotificationMenuModel::NotificationMenuModel(
    MessageCenterTray* tray,
    const NotifierId& notifier_id,
    const base::string16& display_source)
    : ui::SimpleMenuModel(this),
      tray_(tray),
      notifier_id_(notifier_id) {
  if (!display_source.empty()) {
    AddItem(kTogglePermissionCommand,
            l10n_util::GetStringFUTF16(IDS_MESSAGE_CENTER_NOTIFIER_DISABLE,
                                       display_source));
  }

#ifdef OS_CHROMEOS
  // Add settings menu item.
  AddItem(kShowSettingsCommand,
          l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_SETTINGS));
#endif
}

NotificationMenuModel::~NotificationMenuModel() {
}

bool NotificationMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool NotificationMenuModel::IsCommandIdEnabled(int command_id) const {
  return tray_->delegate()->IsContextMenuEnabled();
}

void NotificationMenuModel::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case kTogglePermissionCommand:
      tray_->message_center()->DisableNotificationsByNotifier(notifier_id_);
      break;
#ifdef OS_CHROMEOS
    case kShowSettingsCommand:
      tray_->ShowNotifierSettingsBubble();
      break;
#endif
    default:
      NOTREACHED();
  }
}

}  // namespace

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
  if (message_center_visible_) {
    message_center_->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);
    NotifyMessageCenterTrayChanged();
  }
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
  message_center_->SetVisibility(message_center::VISIBILITY_TRANSIENT);

  // Some notifications (like system ones) should appear as popups again
  // after the message center is closed.
  if (message_center_->HasPopupNotifications()) {
    ShowPopupBubble();
    return;
  }

  NotifyMessageCenterTrayChanged();
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

std::unique_ptr<ui::MenuModel> MessageCenterTray::CreateNotificationMenuModel(
    const NotifierId& notifier_id,
    const base::string16& display_source) {
#if !defined(OS_CHROMEOS)
  // Only web pages are configurable on non-chromeos platforms.
  if (notifier_id.type != NotifierId::WEB_PAGE) {
    return nullptr;
  }
#endif

  return std::make_unique<NotificationMenuModel>(this, notifier_id,
                                                 display_source);
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

void MessageCenterTray::OnNotificationSettingsClicked(bool handled) {
  if (!handled)
    ShowNotifierSettingsBubble();
}

void MessageCenterTray::OnNotificationDisplayed(
    const std::string& notification_id,
    const DisplaySource source) {
  NotifyMessageCenterTrayChanged();
}

void MessageCenterTray::OnQuietModeChanged(bool in_quiet_mode) {
  NotifyMessageCenterTrayChanged();
}

void MessageCenterTray::OnBlockingStateChanged(NotificationBlocker* blocker) {
  OnMessageCenterChanged();
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
