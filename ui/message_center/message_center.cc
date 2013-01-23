// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"

namespace message_center {

//------------------------------------------------------------------------------

// static
MessageCenter* MessageCenter::GetInstance() {
  return Singleton<MessageCenter>::get();
}

MessageCenter::~MessageCenter() {
  notification_list_.reset();
}

void MessageCenter::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MessageCenter::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void MessageCenter::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void MessageCenter::SetMessageCenterVisible(bool visible) {
  notification_list_->SetMessageCenterVisible(visible);
}

size_t MessageCenter::NotificationCount() const {
  return notification_list_->NotificationCount();
}

size_t MessageCenter::UnreadNotificationCount() const {
  return notification_list_->unread_count();
}

bool MessageCenter::HasPopupNotifications() const {
  return notification_list_->HasPopupNotifications();
}

//------------------------------------------------------------------------------
// Client code interface.

void MessageCenter::AddNotification(
    ui::notifications::NotificationType type,
    const std::string& id,
    const string16& title,
    const string16& message,
    const string16& display_source,
    const std::string& extension_id,
    const base::DictionaryValue* optional_fields) {
  notification_list_->AddNotification(type, id, title, message, display_source,
                                      extension_id, optional_fields);
  NotifyMessageCenterChanged(true);
}

void MessageCenter::UpdateNotification(
    const std::string& old_id,
    const std::string& new_id,
    const string16& title,
    const string16& message,
    const base::DictionaryValue* optional_fields) {
  notification_list_->UpdateNotificationMessage(
      old_id, new_id, title, message, optional_fields);
  NotifyMessageCenterChanged(true);
}

void MessageCenter::RemoveNotification(const std::string& id) {
  if (!notification_list_->RemoveNotification(id))
    return;
  NotifyMessageCenterChanged(false);
}

void MessageCenter::SetNotificationPrimaryIcon(const std::string& id,
                                               const gfx::ImageSkia& image) {
  if (notification_list_->SetNotificationPrimaryIcon(id, image))
    NotifyMessageCenterChanged(true);
}

void MessageCenter::SetNotificationSecondaryIcon(const std::string& id,
                                                 const gfx::ImageSkia& image) {
  if (notification_list_->SetNotificationSecondaryIcon(id, image))
    NotifyMessageCenterChanged(true);
}

void MessageCenter::SetNotificationImage(const std::string& id,
                                         const gfx::ImageSkia& image) {
  if (notification_list_->SetNotificationImage(id, image))
    NotifyMessageCenterChanged(true);
}

//------------------------------------------------------------------------------
// Overridden from NotificationList::Delegate.

void MessageCenter::SendRemoveNotification(const std::string& id) {
  if (delegate_)
    delegate_->NotificationRemoved(id);
}

void MessageCenter::SendRemoveAllNotifications() {
  if (delegate_) {
    NotificationList::Notifications notifications;
    notification_list_->GetNotifications(&notifications);
    for (NotificationList::Notifications::const_iterator loopiter =
             notifications.begin();
         loopiter != notifications.end(); ) {
      NotificationList::Notifications::const_iterator curiter = loopiter++;
      std::string notification_id = curiter->id;
      // May call RemoveNotification and erase curiter.
      delegate_->NotificationRemoved(notification_id);
    }
  }
}

void MessageCenter::DisableNotificationByExtension(
    const std::string& id) {
  if (delegate_)
    delegate_->DisableExtension(id);
  // When we disable notifications, we remove any existing matching
  // notifications to avoid adding complicated UI to re-enable the source.
  notification_list_->SendRemoveNotificationsByExtension(id);
}

void MessageCenter::DisableNotificationByUrl(const std::string& id) {
  if (delegate_)
    delegate_->DisableNotificationsFromSource(id);
  notification_list_->SendRemoveNotificationsBySource(id);
}

void MessageCenter::ShowNotificationSettings(const std::string& id) {
  if (delegate_)
    delegate_->ShowSettings(id);
}

void MessageCenter::OnNotificationClicked(const std::string& id) {
  if (delegate_)
    delegate_->OnClicked(id);
  if (HasPopupNotifications()) {
    notification_list_->MarkSinglePopupAsShown(id, true);
    NotifyMessageCenterChanged(false);
  }
}

void MessageCenter::OnQuietModeChanged(bool quiet_mode) {
  NotifyMessageCenterChanged(true);
}

void MessageCenter::OnButtonClicked(const std::string& id, int button_index) {
  if (delegate_)
    delegate_->OnButtonClicked(id, button_index);
  if (HasPopupNotifications()) {
    notification_list_->MarkSinglePopupAsShown(id, true);
    NotifyMessageCenterChanged(false);
  }
}

NotificationList* MessageCenter::GetNotificationList() {
  return notification_list_.get();
}

void MessageCenter::Delegate::OnButtonClicked(const std::string& id,
                                              int button_index) {
}

//------------------------------------------------------------------------------
// Private.

MessageCenter::MessageCenter()
    : delegate_(NULL) {
  notification_list_.reset(new NotificationList(this));
}

void MessageCenter::NotifyMessageCenterChanged(bool new_notification) {
  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    OnMessageCenterChanged(new_notification));
}


}  // namespace message_center
