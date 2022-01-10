// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_context_keyed_service_factories.h"

#include "chrome/browser/ash/android_sms/android_sms_service_factory.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_helper_bridge.h"
#include "chrome/browser/ash/authpolicy/authpolicy_credentials_manager.h"
#include "chrome/browser/ash/bluetooth/debug_logs_manager_factory.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler_user_service.h"
#include "chrome/browser/ash/crostini/crostini_engagement_metrics_service.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/file_system_provider/service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/ash/nearby/nearby_connections_dependencies_provider_factory.h"
#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/ash/platform_keys/key_permissions/user_private_token_kpm_service_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_engagement_metrics_service.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_token_forwarder_factory.h"
#include "chrome/browser/ash/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/ash/policy/networking/user_network_configuration_updater_factory.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/ash/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/secure_channel/nearby_connector_factory.h"
#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/browser/ash/tether/tether_service_factory.h"
#include "chrome/browser/ash/web_applications/crosh_loader_factory.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/chromeos/extensions/printing_metrics/print_job_finished_event_dispatcher.h"
#include "chrome/browser/chromeos/fileapi/file_change_service_factory.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ui/ash/calendar/calendar_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"

#if defined(USE_CUPS)
#include "chrome/browser/chromeos/printing/cups_proxy_service_manager_factory.h"
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#endif

namespace ash {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  android_sms::AndroidSmsServiceFactory::GetInstance();
  arc::ArcAccessibilityHelperBridge::CreateFactory();
  CalendarKeyedServiceFactory::GetInstance();
  full_restore::FullRestoreServiceFactory::GetInstance();
  HoldingSpaceKeyedServiceFactory::GetInstance();
  AuthPolicyCredentialsManagerFactory::GetInstance();
  bluetooth::DebugLogsManagerFactory::GetInstance();
  borealis::BorealisServiceFactory::GetInstance();
  cert_provisioning::CertProvisioningSchedulerUserServiceFactory::GetInstance();
  CroshLoaderFactory::GetInstance();
  crostini::CrostiniEngagementMetricsService::Factory::GetInstance();
#if defined(USE_CUPS)
  chromeos::CupsProxyServiceManagerFactory::GetInstance();
#endif
  chromeos::CupsPrintersManagerFactory::GetInstance();
  chromeos::CupsPrintJobManagerFactory::GetInstance();
  EasyUnlockServiceFactory::GetInstance();
  eche_app::EcheAppManagerFactory::GetInstance();
  extensions::InputMethodAPI::GetFactoryInstance();
  extensions::MediaPlayerAPI::GetFactoryInstance();
#if defined(USE_CUPS)
  extensions::PrintingAPIHandler::GetFactoryInstance();
  extensions::PrintJobFinishedEventDispatcher::GetFactoryInstance();
#endif
  FileChangeServiceFactory::GetInstance();
  file_manager::EventRouterFactory::GetInstance();
  file_manager::VolumeManagerFactory::GetInstance();
  file_system_provider::ServiceFactory::GetInstance();
  guest_os::GuestOsRegistryServiceFactory::GetInstance();
  KerberosCredentialsManagerFactory::GetInstance();
  nearby::NearbyConnectionsDependenciesProviderFactory::GetInstance();
  nearby::NearbyProcessManagerFactory::GetInstance();
  OwnerSettingsServiceAshFactory::GetInstance();
  phonehub::PhoneHubManagerFactory::GetInstance();
  platform_keys::KeyPermissionsServiceFactory::GetInstance();
  platform_keys::UserPrivateTokenKeyPermissionsManagerServiceFactory::
      GetInstance();
  plugin_vm::PluginVmEngagementMetricsService::Factory::GetInstance();
  policy::PolicyCertServiceFactory::GetInstance();
  policy::UserCloudPolicyTokenForwarderFactory::GetInstance();
  policy::UserNetworkConfigurationUpdaterFactory::GetInstance();
  printing::print_management::PrintingManagerFactory::GetInstance();
  PrintJobHistoryServiceFactory::GetInstance();
  secure_channel::NearbyConnectorFactory::GetInstance();
  smb_client::SmbServiceFactory::GetInstance();
  SyncedPrintersManagerFactory::GetInstance();
  tether::TetherServiceFactory::GetInstance();
}

}  // namespace ash
