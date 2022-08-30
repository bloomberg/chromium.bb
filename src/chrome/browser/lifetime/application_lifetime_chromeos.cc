// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime_chromeos.h"
#include "chrome/browser/lifetime/application_lifetime.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/update_engine/update_engine_client.h"

namespace chrome {
namespace {

chromeos::UpdateEngineClient* GetUpdateEngineClient() {
  DCHECK(ash::DBusThreadManager::IsInitialized());
  chromeos::UpdateEngineClient* update_engine_client =
      chromeos::DBusThreadManager::Get()->GetUpdateEngineClient();
  DCHECK(update_engine_client);
  return update_engine_client;
}

ash::PowerManagerClient* GetPowerManagerClient() {
  ash::PowerManagerClient* power_manager_client =
      ash::PowerManagerClient::Get();
  DCHECK(power_manager_client);
  return power_manager_client;
}

}  // namespace

void AttemptRelaunch() {
  GetPowerManagerClient()->RequestRestart(power_manager::REQUEST_RESTART_OTHER,
                                          "Chrome relaunch");
}

void RelaunchIgnoreUnloadHandlers() {
  AttemptRelaunch();
}

void RelaunchForUpdate() {
  DCHECK(UpdatePending());
  GetUpdateEngineClient()->RebootAfterUpdate();
}

bool UpdatePending() {
  if (!ash::DBusThreadManager::IsInitialized())
    return false;

  return GetUpdateEngineClient()->GetLastStatus().current_operation() ==
         update_engine::UPDATED_NEED_REBOOT;
}

}  // namespace chrome
