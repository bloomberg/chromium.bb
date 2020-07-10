// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOOP_NOTIFICATION_SCHEDULE_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOOP_NOTIFICATION_SCHEDULE_SERVICE_H_

#include <memory>
#include <string>

#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/user_action_handler.h"

namespace notifications {

class NoopNotificationScheduleService
    : public NotificationScheduleService,
      public NotificationBackgroundTaskScheduler::Handler,
      public UserActionHandler {
 public:
  NoopNotificationScheduleService();
  ~NoopNotificationScheduleService() override;

 private:
  // NotificationScheduleService implementation.
  void Schedule(
      std::unique_ptr<NotificationParams> notification_params) override;
  void DeleteNotifications(SchedulerClientType type) override;
  void GetClientOverview(
      SchedulerClientType,
      ClientOverview::ClientOverviewCallback callback) override;
  NotificationBackgroundTaskScheduler::Handler*
  GetBackgroundTaskSchedulerHandler() override;
  UserActionHandler* GetUserActionHandler() override;

  // NotificationBackgroundTaskScheduler::Handler implementation.
  void OnStartTask(TaskFinishedCallback callback) override;
  void OnStopTask() override;

  // UserActionHandler implementation.
  void OnUserAction(const UserActionData& action_data) override;

  DISALLOW_COPY_AND_ASSIGN(NoopNotificationScheduleService);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOOP_NOTIFICATION_SCHEDULE_SERVICE_H_
