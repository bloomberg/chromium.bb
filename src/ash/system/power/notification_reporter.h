// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_NOTIFICATION_REPORTER_H_
#define ASH_SYSTEM_POWER_NOTIFICATION_REPORTER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/message_center/message_center_observer.h"

namespace ash {

// This class notifies the power manager when a high priority notification i.e.
// one that can wake up the device (alarms, push notifications etc.), is created
// or updated. This is required for doing a full resume if the device woke up in
// dark resume in response to a notification.
class ASH_EXPORT NotificationReporter
    : public message_center::MessageCenterObserver {
 public:
  NotificationReporter();
  ~NotificationReporter() override;

  // Overridden from MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationUpdated(const std::string& notification_id) override;

 private:
  // Notifies power manager if the notification corresponding to
  // |notification_id| has high priority.
  void MaybeNotifyPowerManager(const std::string& notification_id);

  DISALLOW_COPY_AND_ASSIGN(NotificationReporter);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_NOTIFICATION_REPORTER_H_
