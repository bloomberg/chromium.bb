// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_PLATFORM_IMPL_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_PLATFORM_IMPL_CHROMEOS_H_

#include <memory>

#include "base/callback.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/display/manager/display_configurator.h"

class RelaunchRequiredTimer;

class RelaunchNotificationControllerPlatformImpl
    : public display::DisplayConfigurator::Observer,
      public session_manager::SessionManagerObserver {
 public:
  RelaunchNotificationControllerPlatformImpl();

  ~RelaunchNotificationControllerPlatformImpl() override;

  // Shows the relaunch recommended notification if it is not already open.
  void NotifyRelaunchRecommended(base::Time detection_time, bool past_deadline);

  // Shows the relaunch required notification if it is not already open.
  void NotifyRelaunchRequired(base::Time deadline,
                              base::OnceCallback<base::Time()> on_visible);

  // Sets the notification title to the default one on Chrome OS.
  void CloseRelaunchNotification();

  // Sets the relaunch deadline to |deadline| and refreshes the notification's
  // title accordingly.
  void SetDeadline(base::Time deadline);

  // Returns true if relaunch required notification is shown.
  bool IsRequiredNotificationShown() const;

  // display::DisplayConfigurator::Observer overrides.
  void OnPowerStateChanged(chromeos::DisplayPowerState power_state) override;

  // session_manager::SessionManagerObserver overrides.
  void OnSessionStateChanged() override;

 private:
  // Callback triggered whenever the recommended notification's title has to
  // refresh.
  void RefreshRelaunchRecommendedTitle(bool past_deadline);

  // Ensure show recording only once.
  void RecordRecommendedShowResult();

  // Callback triggered whenever the required notification's title has to
  // refresh.
  void RefreshRelaunchRequiredTitle();

  // Returns true if the display is on && the session is active
  bool CanScheduleReboot();

  // Registers itself to observe display & session state changes
  void StartObserving();

  // Removes itself from observe display & session state observers
  void StopObserving();

  // Timer that takes care of the string refresh in the relaunch required
  // notification title.
  std::unique_ptr<RelaunchRequiredTimer> relaunch_required_timer_;

  // Indicate that show of the Recommended notification was already recorded.
  bool recorded_shown_ = false;

  base::OnceCallback<base::Time()> on_visible_;

  ScopedObserver<display::DisplayConfigurator,
                 display::DisplayConfigurator::Observer>
      display_observer_{this};
  ScopedObserver<session_manager::SessionManager,
                 session_manager::SessionManagerObserver>
      session_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(RelaunchNotificationControllerPlatformImpl);
};

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_PLATFORM_IMPL_CHROMEOS_H_
