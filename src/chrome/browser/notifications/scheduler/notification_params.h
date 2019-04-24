// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_PARAMS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_PARAMS_H_

#include <memory>

#include "chrome/browser/notifications/scheduler/notification_data.h"
#include "chrome/browser/notifications/scheduler/schedule_params.h"

namespace notifications {

// Struct used to schedule a notification.
struct NotificationParams {
  enum class Type {
    PLACE_HOLDER,
  };

  NotificationParams(Type type,
                     NotificationData notification,
                     ScheduleParams schedule_params);
  ~NotificationParams();

  // The type of notification using the scheduling system.
  Type type;

  // Data used to show the notification, such as text or title on the
  // notification.
  NotificationData notification;

  // Scheduling details used to determine when to show the notification.
  ScheduleParams schedule_params;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_PARAMS_H_
