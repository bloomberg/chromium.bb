// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_STRUCT_TRAITS_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_STRUCT_TRAITS_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/message_center/mojo/notification.mojom-shared.h"
#include "ui/message_center/notification.h"

namespace mojo {

template <>
struct EnumTraits<message_center::mojom::NotificationType,
                  message_center::NotificationType> {
  static message_center::mojom::NotificationType ToMojom(
      message_center::NotificationType type) {
    switch (type) {
      case message_center::NOTIFICATION_TYPE_SIMPLE:
        return message_center::mojom::NotificationType::SIMPLE;
      case message_center::NOTIFICATION_TYPE_BASE_FORMAT:
        return message_center::mojom::NotificationType::BASE_FORMAT;
      case message_center::NOTIFICATION_TYPE_IMAGE:
        return message_center::mojom::NotificationType::IMAGE;
      case message_center::NOTIFICATION_TYPE_MULTIPLE:
        return message_center::mojom::NotificationType::MULTIPLE;
      case message_center::NOTIFICATION_TYPE_PROGRESS:
        return message_center::mojom::NotificationType::PROGRESS;
      case message_center::NOTIFICATION_TYPE_CUSTOM:
        return message_center::mojom::NotificationType::CUSTOM;
    }
    NOTREACHED();
    return message_center::mojom::NotificationType::SIMPLE;
  }

  static bool FromMojom(message_center::mojom::NotificationType input,
                        message_center::NotificationType* out) {
    switch (input) {
      case message_center::mojom::NotificationType::SIMPLE:
        *out = message_center::NOTIFICATION_TYPE_SIMPLE;
        return true;
      case message_center::mojom::NotificationType::BASE_FORMAT:
        *out = message_center::NOTIFICATION_TYPE_BASE_FORMAT;
        return true;
      case message_center::mojom::NotificationType::IMAGE:
        *out = message_center::NOTIFICATION_TYPE_IMAGE;
        return true;
      case message_center::mojom::NotificationType::MULTIPLE:
        *out = message_center::NOTIFICATION_TYPE_MULTIPLE;
        return true;
      case message_center::mojom::NotificationType::PROGRESS:
        *out = message_center::NOTIFICATION_TYPE_PROGRESS;
        return true;
      case message_center::mojom::NotificationType::CUSTOM:
        *out = message_center::NOTIFICATION_TYPE_CUSTOM;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct StructTraits<message_center::mojom::RichNotificationDataDataView,
                    message_center::RichNotificationData> {
  static int progress(const message_center::RichNotificationData& r);
  static const base::string16& progress_status(
      const message_center::RichNotificationData& r);
  static bool should_make_spoken_feedback_for_popup_updates(
      const message_center::RichNotificationData& r);
  static bool clickable(const message_center::RichNotificationData& r);
  static bool pinned(const message_center::RichNotificationData& r);
  static const base::string16& accessible_name(
      const message_center::RichNotificationData& r);
  static SkColor accent_color(const message_center::RichNotificationData& r);
  static bool Read(message_center::mojom::RichNotificationDataDataView data,
                   message_center::RichNotificationData* out);
};

template <>
struct StructTraits<message_center::mojom::NotificationDataView,
                    message_center::Notification> {
  static message_center::NotificationType type(
      const message_center::Notification& n);
  static const std::string& id(const message_center::Notification& n);
  static const base::string16& title(const message_center::Notification& n);
  static const base::string16& message(const message_center::Notification& n);
  static gfx::ImageSkia icon(const message_center::Notification& n);
  static const base::string16& display_source(
      const message_center::Notification& n);
  static const GURL& origin_url(const message_center::Notification& n);
  static const message_center::RichNotificationData& optional_fields(
      const message_center::Notification& n);
  static bool Read(message_center::mojom::NotificationDataView data,
                   message_center::Notification* out);
};

}  // namespace mojo

#endif  // UI_MESSAGE_CENTER_NOTIFICATION_STRUCT_TRAITS_H_
