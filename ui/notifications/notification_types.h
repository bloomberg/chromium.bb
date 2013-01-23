// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NOTIFICATIONS_NOTIFICATION_TYPES_H_
#define UI_NOTIFICATIONS_NOTIFICATION_TYPES_H_

#include <string>

#include "ui/base/ui_export.h"

namespace ui {

namespace notifications {

// Keys for optional fields in Notification.
UI_EXPORT extern const char kPriorityKey[];
UI_EXPORT extern const char kTimestampKey[];
UI_EXPORT extern const char kUnreadCountKey[];
UI_EXPORT extern const char kButtonOneTitleKey[];
UI_EXPORT extern const char kButtonOneIconUrlKey[];
UI_EXPORT extern const char kButtonTwoTitleKey[];
UI_EXPORT extern const char kButtonTwoIconUrlKey[];
UI_EXPORT extern const char kExpandedMessageKey[];
UI_EXPORT extern const char kImageUrlKey[];
UI_EXPORT extern const char kItemsKey[];
UI_EXPORT extern const char kItemTitleKey[];
UI_EXPORT extern const char kItemMessageKey[];

enum NotificationType {
  NOTIFICATION_TYPE_SIMPLE,
  NOTIFICATION_TYPE_BASE_FORMAT,
  NOTIFICATION_TYPE_IMAGE,
  NOTIFICATION_TYPE_MULTIPLE,
};

enum NotificationPriority {
  MIN_PRIORITY = -2,
  LOW_PRIORITY = -1,
  DEFAULT_PRIORITY = 0,
  HIGH_PRIORITY = 1,
  MAX_PRIORITY = 2,
};

UI_EXPORT NotificationType StringToNotificationType(std::string& string_type);

}  // namespace notifications

}  // namespace ui

#endif // UI_NOTIFICATIONS_NOTIFICATION_TYPES_H_
