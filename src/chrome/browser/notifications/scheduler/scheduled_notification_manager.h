// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULED_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULED_NOTIFICATION_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/collection_store.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_types.h"

namespace notifications {

struct NotificationEntry;
struct NotificationParams;

// Class to manage in-memory scheduled notifications loaded from the storage.
class ScheduledNotificationManager {
 public:
  using InitCallback = base::OnceCallback<void(bool)>;
  using Notifications =
      std::map<SchedulerClientType, std::vector<const NotificationEntry*>>;

  // Delegate that receives events from the manager.
  class Delegate {
   public:
    // Displays a notification to the user.
    virtual void DisplayNotification(
        std::unique_ptr<NotificationEntry> notification) = 0;

    Delegate() = default;
    virtual ~Delegate() = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Creates the instance.
  static std::unique_ptr<ScheduledNotificationManager> Create(
      std::unique_ptr<CollectionStore<NotificationEntry>> store);

  // Initializes the notification store.
  virtual void Init(Delegate* delegate, InitCallback callback) = 0;

  // Adds a new notification.
  virtual void ScheduleNotification(
      std::unique_ptr<NotificationParams> notification_params) = 0;

  // Displays a notification, the scheduled notification will be removed from
  // storage, then Delegate::DisplayNotification() should be invoked.
  virtual void DisplayNotification(const std::string& guid) = 0;

  // Gets all scheduled notifications. For each type, notifications are sorted
  // by creation timestamp.
  virtual void GetAllNotifications(Notifications* notifications) = 0;

  virtual ~ScheduledNotificationManager();

 protected:
  ScheduledNotificationManager();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScheduledNotificationManager);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULED_NOTIFICATION_MANAGER_H_
