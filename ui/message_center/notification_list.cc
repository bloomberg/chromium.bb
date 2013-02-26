// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_list.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "base/values.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"

namespace message_center {

bool ComparePriorityTimestampSerial::operator()(Notification* n1,
                                                Notification* n2) {
  if (n1->priority() > n2->priority())  // Higher pri go first.
    return true;
  if (n1->priority() < n2->priority())
    return false;
  return CompareTimestampSerial()(n1, n2);
}

bool CompareTimestampSerial::operator()(Notification* n1, Notification* n2) {
  if (n1->timestamp() > n2->timestamp())  // Newer come first.
    return true;
  if (n1->timestamp() < n2->timestamp())
    return false;
  if (n1->serial_number() > n2->serial_number())  // Newer come first.
    return true;
  if (n1->serial_number() < n2->serial_number())
    return false;
  return false;
}

const size_t NotificationList::kMaxVisibleMessageCenterNotifications = 100;
const size_t NotificationList::kMaxVisiblePopupNotifications = 2;

NotificationList::NotificationList(Delegate* delegate)
    : delegate_(delegate),
      message_center_visible_(false),
      unread_count_(0),
      quiet_mode_(false) {
}

NotificationList::~NotificationList() {
  STLDeleteContainerPointers(notifications_.begin(), notifications_.end());
}

void NotificationList::SetMessageCenterVisible(bool visible) {
  if (message_center_visible_ == visible)
    return;
  message_center_visible_ = visible;
  // When the center appears, mark all notifications as shown, and
  // when the center is hidden, clear the unread count, and mark all
  // notifications as read.
  if (!visible)
    unread_count_ = 0;

  for (Notifications::iterator iter = notifications_.begin();
       iter != notifications_.end(); ++iter) {
    if (visible)
      (*iter)->set_shown_as_popup(true);
    else
      (*iter)->set_is_read(true);
  }
}

void NotificationList::AddNotification(
    NotificationType type,
    const std::string& id,
    const string16& title,
    const string16& message,
    const string16& display_source,
    const std::string& extension_id,
    const DictionaryValue* optional_fields) {
  scoped_ptr<Notification> notification(
      new Notification(type, id, title, message, display_source, extension_id,
          optional_fields));
  PushNotification(notification.Pass());
}

void NotificationList::UpdateNotificationMessage(
    const std::string& old_id,
    const std::string& new_id,
    const string16& title,
    const string16& message,
    const base::DictionaryValue* optional_fields) {
  Notifications::iterator iter = GetNotification(old_id);
  if (iter == notifications_.end())
    return;

  // Copy and update a notification. It has an effect of setting a new timestamp
  // if not overridden by optional_fields
  scoped_ptr<Notification> notification(
      new Notification((*iter)->type(),
                       new_id,
                       title,
                       message,
                       (*iter)->display_source(),
                       (*iter)->extension_id(),
                       optional_fields));
  EraseNotification(iter);
  PushNotification(notification.Pass());
}

void NotificationList::RemoveNotification(const std::string& id) {
  EraseNotification(GetNotification(id));
}

void NotificationList::RemoveAllNotifications() {
  for (Notifications::iterator loopiter = notifications_.begin();
       loopiter != notifications_.end(); ) {
    Notifications::iterator curiter = loopiter++;
    EraseNotification(curiter);
  }
  unread_count_ = 0;
}

void NotificationList::SendRemoveNotificationsBySource(
    const std::string& id) {
  Notifications::iterator source_iter = GetNotification(id);
  if (source_iter == notifications_.end())
    return;
  string16 display_source = (*source_iter)->display_source();

  for (Notifications::iterator loopiter = notifications_.begin();
       loopiter != notifications_.end(); ) {
    Notifications::iterator curiter = loopiter++;
    if ((*curiter)->display_source() == display_source)
      delegate_->SendRemoveNotification((*curiter)->id());
  }
}

void NotificationList::SendRemoveNotificationsByExtension(
    const std::string& id) {
  Notifications::iterator source_iter = GetNotification(id);
  if (source_iter == notifications_.end())
    return;
  std::string extension_id = (*source_iter)->extension_id();
  for (Notifications::iterator loopiter = notifications_.begin();
       loopiter != notifications_.end(); ) {
    Notifications::iterator curiter = loopiter++;
    if ((*curiter)->extension_id() == extension_id)
      delegate_->SendRemoveNotification((*curiter)->id());
  }
}

bool NotificationList::SetNotificationIcon(const std::string& notification_id,
                                           const gfx::ImageSkia& image) {
  Notifications::iterator iter = GetNotification(notification_id);
  if (iter == notifications_.end())
    return false;
  (*iter)->set_primary_icon(image);
  return true;
}

bool NotificationList::SetNotificationImage(const std::string& notification_id,
                                            const gfx::ImageSkia& image) {
  Notifications::iterator iter = GetNotification(notification_id);
  if (iter == notifications_.end())
    return false;
  (*iter)->set_image(image);
  return true;
}

bool NotificationList::SetNotificationButtonIcon(
    const std::string& notification_id, int button_index,
    const gfx::ImageSkia& image) {
  Notifications::iterator iter = GetNotification(notification_id);
  if (iter == notifications_.end())
    return false;
  return (*iter)->SetButtonIcon(button_index, image);
}

bool NotificationList::HasNotification(const std::string& id) {
  return GetNotification(id) != notifications_.end();
}

bool NotificationList::HasPopupNotifications() {
  for (Notifications::iterator iter = notifications_.begin();
       iter != notifications_.end(); ++iter) {
    if ((*iter)->priority() < DEFAULT_PRIORITY)
      break;
    if (!(*iter)->shown_as_popup())
      return true;
  }
  return false;
}

NotificationList::PopupNotifications NotificationList::GetPopupNotifications() {
  PopupNotifications result;
  size_t default_priority_popup_count = 0;

  // Collect notifications that should be shown as popups. Start from oldest.
  for (Notifications::const_reverse_iterator iter = notifications_.rbegin();
       iter != notifications_.rend(); iter++) {

    if ((*iter)->shown_as_popup())
      continue;

    // No popups for LOW/MIN priority.
    if ((*iter)->priority() < DEFAULT_PRIORITY)
      break;

    // Checking limits. No limits for HIGH/MAX priority. DEFAULT priority
    // will return at most kMaxVisiblePopupNotifications entries. If the
    // popup entries are more, older entries are used. see crbug.com/165768
    if ((*iter)->priority() == DEFAULT_PRIORITY &&
        default_priority_popup_count++ >= kMaxVisiblePopupNotifications) {
      continue;
    }

    result.insert(*iter);
  }
  return result;
}

void NotificationList::MarkPopupsAsShown(int priority) {
  PopupNotifications popups = GetPopupNotifications();
  for (PopupNotifications::iterator iter = popups.begin();
       iter != popups.end(); ++iter) {
    if ((*iter)->priority() == priority)
      (*iter)->set_shown_as_popup(true);
  }
}

void NotificationList::MarkSinglePopupAsShown(
    const std::string& id, bool mark_notification_as_read) {
  Notifications::iterator iter = GetNotification(id);
  DCHECK(iter != notifications_.end());

  if ((*iter)->shown_as_popup())
    return;

  (*iter)->set_shown_as_popup(true);

  if (mark_notification_as_read) {
    --unread_count_;
    (*iter)->set_is_read(true);
  }
}

void NotificationList::SetQuietMode(bool quiet_mode) {
  SetQuietModeInternal(quiet_mode);
  quiet_mode_timer_.reset();
}

void NotificationList::EnterQuietModeWithExpire(
    const base::TimeDelta& expires_in) {
  if (quiet_mode_timer_.get()) {
    // Note that the capital Reset() is the method to restart the timer, not
    // scoped_ptr::reset().
    quiet_mode_timer_->Reset();
  } else {
    SetQuietModeInternal(true);
    quiet_mode_timer_.reset(new base::OneShotTimer<NotificationList>);
    quiet_mode_timer_->Start(FROM_HERE, expires_in, base::Bind(
        &NotificationList::SetQuietMode, base::Unretained(this), false));
  }
}

const NotificationList::Notifications& NotificationList::GetNotifications() {
  return notifications_;
}

size_t NotificationList::NotificationCount() const {
  return notifications_.size();
}

void NotificationList::SetQuietModeInternal(bool quiet_mode) {
  quiet_mode_ = quiet_mode;
  if (quiet_mode_) {
    for (Notifications::iterator iter = notifications_.begin();
         iter != notifications_.end(); ++iter) {
      (*iter)->set_is_read(true);
      (*iter)->set_shown_as_popup(true);
    }
    unread_count_ = 0;
  }
  delegate_->OnQuietModeChanged(quiet_mode);
}

NotificationList::Notifications::iterator
    NotificationList::GetNotification(const std::string& id) {
  for (Notifications::iterator iter = notifications_.begin();
       iter != notifications_.end(); ++iter) {
    if ((*iter)->id() == id)
      return iter;
  }
  return notifications_.end();
}

void NotificationList::EraseNotification(Notifications::iterator iter) {
  if (!message_center_visible_ && !(*iter)->is_read() &&
      (*iter)->priority() > MIN_PRIORITY) {
    --unread_count_;
  }
  delete *iter;
  notifications_.erase(iter);
}

void NotificationList::PushNotification(scoped_ptr<Notification> notification) {
  // Ensure that notification.id is unique by erasing any existing
  // notification with the same id (shouldn't normally happen).
  Notifications::iterator iter = GetNotification(notification->id());
  if (iter != notifications_.end())
    EraseNotification(iter);
  // Add the notification to the the list and mark it unread and unshown.
  if (!message_center_visible_) {
    // TODO(mukai): needs to distinguish if a notification is dismissed by
    // the quiet mode or user operation.
    notification->set_is_read(quiet_mode_);
    notification->set_shown_as_popup(quiet_mode_);
    if (!quiet_mode_ && notification->priority() > MIN_PRIORITY)
        ++unread_count_;
  }
  // Take ownership. The notification can only be removed from the list
  // in EraseNotification(), which will delete it.
  notifications_.insert(notification.release());
}

}  // namespace message_center
