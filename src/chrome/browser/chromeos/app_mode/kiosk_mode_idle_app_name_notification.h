// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_MODE_IDLE_APP_NAME_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_MODE_IDLE_APP_NAME_NOTIFICATION_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace chromeos {

class IdleAppNameNotificationView;

class KioskModeIdleAppNameNotification : public ui::UserActivityObserver,
                                         public PowerManagerClient::Observer {
 public:
  static void Initialize();

  static void Shutdown();

  KioskModeIdleAppNameNotification();
  ~KioskModeIdleAppNameNotification() override;

 private:
  // Initialize idle app message when KioskModeHelper is initialized.
  void Setup();

  // ui::UserActivityObserver overrides:
  void OnUserActivity(const ui::Event* event) override;

  // PowerManagerClient::Observer overrides:
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // Begins listening for user activity and calls ResetTimer().
  void Start();

  // Resets |timer_| to fire when the application idle message should be shown.
  void ResetTimer();

  // Invoked by |timer_| to display the application idle message.
  void OnTimeout();

  base::OneShotTimer timer_;

  // If set the notification should get shown upon next user activity.
  bool show_notification_upon_next_user_activity_;

  // The notification object which owns and shows the notification.
  std::unique_ptr<IdleAppNameNotificationView> notification_;

  DISALLOW_COPY_AND_ASSIGN(KioskModeIdleAppNameNotification);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_MODE_IDLE_APP_NAME_NOTIFICATION_H_
