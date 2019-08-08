// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service.h"

namespace notifications {

class NotificationScheduler;
struct NotificationParams;

class NotificationScheduleServiceImpl : public NotificationScheduleService {
 public:
  explicit NotificationScheduleServiceImpl(
      std::unique_ptr<NotificationScheduler> scheduler);
  ~NotificationScheduleServiceImpl() override;

 private:
  // NotificationScheduleService implementation.
  void Schedule(
      std::unique_ptr<NotificationParams> notification_params) override;

  // Provides the actual notification scheduling functionalities.
  std::unique_ptr<NotificationScheduler> scheduler_;

  DISALLOW_COPY_AND_ASSIGN(NotificationScheduleServiceImpl);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_IMPL_H_
