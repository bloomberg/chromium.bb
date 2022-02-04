// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_handler.h"

#include "ash/constants/ash_features.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/network/auto_connect_handler.h"
#include "chromeos/network/cellular_connection_handler.h"
#include "chromeos/network/cellular_esim_installer.h"
#include "chromeos/network/cellular_esim_profile_handler_impl.h"
#include "chromeos/network/cellular_esim_uninstall_handler.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/cellular_metrics_logger.h"
#include "chromeos/network/cellular_policy_handler.h"
#include "chromeos/network/client_cert_resolver.h"
#include "chromeos/network/geolocation_handler.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/metrics/connection_info_metrics_logger.h"
#include "chromeos/network/metrics/esim_policy_login_metrics_logger.h"
#include "chromeos/network/metrics/vpn_network_metrics_helper.h"
#include "chromeos/network/network_activation_handler_impl.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_cert_migrator.h"
#include "chromeos/network/network_certificate_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler_impl.h"
#include "chromeos/network/network_device_handler_impl.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_profile_observer.h"
#include "chromeos/network/network_sms_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/prohibited_technologies_handler.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "chromeos/network/stub_cellular_networks_provider.h"

namespace chromeos {

static NetworkHandler* g_network_handler = NULL;

NetworkHandler::NetworkHandler()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  network_state_handler_.reset(new NetworkStateHandler());
  network_device_handler_.reset(new NetworkDeviceHandlerImpl());
  cellular_inhibitor_.reset(new CellularInhibitor());
  cellular_esim_profile_handler_.reset(new CellularESimProfileHandlerImpl());
  stub_cellular_networks_provider_.reset(new StubCellularNetworksProvider());
  cellular_connection_handler_.reset(new CellularConnectionHandler());
  network_profile_handler_.reset(new NetworkProfileHandler());
  network_configuration_handler_.reset(new NetworkConfigurationHandler());
  managed_network_configuration_handler_.reset(
      new ManagedNetworkConfigurationHandlerImpl());
  network_connection_handler_.reset(new NetworkConnectionHandlerImpl());
  cellular_esim_installer_.reset(new CellularESimInstaller());
  cellular_esim_uninstall_handler_.reset(new CellularESimUninstallHandler());
  if (features::IsESimPolicyEnabled()) {
    cellular_policy_handler_.reset(new CellularPolicyHandler());
    esim_policy_login_metrics_logger_.reset(new ESimPolicyLoginMetricsLogger());
  }
  cellular_metrics_logger_.reset(new CellularMetricsLogger());
  connection_info_metrics_logger_.reset(new ConnectionInfoMetricsLogger());
  vpn_network_metrics_helper_.reset(new VpnNetworkMetricsHelper());
  if (NetworkCertLoader::IsInitialized()) {
    network_cert_migrator_.reset(new NetworkCertMigrator());
    client_cert_resolver_.reset(new ClientCertResolver());
    auto_connect_handler_.reset(new AutoConnectHandler());
    network_certificate_handler_.reset(new NetworkCertificateHandler());
  }
  network_activation_handler_.reset(new NetworkActivationHandlerImpl());
  prohibited_technologies_handler_.reset(new ProhibitedTechnologiesHandler());
  network_sms_handler_.reset(new NetworkSmsHandler());
  geolocation_handler_.reset(new GeolocationHandler());
}

NetworkHandler::~NetworkHandler() {
  network_state_handler_->Shutdown();
}

void NetworkHandler::Init() {
  network_state_handler_->InitShillPropertyHandler();
  network_device_handler_->Init(network_state_handler_.get());
  cellular_inhibitor_->Init(network_state_handler_.get(),
                            network_device_handler_.get());
  cellular_esim_profile_handler_->Init(network_state_handler_.get(),
                                       cellular_inhibitor_.get());
  stub_cellular_networks_provider_->Init(network_state_handler_.get(),
                                         cellular_esim_profile_handler_.get());
  cellular_connection_handler_->Init(network_state_handler_.get(),
                                     cellular_inhibitor_.get(),
                                     cellular_esim_profile_handler_.get());
  network_profile_handler_->Init();
  network_configuration_handler_->Init(network_state_handler_.get(),
                                       network_device_handler_.get());
  managed_network_configuration_handler_->Init(
      cellular_policy_handler_.get(), network_state_handler_.get(),
      network_profile_handler_.get(), network_configuration_handler_.get(),
      network_device_handler_.get(), prohibited_technologies_handler_.get());
  network_connection_handler_->Init(
      network_state_handler_.get(), network_configuration_handler_.get(),
      managed_network_configuration_handler_.get(),
      cellular_connection_handler_.get());
  cellular_esim_installer_->Init(
      cellular_connection_handler_.get(), cellular_inhibitor_.get(),
      network_connection_handler_.get(), network_profile_handler_.get(),
      network_state_handler_.get());
  cellular_esim_uninstall_handler_->Init(
      cellular_inhibitor_.get(), cellular_esim_profile_handler_.get(),
      network_configuration_handler_.get(), network_connection_handler_.get(),
      network_state_handler_.get());
  if (features::IsESimPolicyEnabled()) {
    cellular_policy_handler_->Init(
        cellular_esim_installer_.get(), network_profile_handler_.get(),
        managed_network_configuration_handler_.get());
    esim_policy_login_metrics_logger_->Init(
        network_state_handler_.get(),
        managed_network_configuration_handler_.get());
  }
  cellular_metrics_logger_->Init(network_state_handler_.get(),
                                 network_connection_handler_.get(),
                                 cellular_esim_profile_handler_.get());
  connection_info_metrics_logger_->Init(network_state_handler_.get(),
                                        network_connection_handler_.get());
  vpn_network_metrics_helper_->Init(network_configuration_handler_.get());
  if (network_cert_migrator_)
    network_cert_migrator_->Init(network_state_handler_.get());
  if (client_cert_resolver_) {
    client_cert_resolver_->Init(network_state_handler_.get(),
                                managed_network_configuration_handler_.get());
  }
  if (auto_connect_handler_) {
    auto_connect_handler_->Init(client_cert_resolver_.get(),
                                network_connection_handler_.get(),
                                network_state_handler_.get(),
                                managed_network_configuration_handler_.get());
  }
  prohibited_technologies_handler_->Init(
      managed_network_configuration_handler_.get(),
      network_state_handler_.get());
  network_sms_handler_->Init();
  geolocation_handler_->Init();
}

// static
void NetworkHandler::Initialize() {
  CHECK(!g_network_handler);
  g_network_handler = new NetworkHandler();
  g_network_handler->Init();
}

// static
void NetworkHandler::Shutdown() {
  CHECK(g_network_handler);
  delete g_network_handler;
  g_network_handler = NULL;
}

// static
NetworkHandler* NetworkHandler::Get() {
  CHECK(g_network_handler)
      << "NetworkHandler::Get() called before Initialize()";
  return g_network_handler;
}

// static
bool NetworkHandler::IsInitialized() {
  return g_network_handler;
}

void NetworkHandler::InitializePrefServices(
    PrefService* logged_in_profile_prefs,
    PrefService* device_prefs) {
  cellular_esim_profile_handler_->SetDevicePrefs(device_prefs);
  ui_proxy_config_service_.reset(new UIProxyConfigService(
      logged_in_profile_prefs, device_prefs, network_state_handler_.get(),
      network_profile_handler_.get()));
  managed_network_configuration_handler_->set_ui_proxy_config_service(
      ui_proxy_config_service_.get());
  network_metadata_store_.reset(new NetworkMetadataStore(
      network_configuration_handler_.get(), network_connection_handler_.get(),
      network_state_handler_.get(), logged_in_profile_prefs, device_prefs,
      is_enterprise_managed_));
}

void NetworkHandler::ShutdownPrefServices() {
  cellular_esim_profile_handler_->SetDevicePrefs(nullptr);
  ui_proxy_config_service_.reset();
  network_metadata_store_.reset();
}

bool NetworkHandler::HasUiProxyConfigService() {
  return IsInitialized() && Get()->ui_proxy_config_service_.get();
}

UIProxyConfigService* NetworkHandler::GetUiProxyConfigService() {
  DCHECK(HasUiProxyConfigService());
  return Get()->ui_proxy_config_service_.get();
}

NetworkStateHandler* NetworkHandler::network_state_handler() {
  return network_state_handler_.get();
}

AutoConnectHandler* NetworkHandler::auto_connect_handler() {
  return auto_connect_handler_.get();
}

CellularConnectionHandler* NetworkHandler::cellular_connection_handler() {
  return cellular_connection_handler_.get();
}

CellularESimInstaller* NetworkHandler::cellular_esim_installer() {
  return cellular_esim_installer_.get();
}

CellularESimProfileHandler* NetworkHandler::cellular_esim_profile_handler() {
  return cellular_esim_profile_handler_.get();
}

CellularESimUninstallHandler*
NetworkHandler::cellular_esim_uninstall_handler() {
  return cellular_esim_uninstall_handler_.get();
}

CellularInhibitor* NetworkHandler::cellular_inhibitor() {
  return cellular_inhibitor_.get();
}

CellularPolicyHandler* NetworkHandler::cellular_policy_handler() {
  return cellular_policy_handler_.get();
}

NetworkDeviceHandler* NetworkHandler::network_device_handler() {
  return network_device_handler_.get();
}

NetworkProfileHandler* NetworkHandler::network_profile_handler() {
  return network_profile_handler_.get();
}

NetworkConfigurationHandler* NetworkHandler::network_configuration_handler() {
  return network_configuration_handler_.get();
}

ManagedNetworkConfigurationHandler*
NetworkHandler::managed_network_configuration_handler() {
  return managed_network_configuration_handler_.get();
}

NetworkActivationHandler* NetworkHandler::network_activation_handler() {
  return network_activation_handler_.get();
}

NetworkCertificateHandler* NetworkHandler::network_certificate_handler() {
  return network_certificate_handler_.get();
}

NetworkConnectionHandler* NetworkHandler::network_connection_handler() {
  return network_connection_handler_.get();
}

NetworkMetadataStore* NetworkHandler::network_metadata_store() {
  return network_metadata_store_.get();
}

NetworkSmsHandler* NetworkHandler::network_sms_handler() {
  return network_sms_handler_.get();
}

GeolocationHandler* NetworkHandler::geolocation_handler() {
  return geolocation_handler_.get();
}

ProhibitedTechnologiesHandler*
NetworkHandler::prohibited_technologies_handler() {
  return prohibited_technologies_handler_.get();
}

void NetworkHandler::SetIsEnterpriseManaged(bool is_enterprise_managed) {
  is_enterprise_managed_ = is_enterprise_managed;
  if (esim_policy_login_metrics_logger_) {
    // Call SetIsEnterpriseManaged on ESimPolicyLoginMetricsLogger, this only
    // gets called when the primary user logs in.
    esim_policy_login_metrics_logger_->SetIsEnterpriseManaged(
        is_enterprise_managed);
  }
}

}  // namespace chromeos
