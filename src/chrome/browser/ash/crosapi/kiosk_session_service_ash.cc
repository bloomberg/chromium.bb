// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/kiosk_session_service_ash.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {

KioskSessionServiceAsh::KioskSessionServiceAsh() = default;

KioskSessionServiceAsh::~KioskSessionServiceAsh() = default;

void KioskSessionServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::KioskSessionService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void KioskSessionServiceAsh::AttemptUserExit() {
  chrome::AttemptUserExit();
}

void KioskSessionServiceAsh::RestartDevice(const std::string& description,
                                           RestartDeviceCallback callback) {
  if (user_manager::UserManager::Get()->IsLoggedInAsKioskApp() ||
      user_manager::UserManager::Get()->IsLoggedInAsWebKioskApp()) {
    chromeos::PowerManagerClient::Get()->RequestRestart(
        power_manager::REQUEST_RESTART_OTHER, description);
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

}  // namespace crosapi
