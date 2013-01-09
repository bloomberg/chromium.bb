// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_list.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/time.h"
#include "base/values.h"

namespace message_center {

const size_t NotificationList::kMaxVisibleMessageCenterNotifications = 100;
const size_t NotificationList::kMaxVisiblePopupNotifications = 2;

NotificationList::Notification::Notification() : is_read(false),
                                                 shown_as_popup(false) {
}

NotificationList::Notification::~Notification() {
}

NotificationList::NotificationList(Delegate* delegate)
    : delegate_(delegate),
      message_center_visible_(false),
      unread_count_(0),
      quiet_mode_(false) {
}

NotificationList::~NotificationList() {
}

void NotificationList::SetMessageCenterVisible(bool visible) {
  if (message_center_visible_ == visible)
    return;
  message_center_visible_ = visible;
  if (!visible) {
    // When the list is hidden, clear the unread count, and mark all
    // notifications as read and shown.
    unread_count_ = 0;
    for (NotificationMap::iterator mapiter = notifications_.begin();
         mapiter != notifications_.end(); ++mapiter) {
      for (Notifications::iterator iter = mapiter->second.begin();
           iter != mapiter->second.end(); ++iter) {
        iter->is_read = true;
        iter->shown_as_popup = true;
      }
    }
  }
}

void NotificationList::AddNotification(
    ui::notifications::NotificationType type,
    const std::string& id,
    const string16& title,
    const string16& message,
    const string16& display_source,
    const std::string& extension_id,
    const DictionaryValue* optional_fields) {
  Notification notification;
  notification.type = type;
  notification.id = id;
  notification.title = title;
  notification.message = message;
  notification.display_source = display_source;
  notification.extension_id = extension_id;

  // Initialize primitive fields before unpacking optional fields.
  // timestamp initializes to default NULL time.
  notification.priority = ui::notifications::DEFAULT_PRIORITY;
  notification.unread_count = 0;

  UnpackOptionalFields(optional_fields, &notification);

  PushNotification(notification);
}

void NotificationList::UnpackOptionalFields(
    const DictionaryValue* optional_fields, Notification* notification) {
  if (!optional_fields)
    return;

  if (optional_fields->HasKey(ui::notifications::kPriorityKey))
    optional_fields->GetInteger(ui::notifications::kPriorityKey,
                                &notification->priority);
  if (optional_fields->HasKey(ui::notifications::kTimestampKey)) {
    std::string time_string;
    optional_fields->GetString(ui::notifications::kTimestampKey, &time_string);
    base::Time::FromString(time_string.c_str(), &notification->timestamp);
  }
  if (optional_fields->HasKey(ui::notifications::kUnreadCountKey))
    optional_fields->GetInteger(ui::notifications::kUnreadCountKey,
                                &notification->unread_count);
  if (optional_fields->HasKey(ui::notifications::kButtonOneTitleKey))
    optional_fields->GetString(ui::notifications::kButtonOneTitleKey,
                               &notification->button_one_title);
  if (optional_fields->HasKey(ui::notifications::kButtonTwoTitleKey))
    optional_fields->GetString(ui::notifications::kButtonTwoTitleKey,
                               &notification->button_two_title);
  if (optional_fields->HasKey(ui::notifications::kExpandedMessageKey))
    optional_fields->GetString(ui::notifications::kExpandedMessageKey,
                               &notification->expanded_message);
  if (optional_fields->HasKey(ui::notifications::kItemsKey)) {
    const ListValue* items;
    CHECK(optional_fields->GetList(ui::notifications::kItemsKey, &items));
    for (size_t i = 0; i < items->GetSize(); ++i) {
      string16 title;
      string16 message;
      const base::DictionaryValue* item;
      items->GetDictionary(i, &item);
      item->GetString(ui::notifications::kItemTitleKey, &title);
      item->GetString(ui::notifications::kItemMessageKey, &message);
      notification->items.push_back(NotificationItem(title, message));
    }
  }
}

void NotificationList::UpdateNotificationMessage(
    const std::string& old_id,
    const std::string& new_id,
    const string16& title,
    const string16& message,
    const base::DictionaryValue* optional_fields) {
  Notifications::iterator iter;
  if (!GetNotification(old_id, &iter))
    return;
  // Copy and update notification, then move it to the front of the list.
  Notification notification(*iter);
  notification.id = new_id;
  notification.title = title;
  notification.message = message;
  UnpackOptionalFields(optional_fields, &notification);
  EraseNotification(iter);
  PushNotification(notification);
}

bool NotificationList::RemoveNotification(const std::string& id) {
  Notifications::iterator iter;
  if (!GetNotification(id, &iter))
    return false;
  EraseNotification(iter);
  return true;
}

void NotificationList::RemoveAllNotifications() {
  notifications_.clear();
  unread_count_ = 0;
}

void NotificationList::SendRemoveNotificationsBySource(
    const std::string& id) {
  Notifications::iterator source_iter;
  if (!GetNotification(id, &source_iter))
    return;
  string16 display_source = source_iter->display_source;
  for (NotificationMap::iterator mapiter = notifications_.begin();
       mapiter != notifications_.end(); ++mapiter) {
    for (Notifications::iterator loopiter = mapiter->second.begin();
         loopiter != mapiter->second.end(); ) {
      Notifications::iterator curiter = loopiter++;
      if (curiter->display_source == display_source)
        delegate_->SendRemoveNotification(curiter->id);
    }
  }
}

void NotificationList::SendRemoveNotificationsByExtension(
    const std::string& id) {
  Notifications::iterator source_iter;
  if (!GetNotification(id, &source_iter))
    return;
  std::string extension_id = source_iter->extension_id;
  for (NotificationMap::iterator mapiter = notifications_.begin();
       mapiter != notifications_.end(); ++mapiter) {
    for (Notifications::iterator loopiter = mapiter->second.begin();
         loopiter != mapiter->second.end(); ) {
      Notifications::iterator curiter = loopiter++;
      if (curiter->extension_id == extension_id)
        delegate_->SendRemoveNotification(curiter->id);
    }
  }
}

bool NotificationList::SetNotificationPrimaryIcon(const std::string& id,
                                                  const gfx::ImageSkia& image) {
  Notifications::iterator iter;
  if (!GetNotification(id, &iter))
    return false;
  iter->primary_icon = image;
  return true;
}

bool NotificationList::SetNotificationSecondaryIcon(
    const std::string& id,
    const gfx::ImageSkia& image) {
  Notifications::iterator iter;
  if (!GetNotification(id, &iter))
    return false;
  iter->secondary_icon = image;
  return true;
}

bool NotificationList::SetNotificationImage(const std::string& id,
                                            const gfx::ImageSkia& image) {
  Notifications::iterator iter;
  if (!GetNotification(id, &iter))
    return false;
  iter->image = image;
  return true;
}

bool NotificationList::HasNotification(const std::string& id) {
  Notifications::iterator dummy;
  return GetNotification(id, &dummy);
}

bool NotificationList::HasPopupNotifications() {
  for (int i = ui::notifications::DEFAULT_PRIORITY;
       i <= ui::notifications::MAX_PRIORITY; ++i) {
    Notifications notifications = notifications_[i];
    if (!notifications.empty() && !notifications.front().shown_as_popup)
      return true;
  }
  return false;
}

void NotificationList::GetPopupNotifications(
    NotificationList::Notifications* notifications) {
  typedef std::pair<Notifications::iterator, Notifications::iterator>
      NotificationRange;
  // In the popup, latest should come earlier.
  std::list<NotificationRange> iters;
  for (int i = ui::notifications::DEFAULT_PRIORITY;
       i <= ui::notifications::MAX_PRIORITY; ++i) {
    Notifications::iterator first, last;
    GetPopupIterators(i, &first, &last);
    if (first != last)
      iters.push_back(make_pair(first, last));
  }
  notifications->clear();
  while (!iters.empty()) {
    std::list<NotificationRange>::iterator max_iter = iters.begin();
    std::list<NotificationRange>::iterator iter = max_iter;
    iter++;
    for (; iter != iters.end(); ++iter) {
      if (max_iter->first->timestamp < iter->first->timestamp)
        max_iter = iter;
    }
    notifications->push_back(*(max_iter->first));
    ++(max_iter->first);
    if (max_iter->first == max_iter->second)
      iters.erase(max_iter);
  }
}

void NotificationList::MarkPopupsAsShown(int priority) {
  Notifications::iterator first, last;
  GetPopupIterators(priority, &first, &last);
  for (Notifications::iterator iter = first; iter != last; ++iter)
    iter->shown_as_popup = true;
}

void NotificationList::MarkSinglePopupAsShown(const std::string& id) {
  Notifications::iterator iter;
  if (!GetNotification(id, &iter))
    return;

  if (iter->shown_as_popup)
    return;

  // Moves the item to the beginning of the already-shown items.
  Notification notification = *iter;
  notification.shown_as_popup = true;
  notifications_[notification.priority].erase(iter);
  for (Notifications::iterator iter2 =
           notifications_[notification.priority].begin();
       iter2 != notifications_[notification.priority].end(); iter2++) {
    if (iter2->shown_as_popup) {
      notifications_[notification.priority].insert(iter2, notification);
      return;
    }
  }

  // No notifications are already shown as popup, so just re-adding at the end
  // of the list.
  notifications_[notification.priority].push_back(notification);
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

void NotificationList::GetNotifications(
    NotificationList::Notifications* notifications) const {
  DCHECK(notifications);
  // Higher priority should come earlier.
  for (NotificationMap::const_reverse_iterator mapiter =
           notifications_.rbegin();
       mapiter != notifications_.rend(); ++mapiter) {
    for (Notifications::const_iterator iter = mapiter->second.begin();
         iter != mapiter->second.end(); ++iter) {
      notifications->push_back(*iter);
    }
  }
}

size_t NotificationList::NotificationCount() const {
  size_t result = 0;
  for (NotificationMap::const_iterator mapiter = notifications_.begin();
       mapiter != notifications_.end(); ++mapiter) {
    result += mapiter->second.size();
  }
  return result;
}

void NotificationList::SetQuietModeInternal(bool quiet_mode) {
  quiet_mode_ = quiet_mode;
  if (quiet_mode_) {
    for (NotificationMap::iterator mapiter = notifications_.begin();
         mapiter != notifications_.end(); ++mapiter) {
      for (Notifications::iterator iter = mapiter->second.begin();
           iter != mapiter->second.end(); ++iter) {
        iter->is_read = true;
        iter->shown_as_popup = true;
      }
    }
    unread_count_ = 0;
  }
  delegate_->OnQuietModeChanged(quiet_mode);
}

bool NotificationList::GetNotification(
    const std::string& id, Notifications::iterator* iter) {
  for (NotificationMap::iterator mapiter = notifications_.begin();
       mapiter != notifications_.end(); ++mapiter) {
    for (Notifications::iterator curiter = mapiter->second.begin();
         curiter != mapiter->second.end(); ++curiter) {
      if (curiter->id == id) {
        *iter = curiter;
        return true;
      }
    }
  }
  return false;
}

void NotificationList::EraseNotification(Notifications::iterator iter) {
  if (!message_center_visible_ && !iter->is_read &&
      iter->priority > ui::notifications::MIN_PRIORITY) {
    --unread_count_;
  }
  notifications_[iter->priority].erase(iter);
}

void NotificationList::PushNotification(Notification& notification) {
  // Ensure that notification.id is unique by erasing any existing
  // notification with the same id (shouldn't normally happen).
  Notifications::iterator iter;
  if (GetNotification(notification.id, &iter))
    EraseNotification(iter);
  // Add the notification to the front (top) of the list and mark it
  // unread and unshown.
  if (!message_center_visible_) {
    if (quiet_mode_) {
      // TODO(mukai): needs to distinguish if a notification is dismissed by
      // the quiet mode or user operation.
      notification.is_read = true;
      notification.shown_as_popup = true;
    } else {
      if (notification.priority > ui::notifications::MIN_PRIORITY)
        ++unread_count_;
      notification.is_read = false;
      notification.shown_as_popup = false;
    }
  }
  notifications_[notification.priority].push_front(notification);
}

void NotificationList::GetPopupIterators(int priority,
                                         Notifications::iterator* first,
                                         Notifications::iterator* last) {
  Notifications& notifications = notifications_[priority];
  // No popups for LOW/MIN priority.
  if (priority < ui::notifications::DEFAULT_PRIORITY) {
    *first = notifications.end();
    *last = notifications.end();
    return;
  }

  size_t popup_count = 0;
  *first = notifications.begin();
  *last = *first;
  while (*last != notifications.end()) {
    if ((*last)->shown_as_popup)
      break;
    ++(*last);

    // Checking limits. No limits for HIGH/MAX priority. DEFAULT priority
    // will return at most kMaxVisiblePopupNotifications entries. If the
    // popup entries are more, older entries are used. see crbug.com/165768
    if (priority == ui::notifications::DEFAULT_PRIORITY &&
        popup_count >= kMaxVisiblePopupNotifications) {
      ++(*first);
    } else {
      ++popup_count;
    }
  }
}

}  // namespace message_center
