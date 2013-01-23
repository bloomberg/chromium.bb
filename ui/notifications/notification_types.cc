// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/notifications/notification_types.h"

namespace ui {

namespace notifications {

const char kMessageIntentKey[] = "message_intent";
const char kPriorityKey[] = "priority";
const char kTimestampKey[] = "timestamp";
const char kUnreadCountKey[] = "unread_count";
const char kButtonOneTitleKey[] = "button_one_title";
const char kButtonOneIconUrlKey[] = "button_one_icon_url";
const char kButtonTwoTitleKey[] = "button_two_title";
const char kButtonTwoIconUrlKey[] = "button_two_icon_url";
const char kExpandedMessageKey[] = "expanded_message";
const char kImageUrlKey[] = "image_url";
const char kItemsKey[] = "items";
const char kItemTitleKey[] = "title";
const char kItemMessageKey[] = "message";

const char kSimpleType[] = "simple";
const char kBaseFormatType[] = "base";
const char kImageType[] = "image";
const char kMultipleType[] = "multiple";

NotificationType StringToNotificationType(std::string& string_type) {
  // In case of unrecognized string, fall back to most common type.
  return (string_type == kSimpleType) ? NOTIFICATION_TYPE_SIMPLE :
         (string_type == kBaseFormatType) ? NOTIFICATION_TYPE_BASE_FORMAT :
         (string_type == kImageType) ? NOTIFICATION_TYPE_IMAGE :
         (string_type == kMultipleType) ? NOTIFICATION_TYPE_MULTIPLE :
         NOTIFICATION_TYPE_SIMPLE;
}

}  // namespace notifications

}  // namespace ui
