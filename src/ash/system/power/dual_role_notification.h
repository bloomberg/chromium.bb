// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_DUAL_ROLE_NOTIFICATION_H_
#define ASH_SYSTEM_POWER_DUAL_ROLE_NOTIFICATION_H_

#include <stddef.h>

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/power/power_status.h"
#include "base/macros.h"

namespace message_center {
class MessageCenter;
class Notification;
}

namespace ash {

// Shows a non-toasting MessageCenter notification based on what dual-role
// devices are connected.
class ASH_EXPORT DualRoleNotification {
 public:
  explicit DualRoleNotification(message_center::MessageCenter* message_center);
  ~DualRoleNotification();

  // Creates or updates the notification.
  void Update();

 private:
  // Creates the notification using the updated status.
  std::unique_ptr<message_center::Notification> CreateNotification();

  message_center::MessageCenter* message_center_;
  std::unique_ptr<PowerStatus::PowerSource> dual_role_source_;
  std::unique_ptr<PowerStatus::PowerSource> dual_role_sink_;
  size_t num_dual_role_sinks_;
  bool line_power_connected_;

  DISALLOW_COPY_AND_ASSIGN(DualRoleNotification);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_DUAL_ROLE_NOTIFICATION_H_
