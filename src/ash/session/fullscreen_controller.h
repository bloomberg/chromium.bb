// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_FULLSCREEN_CONTROLLER_H_
#define ASH_SESSION_FULLSCREEN_CONTROLLER_H_

#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

class SessionControllerImpl;

class FullscreenController : public chromeos::PowerManagerClient::Observer {
 public:
  explicit FullscreenController(SessionControllerImpl* session_controller);
  FullscreenController(const FullscreenController&) = delete;
  FullscreenController& operator=(const FullscreenController&) = delete;

  ~FullscreenController() override;

  static void MaybeExitFullscreen();

 private:
  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& proto) override;

  const SessionControllerImpl* const session_controller_;
};

}  // namespace ash

#endif  // ASH_SESSION_FULLSCREEN_CONTROLLER_H_
