// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_USER_ACTIVITY_POWER_MANAGER_NOTIFIER_H_
#define UI_CHROMEOS_USER_ACTIVITY_POWER_MANAGER_NOTIFIER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/chromeos/ui_chromeos_export.h"

namespace ui {

class UserActivityDetector;

// Notifies the power manager via D-Bus when the user is active.
class UI_CHROMEOS_EXPORT UserActivityPowerManagerNotifier
    : public UserActivityObserver {
 public:
  // Registers and unregisters itself as an observer of |detector| on
  // construction and destruction.
  explicit UserActivityPowerManagerNotifier(UserActivityDetector* detector);
  ~UserActivityPowerManagerNotifier() override;

  // UserActivityObserver implementation.
  void OnUserActivity(const Event* event) override;

 private:
  UserActivityDetector* detector_;  // not owned

  // Last time that the power manager was notified.
  base::TimeTicks last_notify_time_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityPowerManagerNotifier);
};

}  // namespace ui

#endif  // UI_CHROMEOS_USER_ACTIVITY_POWER_MANAGER_NOTIFIER_H_
