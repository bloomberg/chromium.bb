// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_ENTRY_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_ENTRY_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/notification_data.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/schedule_params.h"

namespace notifications {

// Represents the in-memory counterpart of scheduled notification database
// record.
struct NotificationEntry {
  NotificationEntry();
  NotificationEntry(SchedulerClientType type, const std::string& guid);
  NotificationEntry(const NotificationEntry& other);
  bool operator==(const NotificationEntry& other) const;
  ~NotificationEntry();

  // The type of the notification.
  SchedulerClientType type;

  // The unique id of the notification database entry.
  std::string guid;

  // Creation timestamp.
  base::Time create_time;

  // Contains information to construct the notification.
  NotificationData notification_data;

  // Scheduling details.
  ScheduleParams schedule_params;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_ENTRY_H_
