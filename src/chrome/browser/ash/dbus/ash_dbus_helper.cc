// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/ash_dbus_helper.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_client.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/components/chromebox_for_meetings/buildflags/buildflags.h"  // PLATFORM_CFM
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/arc/arc_camera_client.h"
#include "chromeos/dbus/arc/arc_sensor_service_client.h"
#include "chromeos/dbus/attestation/attestation_client.h"
#include "chromeos/dbus/audio/cras_audio_client.h"
#include "chromeos/dbus/authpolicy/authpolicy_client.h"
#include "chromeos/dbus/biod/biod_client.h"
#include "chromeos/dbus/cdm_factory_daemon/cdm_factory_daemon_client.h"
#include "chromeos/dbus/cicerone/cicerone_client.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cups_proxy/cups_proxy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/federated/federated_client.h"
#include "chromeos/dbus/hermes/hermes_clients.h"
#include "chromeos/dbus/init/initialize_dbus_client.h"
#include "chromeos/dbus/ip_peripheral/ip_peripheral_service_client.h"
#include "chromeos/dbus/kerberos/kerberos_client.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/dbus/media_analytics/media_analytics_client.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "chromeos/dbus/os_install/os_install_client.h"
#include "chromeos/dbus/pciguard/pciguard_client.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/resourced/resourced_client.h"
#include "chromeos/dbus/rmad/rmad_client.h"
#include "chromeos/dbus/seneschal/seneschal_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "chromeos/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/typecd/typecd_client.h"
#include "chromeos/dbus/u2f/u2f_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/dbus/userdataauth/arc_quota_client.h"
#include "chromeos/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/dbus/userdataauth/cryptohome_pkcs11_client.h"
#include "chromeos/dbus/userdataauth/install_attributes_client.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/tpm/install_attributes.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

#if BUILDFLAG(PLATFORM_CFM)
#include "chromeos/components/chromebox_for_meetings/features/features.h"
#include "chromeos/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#endif

namespace {

// If running on desktop, override paths so that enrollment and cloud policy
// work correctly, and can be tested.
void OverrideStubPathsIfNeeded() {
  base::FilePath user_data_dir;
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    chromeos::RegisterStubPathOverrides(user_data_dir);
    chromeos::dbus_paths::RegisterStubPathOverrides(user_data_dir);
  }
}

}  // namespace

namespace ash {

void InitializeDBus() {
  using chromeos::InitializeDBusClient;

  OverrideStubPathsIfNeeded();

  chromeos::SystemSaltGetter::Initialize();

  // Initialize DBusThreadManager for the browser.
  chromeos::DBusThreadManager::Initialize();

  // Initialize Chrome dbus clients.
  dbus::Bus* bus = chromeos::DBusThreadManager::Get()->GetSystemBus();

  // NOTE: base::Feature is not initialized yet, so any non MultiProcessMash
  // dbus client initialization for Ash should be done in Shell::Init.
  InitializeDBusClient<chromeos::ArcCameraClient>(bus);
  InitializeDBusClient<chromeos::ArcQuotaClient>(bus);
  InitializeDBusClient<chromeos::ArcSensorServiceClient>(bus);
  InitializeDBusClient<chromeos::AttestationClient>(bus);
  InitializeDBusClient<chromeos::AuthPolicyClient>(bus);
  InitializeDBusClient<chromeos::BiodClient>(bus);  // For device::Fingerprint.
  InitializeDBusClient<chromeos::CdmFactoryDaemonClient>(bus);
  InitializeDBusClient<chromeos::CiceroneClient>(bus);
  // ConciergeClient depends on CiceroneClient.
  InitializeDBusClient<chromeos::ConciergeClient>(bus);
  InitializeDBusClient<chromeos::CrasAudioClient>(bus);
  InitializeDBusClient<chromeos::CrosHealthdClient>(bus);
  InitializeDBusClient<chromeos::CryptohomeMiscClient>(bus);
  InitializeDBusClient<chromeos::CryptohomePkcs11Client>(bus);
  InitializeDBusClient<chromeos::CupsProxyClient>(bus);
  InitializeDBusClient<chromeos::DlcserviceClient>(bus);
  InitializeDBusClient<chromeos::DlpClient>(bus);
  InitializeDBusClient<chromeos::FederatedClient>(bus);
  chromeos::hermes_clients::Initialize(bus);
  InitializeDBusClient<chromeos::InstallAttributesClient>(bus);
  InitializeDBusClient<chromeos::IpPeripheralServiceClient>(bus);
  InitializeDBusClient<chromeos::KerberosClient>(bus);
  InitializeDBusClient<chromeos::MachineLearningClient>(bus);
  InitializeDBusClient<chromeos::MediaAnalyticsClient>(bus);
  InitializeDBusClient<chromeos::MissiveClient>(bus);
  InitializeDBusClient<chromeos::OsInstallClient>(bus);
  InitializeDBusClient<chromeos::PciguardClient>(bus);
  InitializeDBusClient<chromeos::PermissionBrokerClient>(bus);
  InitializeDBusClient<chromeos::PowerManagerClient>(bus);
  InitializeDBusClient<chromeos::ResourcedClient>(bus);
  InitializeDBusClient<chromeos::SeneschalClient>(bus);
  InitializeDBusClient<chromeos::SessionManagerClient>(bus);
  InitializeDBusClient<chromeos::SystemClockClient>(bus);
  InitializeDBusClient<chromeos::SystemProxyClient>(bus);
  InitializeDBusClient<chromeos::TpmManagerClient>(bus);
  InitializeDBusClient<chromeos::TypecdClient>(bus);
  InitializeDBusClient<chromeos::U2FClient>(bus);
  InitializeDBusClient<chromeos::UserDataAuthClient>(bus);
  InitializeDBusClient<chromeos::UpstartClient>(bus);

  // Initialize the device settings service so that we'll take actions per
  // signals sent from the session manager. This needs to happen before
  // g_browser_process initializes BrowserPolicyConnector.
  chromeos::DeviceSettingsService::Initialize();
  chromeos::InstallAttributes::Initialize();
}

void InitializeFeatureListDependentDBus() {
  using chromeos::InitializeDBusClient;

  dbus::Bus* bus = chromeos::DBusThreadManager::Get()->GetSystemBus();
  InitializeDBusClient<bluez::BluezDBusManager>(bus);
#if BUILDFLAG(PLATFORM_CFM)
  if (base::FeatureList::IsEnabled(chromeos::cfm::features::kMojoServices)) {
    InitializeDBusClient<chromeos::CfmHotlineClient>(bus);
  }
#endif
  if (ash::features::IsShimlessRMAFlowEnabled()) {
    InitializeDBusClient<chromeos::RmadClient>(bus);
  }
  InitializeDBusClient<chromeos::WilcoDtcSupportdClient>(bus);
}

void ShutdownDBus() {
  // Feature list-dependent D-Bus clients are shut down first because we try to
  // shut down in reverse order of initialization (in case of dependencies).
  chromeos::WilcoDtcSupportdClient::Shutdown();
#if BUILDFLAG(PLATFORM_CFM)
  if (base::FeatureList::IsEnabled(chromeos::cfm::features::kMojoServices)) {
    chromeos::CfmHotlineClient::Shutdown();
  }
#endif
  bluez::BluezDBusManager::Shutdown();

  // Other D-Bus clients are shut down, also in reverse order of initialization.
  chromeos::UpstartClient::Shutdown();
  chromeos::UserDataAuthClient::Shutdown();
  chromeos::U2FClient::Shutdown();
  chromeos::TypecdClient::Shutdown();
  chromeos::TpmManagerClient::Shutdown();
  chromeos::SystemProxyClient::Shutdown();
  chromeos::SystemClockClient::Shutdown();
  chromeos::SessionManagerClient::Shutdown();
  chromeos::SeneschalClient::Shutdown();
  chromeos::ResourcedClient::Shutdown();
  if (ash::features::IsShimlessRMAFlowEnabled()) {
    chromeos::RmadClient::Shutdown();
  }
  chromeos::PowerManagerClient::Shutdown();
  chromeos::PermissionBrokerClient::Shutdown();
  chromeos::PciguardClient::Shutdown();
  chromeos::OsInstallClient::Shutdown();
  chromeos::MissiveClient::Shutdown();
  chromeos::MediaAnalyticsClient::Shutdown();
  chromeos::MachineLearningClient::Shutdown();
  chromeos::KerberosClient::Shutdown();
  chromeos::IpPeripheralServiceClient::Shutdown();
  chromeos::InstallAttributesClient::Shutdown();
  chromeos::hermes_clients::Shutdown();
  chromeos::FederatedClient::Shutdown();
  chromeos::DlcserviceClient::Shutdown();
  chromeos::DlpClient::Shutdown();
  chromeos::CupsProxyClient::Shutdown();
  chromeos::CryptohomePkcs11Client::Shutdown();
  chromeos::CryptohomeMiscClient::Shutdown();
  chromeos::CrosHealthdClient::Shutdown();
  chromeos::CrasAudioClient::Shutdown();
  chromeos::ConciergeClient::Shutdown();
  chromeos::CiceroneClient::Shutdown();
  chromeos::CdmFactoryDaemonClient::Shutdown();
  chromeos::BiodClient::Shutdown();
  chromeos::AuthPolicyClient::Shutdown();
  chromeos::AttestationClient::Shutdown();
  chromeos::ArcQuotaClient::Shutdown();
  chromeos::ArcCameraClient::Shutdown();

  chromeos::DBusThreadManager::Shutdown();
  chromeos::SystemSaltGetter::Shutdown();
}

}  // namespace ash
