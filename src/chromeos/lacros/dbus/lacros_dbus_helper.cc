// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/dbus/lacros_dbus_helper.h"

#include "base/feature_list.h"
#include "chromeos/dbus/init/initialize_dbus_client.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#include "chromeos/lacros/dbus/lacros_dbus_thread_manager.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"

namespace chromeos {

void LacrosInitializeDBus() {
  // Unlike Ash, Lacros has no services that need paths, and therefore needs
  // not override paths like Ash does.

  // Initialize LacrosDBusThreadManager for the browser.
  LacrosDBusThreadManager::Initialize();

  // Initialize Chrome D-Bus clients.
  dbus::Bus* bus = LacrosDBusThreadManager::Get()->GetSystemBus();

  InitializeDBusClient<PermissionBrokerClient>(bus);

  InitializeDBusClient<MissiveClient>(bus);
}

void LacrosInitializeFeatureListDependentDBus() {
  dbus::Bus* bus = LacrosDBusThreadManager::Get()->GetSystemBus();
  if (base::FeatureList::IsEnabled(floss::features::kFlossEnabled)) {
    InitializeDBusClient<floss::FlossDBusManager>(bus);
  } else {
    InitializeDBusClient<bluez::BluezDBusManager>(bus);
  }
}

void LacrosShutdownDBus() {
  // Shut down D-Bus clients in reverse order of initialization.
  if (base::FeatureList::IsEnabled(floss::features::kFlossEnabled)) {
    floss::FlossDBusManager::Shutdown();
  } else {
    bluez::BluezDBusManager::Shutdown();
  }

  MissiveClient::Shutdown();

  PermissionBrokerClient::Shutdown();

  LacrosDBusThreadManager::Shutdown();
}

}  // namespace chromeos
