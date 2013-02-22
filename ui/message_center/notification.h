// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_H_

#include <vector>

#include "base/string16.h"
#include "base/time.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center_export.h"
#include "ui/notifications/notification_types.h"

namespace message_center {

struct MESSAGE_CENTER_EXPORT NotificationItem {
  string16 title;
  string16 message;

  NotificationItem(string16 title, string16 message);
};

struct MESSAGE_CENTER_EXPORT Notification {
  Notification();
  virtual ~Notification();

  ui::notifications::NotificationType type;
  std::string id;
  string16 title;
  string16 message;
  string16 display_source;
  std::string extension_id;

  // Begin unpacked values from optional_fields
  int priority;
  base::Time timestamp;
  std::vector<string16> button_titles;
  string16 expanded_message;
  std::vector<NotificationItem> items;
  // End unpacked values

  // Images fetched asynchronously
  gfx::ImageSkia primary_icon;
  gfx::ImageSkia image;
  std::vector<gfx::ImageSkia> button_icons;

  bool is_read;  // True if this has been seen in the message center
  bool shown_as_popup;  // True if this has been shown as a popup notification
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_NOTIFICATION_H_
