// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/dbus_helper.h"

#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/arc_camera_client.h"
#include "chromeos/dbus/audio/cras_audio_client.h"
#include "chromeos/dbus/auth_policy/auth_policy_client.h"
#include "chromeos/dbus/biod/biod_client.h"
#include "chromeos/dbus/cups_proxy/cups_proxy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/kerberos/kerberos_client.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/dbus/media_analytics/media_analytics_client.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/tpm/install_attributes.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

namespace {

void OverrideStubPathsIfNeeded() {
  base::FilePath user_data_dir;
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    chromeos::RegisterStubPathOverrides(user_data_dir);
  }
}

}  // namespace

namespace chromeos {

void InitializeDBus() {
  OverrideStubPathsIfNeeded();

  SystemSaltGetter::Initialize();

  // Initialize DBusThreadManager for the browser.
  DBusThreadManager::Initialize(DBusThreadManager::kAll);

  // Initialize Chrome dbus clients.
  dbus::Bus* bus = DBusThreadManager::Get()->GetSystemBus();

  // NOTE: base::Feature is not initialized yet, so any non MultiProcessMash
  // dbus client initialization for Ash should be done in Shell::Init.

  if (bus) {
    ArcCameraClient::Initialize(bus);
    AuthPolicyClient::Initialize(bus);
    BiodClient::Initialize(bus);  // For device::Fingerprint.
    bluez::BluezDBusManager::Initialize(bus);
    CrasAudioClient::Initialize(bus);
    CryptohomeClient::Initialize(bus);
    CupsProxyClient::Initialize(bus);
    KerberosClient::Initialize(bus);
    MachineLearningClient::Initialize(bus);
    MediaAnalyticsClient::Initialize(bus);
    PermissionBrokerClient::Initialize(bus);
    PowerManagerClient::Initialize(bus);
    SessionManagerClient::Initialize(bus);
    SystemClockClient::Initialize(bus);
    UpstartClient::Initialize(bus);
  } else {
    ArcCameraClient::InitializeFake();
    AuthPolicyClient::InitializeFake();
    BiodClient::InitializeFake();  // For device::Fingerprint.
    bluez::BluezDBusManager::InitializeFake();
    CrasAudioClient::InitializeFake();
    CryptohomeClient::InitializeFake();
    CupsProxyClient::InitializeFake();
    KerberosClient::InitializeFake();
    MachineLearningClient::InitializeFake();
    MediaAnalyticsClient::InitializeFake();
    PermissionBrokerClient::InitializeFake();
    PowerManagerClient::InitializeFake();
    SessionManagerClient::InitializeFake();
    SystemClockClient::InitializeFake();
    UpstartClient::InitializeFake();
  }

  // Initialize the device settings service so that we'll take actions per
  // signals sent from the session manager. This needs to happen before
  // g_browser_process initializes BrowserPolicyConnector.
  DeviceSettingsService::Initialize();
  InstallAttributes::Initialize();
}

void ShutdownDBus() {
  UpstartClient::Shutdown();
  SystemClockClient::Shutdown();
  SessionManagerClient::Shutdown();
  PowerManagerClient::Shutdown();
  PermissionBrokerClient::Shutdown();
  MediaAnalyticsClient::Shutdown();
  MachineLearningClient::Shutdown();
  KerberosClient::Shutdown();
  CupsProxyClient::Shutdown();
  CryptohomeClient::Shutdown();
  CrasAudioClient::Shutdown();
  bluez::BluezDBusManager::Shutdown();
  BiodClient::Shutdown();
  AuthPolicyClient::Shutdown();
  ArcCameraClient::Shutdown();

  DBusThreadManager::Shutdown();
  SystemSaltGetter::Shutdown();
}

}  // namespace chromeos
