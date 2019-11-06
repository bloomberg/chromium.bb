// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/notification_utils.h"

#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

std::unique_ptr<message_center::Notification> CreateSystemNotification(
    const std::string& notification_id,
    const base::string16& title,
    const base::string16& message,
    const std::string& system_component_id,
    const base::RepeatingClosure& click_callback) {
  DCHECK(!click_callback.is_null());
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
          message, base::string16() /* display_source */, GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              system_component_id),
          message_center::RichNotificationData(),
          new message_center::HandleNotificationClickDelegate(click_callback),
          gfx::kNoneIcon,
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  notification->SetSystemPriority();
  return notification;
}

std::unique_ptr<message_center::Notification> CreateSystemNotification(
    message_center::NotificationType type,
    const std::string& id,
    const base::string16& title,
    const base::string16& message,
    const base::string16& display_source,
    const GURL& origin_url,
    const message_center::NotifierId& notifier_id,
    const message_center::RichNotificationData& optional_fields,
    scoped_refptr<message_center::NotificationDelegate> delegate,
    const gfx::VectorIcon& small_image,
    message_center::SystemNotificationWarningLevel color_type) {
  DCHECK_EQ(message_center::NotifierType::SYSTEM_COMPONENT, notifier_id.type);
  SkColor color = kSystemNotificationColorNormal;
  switch (color_type) {
    case message_center::SystemNotificationWarningLevel::NORMAL:
      color = kSystemNotificationColorNormal;
      break;
    case message_center::SystemNotificationWarningLevel::WARNING:
      color = kSystemNotificationColorWarning;
      break;
    case message_center::SystemNotificationWarningLevel::CRITICAL_WARNING:
      color = kSystemNotificationColorCriticalWarning;
      break;
  }
  auto notification = std::make_unique<message_center::Notification>(
      type, id, title, message, gfx::Image(), display_source, origin_url,
      notifier_id, optional_fields, delegate);
  notification->set_accent_color(color);
  if (!small_image.is_empty())
    notification->set_vector_small_image(small_image);
  return notification;
}

}  // namespace ash
