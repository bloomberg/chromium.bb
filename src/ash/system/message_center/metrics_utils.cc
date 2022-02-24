// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/metrics_utils.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {
namespace metrics_utils {

NotificationTypeDetailed GetNotificationTypeForChromeApp(
    const message_center::Notification& notification) {
  int priority = notification.rich_notification_data().priority;
  bool require_interaction =
      notification.rich_notification_data().never_timeout;

  if (require_interaction) {
    switch (priority) {
      case -2:
        return CHROME_APP_PRIORITY_MINUS_TWO_REQUIRE_INTERACTION;
      case -1:
        return CHROME_APP_PRIORITY_MINUS_ONE_REQUIRE_INTERACTION;
      case 0:
        return CHROME_APP_PRIORITY_ZERO_REQUIRE_INTERACTION;
      case 1:
        return CHROME_APP_PRIORITY_ONE_REQUIRE_INTERACTION;
      case 2:
        return CHROME_APP_PRIORITY_TWO_REQUIRE_INTERACTION;
      default:
        NOTREACHED();
        return OTHER;
    }
  } else {
    switch (priority) {
      case -2:
        return CHROME_APP_PRIORITY_MINUS_TWO;
      case -1:
        return CHROME_APP_PRIORITY_MINUS_ONE;
      case 0:
        return CHROME_APP_PRIORITY_ZERO;
      case 1:
        return CHROME_APP_PRIORITY_ONE;
      case 2:
        return CHROME_APP_PRIORITY_TWO;
      default:
        NOTREACHED();
        return OTHER;
    }
  }
}

NotificationTypeDetailed GetNotificationTypeForArc(
    const message_center::Notification& notification) {
  int priority = notification.rich_notification_data().priority;
  bool pinned = notification.rich_notification_data().pinned;

  if (pinned) {
    switch (priority) {
      case -2:
        return ARC_PRIORITY_MINUS_TWO_PINNED;
      case -1:
        return ARC_PRIORITY_MINUS_ONE_PINNED;
      case 0:
        return ARC_PRIORITY_ZERO_PINNED;
      case 1:
        return ARC_PRIORITY_ONE_PINNED;
      case 2:
        return ARC_PRIORITY_TWO_PINNED;
      default:
        NOTREACHED();
        return OTHER;
    }
  } else {
    switch (priority) {
      case -2:
        return ARC_PRIORITY_MINUS_TWO;
      case -1:
        return ARC_PRIORITY_MINUS_ONE;
      case 0:
        return ARC_PRIORITY_ZERO;
      case 1:
        return ARC_PRIORITY_ONE;
      case 2:
        return ARC_PRIORITY_TWO;
      default:
        NOTREACHED();
        return OTHER;
    }
  }
}

NotificationTypeDetailed GetNotificationTypeForCrosSystemPriority(
    const message_center::Notification& notification) {
  // The warning level is not stored in the notification data, so we need to
  // infer it from the accent color.
  absl::optional<SkColor> accent_color =
      notification.rich_notification_data().accent_color;
  message_center::SystemNotificationWarningLevel warning_level =
      message_center::SystemNotificationWarningLevel::NORMAL;
  if (accent_color.has_value()) {
    if (accent_color.value() == kSystemNotificationColorWarning) {
      warning_level = message_center::SystemNotificationWarningLevel::WARNING;
    } else if (accent_color.value() ==
               kSystemNotificationColorCriticalWarning) {
      warning_level =
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
    }
  }

  bool pinned = notification.rich_notification_data().pinned;

  if (pinned) {
    switch (warning_level) {
      case message_center::SystemNotificationWarningLevel::NORMAL:
        return CROS_SYSTEM_PRIORITY_PINNED;
      case message_center::SystemNotificationWarningLevel::WARNING:
        return CROS_SYSTEM_PRIORITY_WARNING_PINNED;
      case message_center::SystemNotificationWarningLevel::CRITICAL_WARNING:
        return CROS_SYSTEM_PRIORITY_CRITICAL_WARNING_PINNED;
    }
  } else {
    switch (warning_level) {
      case message_center::SystemNotificationWarningLevel::NORMAL:
        return CROS_SYSTEM_PRIORITY;
      case message_center::SystemNotificationWarningLevel::WARNING:
        return CROS_SYSTEM_PRIORITY_WARNING;
      case message_center::SystemNotificationWarningLevel::CRITICAL_WARNING:
        return CROS_SYSTEM_PRIORITY_CRITICAL_WARNING;
    }
  }
}

NotificationTypeDetailed GetNotificationTypeForCros(
    const message_center::Notification& notification) {
  int priority = notification.rich_notification_data().priority;
  if (priority == message_center::SYSTEM_PRIORITY)
    return GetNotificationTypeForCrosSystemPriority(notification);

  bool pinned = notification.rich_notification_data().pinned;

  if (pinned) {
    switch (priority) {
      case -2:
        return CROS_PRIORITY_MINUS_TWO_PINNED;
      case -1:
        return CROS_PRIORITY_MINUS_ONE_PINNED;
      case 0:
        return CROS_PRIORITY_ZERO_PINNED;
      case 1:
        return CROS_PRIORITY_ONE_PINNED;
      case 2:
        return CROS_PRIORITY_TWO_PINNED;
      default:
        NOTREACHED();
        return OTHER;
    }
  } else {
    switch (priority) {
      case -2:
        return CROS_PRIORITY_MINUS_TWO;
      case -1:
        return CROS_PRIORITY_MINUS_ONE;
      case 0:
        return CROS_PRIORITY_ZERO;
      case 1:
        return CROS_PRIORITY_ONE;
      case 2:
        return CROS_PRIORITY_TWO;
      default:
        NOTREACHED();
        return OTHER;
    }
  }
}

NotificationTypeDetailed GetNotificationTypeForWeb(
    const message_center::Notification& notification) {
  bool require_interaction =
      notification.rich_notification_data().never_timeout;
  return require_interaction ? WEB_REQUIRE_INTERACTION : WEB;
}

NotificationTypeDetailed GetNotificationTypeForPhoneHub(
    const message_center::Notification& notification) {
  int priority = notification.rich_notification_data().priority;
  switch (priority) {
    case -2:
      return PHONEHUB_PRIORITY_MINUS_TWO;
    case -1:
      return PHONEHUB_PRIORITY_MINUS_ONE;
    case 0:
      return PHONEHUB_PRIORITY_ZERO;
    case 1:
      return PHONEHUB_PRIORITY_ONE;
    case 2:
      return PHONEHUB_PRIORITY_TWO;
    default:
      NOTREACHED();
      return OTHER;
  }
}

NotificationTypeDetailed GetNotificationType(
    const message_center::Notification& notification) {
  message_center::NotifierType notifier_type = notification.notifier_id().type;
  switch (notifier_type) {
    case message_center::NotifierType::APPLICATION:
      return GetNotificationTypeForChromeApp(notification);
    case message_center::NotifierType::ARC_APPLICATION:
      return GetNotificationTypeForArc(notification);
    case message_center::NotifierType::WEB_PAGE:
      return GetNotificationTypeForWeb(notification);
    case message_center::NotifierType::SYSTEM_COMPONENT:
      return GetNotificationTypeForCros(notification);
    case message_center::NotifierType::PHONE_HUB:
      return GetNotificationTypeForPhoneHub(notification);
    case message_center::NotifierType::CROSTINI_APPLICATION:
    default:
      return OTHER;
  }
}

absl::optional<NotificationViewType> GetNotificationViewType(
    message_center::Notification* notification) {
  absl::optional<NotificationViewType> type;

  // Ignore ARC notification since its view is rendered through Android, and
  // its notification metadata for image and buttons is empty. Also ignore group
  // parent notification since it only serves as a placeholder.
  if (!notification ||
      notification->notifier_id().type ==
          message_center::NotifierType::ARC_APPLICATION ||
      notification->group_parent())
    return type;

  bool has_inline_reply = false;
  for (auto button_info : notification->buttons()) {
    if (button_info.placeholder) {
      has_inline_reply = true;
      break;
    }
  }

  bool has_buttons = !notification->buttons().empty();
  bool has_image = !notification->image().IsEmpty();
  bool is_grouped_child = notification->group_child();

  if (has_inline_reply) {
    if (has_image) {
      type = is_grouped_child
                 ? NotificationViewType::GROUPED_HAS_IMAGE_AND_INLINE_REPLY
                 : NotificationViewType::HAS_IMAGE_AND_INLINE_REPLY;
    } else {
      type = is_grouped_child ? NotificationViewType::GROUPED_HAS_INLINE_REPLY
                              : NotificationViewType::HAS_INLINE_REPLY;
    }
    return type;
  }

  if (has_buttons) {
    if (has_image) {
      type = is_grouped_child
                 ? NotificationViewType::GROUPED_HAS_IMAGE_AND_ACTION
                 : NotificationViewType::HAS_IMAGE_AND_ACTION;
    } else {
      type = is_grouped_child ? NotificationViewType::GROUPED_HAS_ACTION
                              : NotificationViewType::HAS_ACTION;
    }
    return type;
  }

  if (has_image) {
    return is_grouped_child ? NotificationViewType::GROUPED_HAS_IMAGE
                            : NotificationViewType::HAS_IMAGE;
  } else {
    return is_grouped_child ? NotificationViewType::GROUPED_SIMPLE
                            : NotificationViewType::SIMPLE;
  }
}

absl::optional<NotificationTypeDetailed> GetNotificationType(
    const std::string& notification_id) {
  absl::optional<NotificationTypeDetailed> type;
  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);
  if (!notification)
    return type;
  type = GetNotificationType(*notification);
  return type;
}

void LogHover(const std::string& notification_id, bool is_popup) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  if (is_popup) {
    base::UmaHistogramEnumeration("Notifications.Cros.Actions.Popup.Hover",
                                  type.value());
  } else {
    base::UmaHistogramEnumeration("Notifications.Cros.Actions.Tray.Hover",
                                  type.value());
  }
}

void LogClickedBody(const std::string& notification_id, bool is_popup) {
  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);
  if (!notification)
    return;
  auto type = GetNotificationType(*notification);

  if (is_popup) {
    UMA_HISTOGRAM_ENUMERATION("Notifications.Cros.Actions.Popup.ClickedBody",
                              type);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Notifications.Cros.Actions.Tray.ClickedBody",
                              type);
  }

  // If notification's delegate is null, that means the notification is not
  // clickable and the user just did a "bad click", which is a click that did
  // not do anything.
  if (notification->delegate()) {
    base::UmaHistogramEnumeration(
        "Notifications.Cros.Actions.ClickedBody.GoodClick", type);
  } else {
    base::UmaHistogramEnumeration(
        "Notifications.Cros.Actions.ClickedBody.BadClick", type);
  }
}

void LogClickedActionButton(const std::string& notification_id, bool is_popup) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  if (is_popup) {
    UMA_HISTOGRAM_ENUMERATION(
        "Notifications.Cros.Actions.Popup.ClickedActionButton", type.value());
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Notifications.Cros.Actions.Tray.ClickedActionButton", type.value());
  }
}

void LogInlineReplySent(const std::string& notification_id, bool is_popup) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  if (is_popup) {
    base::UmaHistogramEnumeration(
        "Notifications.Cros.Actions.Popup.InlineReplySent", type.value());
  } else {
    base::UmaHistogramEnumeration(
        "Notifications.Cros.Actions.Tray.InlineReplySent", type.value());
  }
}

void LogPopupExpiredToTray(const std::string& notification_id) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  UMA_HISTOGRAM_ENUMERATION("Notifications.Cros.Actions.Popup.ExpireToTray",
                            type.value());
}

void LogClosedByUser(const std::string& notification_id,
                     bool is_swipe,
                     bool is_popup) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  if (is_popup) {
    if (is_swipe) {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Popup.ClosedByUser.Swipe", type.value());
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Popup.ClosedByUser.Click", type.value());
    }
  } else {
    if (is_swipe) {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Tray.ClosedByUser.Swipe", type.value());
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Tray.ClosedByUser.Click", type.value());
    }
  }
}

void LogSettingsShown(const std::string& notification_id,
                      bool is_slide_controls,
                      bool is_popup) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  if (is_popup) {
    DCHECK(!is_slide_controls);
    UMA_HISTOGRAM_ENUMERATION(
        "Notifications.Cros.Actions.Popup.SettingsShown.HoverControls",
        type.value());
  } else {
    if (is_slide_controls) {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Tray.SettingsShown.SlideControls",
          type.value());
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Tray.SettingsShown.HoverControls",
          type.value());
    }
  }
}

void LogSnoozed(const std::string& notification_id,
                bool is_slide_controls,
                bool is_popup) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  if (is_popup) {
    DCHECK(!is_slide_controls);
    UMA_HISTOGRAM_ENUMERATION(
        "Notifications.Cros.Actions.Popup.Snoozed.HoverControls", type.value());
  } else {
    if (is_slide_controls) {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Tray.Snoozed.SlideControls",
          type.value());
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Tray.Snoozed.HoverControls",
          type.value());
    }
  }
}

void LogPopupShown(const std::string& notification_id) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  UMA_HISTOGRAM_ENUMERATION("Notifications.Cros.Actions.Popup.Shown",
                            type.value());
}

void LogClosedByClearAll(const std::string& notification_id) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  UMA_HISTOGRAM_ENUMERATION("Notifications.Cros.Actions.Tray.ClosedByClearAll",
                            type.value());
}

void LogNotificationAdded(const std::string& notification_id) {
  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);
  if (!notification)
    return;

  base::UmaHistogramEnumeration("Notifications.Cros.Actions.NotificationAdded",
                                GetNotificationType(*notification));

  auto notification_view_type = GetNotificationViewType(notification);
  if (!notification_view_type)
    return;

  base::UmaHistogramEnumeration("Ash.NotificationView.NotificationAdded.Type",
                                notification_view_type.value());
}

void LogNotificationsShownInFirstMinute(int count) {
  UMA_HISTOGRAM_COUNTS_1000(
      "Notifications.Cros.Actions.CountOfNotificationShownInFirstMinutePerUser",
      count);
}

}  // namespace metrics_utils
}  // namespace ash
