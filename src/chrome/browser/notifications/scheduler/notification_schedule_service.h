// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace notifications {

struct NotificationParams;

// Service to schedule a notification to display in the future. An internal
// throttling mechanism will be applied to limit the maximum notification shown
// to the user. Also user's interaction with the notification will affect the
// frequency of notification delivery.
class NotificationScheduleService : public KeyedService {
 public:
  // Schedules a notification to display.
  virtual void Schedule(
      std::unique_ptr<NotificationParams> notification_params) = 0;

 protected:
  NotificationScheduleService() = default;
  ~NotificationScheduleService() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationScheduleService);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_H_
