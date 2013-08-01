// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification.h"

#include "base/logging.h"
#include "ui/message_center/notification_types.h"

namespace {
unsigned g_next_serial_number_ = 0;
}

namespace message_center {

NotificationItem::NotificationItem(const string16& title,
                                   const string16& message)
 : title(title),
   message(message) {
}

ButtonInfo::ButtonInfo(const string16& title)
 : title(title) {
}

RichNotificationData::RichNotificationData()
    : priority(DEFAULT_PRIORITY),
      never_timeout(false),
      timestamp(base::Time::Now()),
      progress(0) {}

RichNotificationData::RichNotificationData(const RichNotificationData& other)
    : priority(other.priority),
      never_timeout(other.never_timeout),
      timestamp(other.timestamp),
      expanded_message(other.expanded_message),
      image(other.image),
      items(other.items),
      progress(other.progress),
      buttons(other.buttons) {}

RichNotificationData::~RichNotificationData() {}

Notification::Notification(NotificationType type,
                           const std::string& id,
                           const string16& title,
                           const string16& message,
                           const gfx::Image& icon,
                           const string16& display_source,
                           const std::string& extension_id,
                           const RichNotificationData& optional_fields,
                           NotificationDelegate* delegate)
    : type_(type),
      id_(id),
      title_(title),
      message_(message),
      icon_(icon),
      display_source_(display_source),
      extension_id_(extension_id),
      serial_number_(g_next_serial_number_++),
      optional_fields_(optional_fields),
      shown_as_popup_(false),
      is_read_(false),
      is_expanded_(false),
      delegate_(delegate) {}

Notification::Notification(const Notification& other)
    : type_(other.type_),
      id_(other.id_),
      title_(other.title_),
      message_(other.message_),
      icon_(other.icon_),
      display_source_(other.display_source_),
      extension_id_(other.extension_id_),
      serial_number_(other.serial_number_),
      optional_fields_(other.optional_fields_),
      shown_as_popup_(other.shown_as_popup_),
      is_read_(other.is_read_),
      is_expanded_(other.is_expanded_),
      delegate_(other.delegate_) {}

Notification& Notification::operator=(const Notification& other) {
  type_ = other.type_;
  id_ = other.id_;
  title_ = other.title_;
  message_ = other.message_;
  icon_ = other.icon_;
  display_source_ = other.display_source_;
  extension_id_ = other.extension_id_;
  serial_number_ = other.serial_number_;
  optional_fields_ = other.optional_fields_;
  shown_as_popup_ = other.shown_as_popup_;
  is_read_ = other.is_read_;
  is_expanded_ = other.is_expanded_;
  delegate_ = other.delegate_;

  return *this;
}

Notification::~Notification() {}

void Notification::CopyState(Notification* base) {
  shown_as_popup_ = base->shown_as_popup();
  is_read_ = base->is_read();
  is_expanded_ = base->is_expanded();
  if (!delegate_.get())
    delegate_ = base->delegate();
  optional_fields_.never_timeout = base->never_timeout();
}

void Notification::SetButtonIcon(size_t index, const gfx::Image& icon) {
  if (index >= optional_fields_.buttons.size())
    return;
  optional_fields_.buttons[index].icon = icon;
}

void Notification::SetSystemPriority() {
  optional_fields_.priority = SYSTEM_PRIORITY;
  optional_fields_.never_timeout = true;
}

}  // namespace message_center
