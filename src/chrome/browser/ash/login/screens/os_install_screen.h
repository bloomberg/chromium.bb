// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OS_INSTALL_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OS_INSTALL_SCREEN_H_

#include <string>

#include "base/scoped_observation.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/os_install_screen_handler.h"
#include "chromeos/dbus/os_install/os_install_client.h"

namespace ash {

class OsInstallScreen : public BaseScreen, public OsInstallClient::Observer {
 public:
  using TView = chromeos::OsInstallScreenView;

  explicit OsInstallScreen(OsInstallScreenView* view,
                           const base::RepeatingClosure& exit_callback);
  OsInstallScreen(const OsInstallScreen&) = delete;
  OsInstallScreen& operator=(const OsInstallScreen&) = delete;
  ~OsInstallScreen() override;

  void OnViewDestroyed(OsInstallScreenView* view);

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  // OsInstallClient::Observer:
  void StatusChanged(OsInstallClient::Status status,
                     const std::string& service_log) override;

  void StartInstall();
  void RunAutoShutdownCountdown();
  void UpdateCountdownString();
  void Shutdown();

  OsInstallScreenView* view_ = nullptr;

  base::TimeTicks shutdown_time_;

  // Shut down countdown timer on installation success.
  std::unique_ptr<base::RepeatingTimer> shutdown_countdown_;

  // Used for testing.
  const base::TickClock* tick_clock_;

  base::ScopedObservation<OsInstallClient, OsInstallClient::Observer>
      scoped_observation_{this};

  const base::RepeatingClosure exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OS_INSTALL_SCREEN_H_
