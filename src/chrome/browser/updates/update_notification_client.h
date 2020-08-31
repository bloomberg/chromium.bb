// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_CLIENT_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_CLIENT_H_

#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"

namespace updates {

class UpdateNotificationService;

// Client side code for Chrome Update notification, integrated with
// notificaiton scheduler system.
class UpdateNotificationClient
    : public notifications::NotificationSchedulerClient {
 public:
  using NotificationData = notifications::NotificationData;
  using UserActionData = notifications::UserActionData;
  using ThrottleConfig = notifications::ThrottleConfig;
  using GetServiceCallback =
      base::RepeatingCallback<UpdateNotificationService*()>;

  explicit UpdateNotificationClient(GetServiceCallback callback);
  ~UpdateNotificationClient() override;

 private:
  // NotificationSchedulerClient implementation.
  void BeforeShowNotification(
      std::unique_ptr<NotificationData> notification_data,
      NotificationDataCallback callback) override;
  void OnSchedulerInitialized(bool success,
                              std::set<std::string> guids) override;
  void OnUserAction(const UserActionData& action_data) override;
  void GetThrottleConfig(ThrottleConfigCallback callback) override;

  GetServiceCallback get_service_callback_;

  DISALLOW_COPY_AND_ASSIGN(UpdateNotificationClient);
};

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_CLIENT_H_
