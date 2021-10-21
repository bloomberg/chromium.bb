// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/network_health_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/diagnostics_ui/backend/networking_log.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/cellular_esim_profile_handler_impl.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/system_token_cert_db_storage.h"
#include "chromeos/services/network_config/cros_network_config.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-shared.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace diagnostics {
namespace {

constexpr char kEth0DevicePath[] = "/device/eth0";
constexpr char kEth0Name[] = "eth0_name";
constexpr char kEth0NetworkGuid[] = "eth0_network_guid";
constexpr char kWlan0DevicePath[] = "/device/wlan0";
constexpr char kWlan0Name[] = "wlan0_name";
constexpr char kWlan0NetworkGuid[] = "wlan0_network_guid";
constexpr char kCellular0DevicePath[] = "/device/cellular0";
constexpr char kCellular0Name[] = "cellular0_name";
constexpr char kCellular0NetworkGuid[] = "cellular0_network_guid";
constexpr char kFormattedMacAddress[] = "01:23:45:67:89:AB";
constexpr char kTestIPConfigPath[] = "test_ip_config_path";

// TODO(https://crbug.com/1164001): remove when network_config is moved to ash.
namespace network_config = ::chromeos::network_config;

class FakeNetworkListObserver : public mojom::NetworkListObserver {
 public:
  void OnNetworkListChanged(const std::vector<std::string>& observer_guids,
                            const std::string& active_guid) override {
    observer_guids_ = observer_guids;
    active_guid_ = active_guid;
    call_count_++;
  }

  mojo::PendingRemote<mojom::NetworkListObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const std::vector<std::string>& observer_guids() const {
    return observer_guids_;
  }

  const std::string& active_guid() const { return active_guid_; }

  size_t call_count() const { return call_count_; }

 private:
  std::vector<std::string> observer_guids_;
  std::string active_guid_;
  size_t call_count_ = 0;
  mojo::Receiver<mojom::NetworkListObserver> receiver_{this};
};

class FakeNetworkStateObserver : public mojom::NetworkStateObserver {
 public:
  void OnNetworkStateChanged(mojom::NetworkPtr network_ptr) override {
    network_state_updates_.push_back(std::move(network_ptr));
  }

  mojo::PendingRemote<mojom::NetworkStateObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const mojom::NetworkPtr& GetLatestState() const {
    DCHECK(HasFired());
    return network_state_updates_.back();
  }

  size_t GetCallCount() const { return network_state_updates_.size(); }

  bool HasFired() const { return GetCallCount() > 0; }

 private:
  // Tracks calls to OnNetworkStateChanged. Each call adds an element to
  // the vector.
  std::vector<mojom::NetworkPtr> network_state_updates_;
  mojo::Receiver<mojom::NetworkStateObserver> receiver_{this};
};

// Expects that the call count increases and returns the new call count.
void ExpectListObserverFired(const FakeNetworkListObserver& observer,
                             size_t* prior_call_count) {
  DCHECK(prior_call_count);
  const size_t current_call_count = observer.call_count();
  EXPECT_GT(current_call_count, *prior_call_count);
  *prior_call_count = current_call_count;
}

// Expects that the call count increases and returns the new call count.
void ExpectStateObserverFired(const FakeNetworkStateObserver& observer,
                              size_t* prior_call_count) {
  DCHECK(prior_call_count);
  const size_t current_call_count = observer.GetCallCount();
  EXPECT_GT(current_call_count, *prior_call_count);
  *prior_call_count = current_call_count;
}

}  // namespace

class NetworkHealthProviderTest : public testing::Test {
 public:
  NetworkHealthProviderTest() {
    LoginState::Initialize();
    SystemTokenCertDbStorage::Initialize();

    // NetworkHandler has pieces that depend on NetworkCertLoader so it's better
    // to initialize NetworkHandlerTestHelper after
    // NetworkCertLoader::Initialize(). Same with CrosNetworkConfig since it
    // depends on NetworkHandler
    NetworkCertLoader::Initialize();
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();
    ClearDevicesAndServices();
    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    ::onc::RegisterProfilePrefs(user_prefs_.registry());
    ::onc::RegisterPrefs(local_state_.registry());
    chromeos::NetworkMetadataStore::RegisterPrefs(user_prefs_.registry());
    chromeos::NetworkMetadataStore::RegisterPrefs(local_state_.registry());
    chromeos::CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(
        local_state_.registry());
    NetworkHandler::Get()->InitializePrefServices(&user_prefs_, &local_state_);

    cros_network_config_ =
        std::make_unique<network_config::CrosNetworkConfig>();

    network_config::OverrideInProcessInstanceForTesting(
        cros_network_config_.get());
    base::RunLoop().RunUntilIdle();

    ManagedNetworkConfigurationHandler* managed_network_configuration_handler =
        NetworkHandler::Get()->managed_network_configuration_handler();
    managed_network_configuration_handler->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY,
        /*userhash=*/std::string(),
        /*network_configs_onc=*/base::ListValue(),
        /*global_network_config=*/base::DictionaryValue());

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    network_health_provider_ = std::make_unique<NetworkHealthProvider>();
  }

  ~NetworkHealthProviderTest() override {
    // Ordering here is based on dependencies between classes,
    // CrosNetworkConfig depends on NetworkHandler and NetworkHandler
    // indirectly depends on NetworkCertLoader.
    network_health_provider_.reset();
    cros_network_config_.reset();
    network_handler_test_helper_.reset();
    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
    LoginState::Shutdown();
  }

 protected:
  void CreateEthernetDevice() {
    network_handler_test_helper_->manager_test()->AddTechnology(
        shill::kTypeEthernet, true);
    network_handler_test_helper_->device_test()->AddDevice(
        kEth0DevicePath, shill::kTypeEthernet, kEth0Name);

    base::RunLoop().RunUntilIdle();
  }

  void CreateWifiDevice() {
    network_handler_test_helper_->manager_test()->AddTechnology(
        shill::kTypeWifi, true);
    network_handler_test_helper_->device_test()->AddDevice(
        kWlan0DevicePath, shill::kTypeWifi, kWlan0Name);

    base::RunLoop().RunUntilIdle();
  }

  void AssociateIPConfigWithWifiDevice() {
    network_handler_test_helper_->device_test()->SetDeviceProperty(
        kWlan0DevicePath, shill::kSavedIPConfigProperty,
        base::Value(kTestIPConfigPath),
        /*notify_changed=*/true);

    base::RunLoop().RunUntilIdle();
  }

  void CreateVpnDevice() {
    network_handler_test_helper_->manager_test()->AddTechnology(shill::kTypeVPN,
                                                                true);
    network_handler_test_helper_->device_test()->AddDevice(
        "/device/vpn", shill::kTypeVPN, "vpn_name");

    base::RunLoop().RunUntilIdle();
  }

  // The device must have been created with CreateEthernetDevice().
  void AssociateEthernet() {
    network_handler_test_helper_->service_test()->AddService(
        kEth0DevicePath, kEth0NetworkGuid, kEth0Name, shill::kTypeEthernet,
        shill::kStateAssociation, true);

    base::RunLoop().RunUntilIdle();
  }

  void AssociateWifi() {
    network_handler_test_helper_->service_test()->AddService(
        kWlan0DevicePath, kWlan0NetworkGuid, kWlan0Name, shill::kTypeWifi,
        shill::kStateAssociation, true);

    base::RunLoop().RunUntilIdle();
  }

  void AssociateWifiWithIPConfig() {
    network_handler_test_helper_->service_test()->AddServiceWithIPConfig(
        kWlan0DevicePath, kWlan0NetworkGuid, kWlan0Name, shill::kTypeWifi,
        shill::kStateAssociation, kTestIPConfigPath, true);

    base::RunLoop().RunUntilIdle();
  }

  void AssociateAndConnectVpn() {
    network_handler_test_helper_->service_test()->AddService(
        "/device/vpn", "vpn guid", "vpn_name", shill::kTypeVPN,
        shill::kStateAssociation, true);

    SetNetworkState("/device/vpn", shill::kStateOnline);
    base::RunLoop().RunUntilIdle();
  }

  void SetNetworkState(const std::string& device_path,
                       const std::string& state) {
    network_handler_test_helper_->SetServiceProperty(
        device_path, shill::kStateProperty, base::Value(state));
    base::RunLoop().RunUntilIdle();
  }

  void SetEthernetConnected() {
    // NOTE: `kStateReady` is connected but not "online".
    SetNetworkState(kEth0DevicePath, shill::kStateReady);
  }

  void SetEthernetOnline() {
    SetNetworkState(kEth0DevicePath, shill::kStateOnline);
  }

  void SetEthernetDisconnected() {
    SetNetworkState(kEth0DevicePath, shill::kStateOffline);
  }

  void SetDeviceState(const std::string& type, bool enabled) {
    chromeos::NetworkTypePattern pattern = type == shill::kTypeEthernet
                                               ? NetworkTypePattern::Ethernet()
                                               : NetworkTypePattern::WiFi();
    NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
        pattern, enabled, network_handler::ErrorCallback());
    base::RunLoop().RunUntilIdle();
  }

  void SetEthernetDisabled() { SetDeviceState(shill::kTypeEthernet, false); }

  void SetEthernetEnabled() { SetDeviceState(shill::kTypeEthernet, true); }

  void SetWifiDisabled() { SetDeviceState(shill::kTypeWifi, false); }

  void SetWifiEnabled() { SetDeviceState(shill::kTypeWifi, true); }

  void SetWifiConnected() {
    // NOTE: `kStateReady` is connected but not "online".
    SetNetworkState(kWlan0DevicePath, shill::kStateReady);
  }

  void SetWifiOnline() {
    SetNetworkState(kWlan0DevicePath, shill::kStateOnline);
  }

  void SetWifiDisconnected() {
    SetNetworkState(kWlan0DevicePath, shill::kStateOffline);
  }

  void SetWifiPortal() {
    SetNetworkState(kWlan0DevicePath, shill::kStatePortal);
  }

  void SetCellularConnected() {
    SetNetworkState(kCellular0DevicePath, shill::kStateReady);
  }

  void SetCellularDisconnected() {
    SetNetworkState(kCellular0DevicePath, shill::kStateOffline);
  }

  void SetCellularOnline() {
    SetNetworkState(kCellular0DevicePath, shill::kStateOnline);
  }

  void AssociateCellular() {
    network_handler_test_helper_->service_test()->AddService(
        kCellular0DevicePath, kCellular0NetworkGuid, kCellular0Name,
        shill::kTypeCellular, shill::kStateAssociation, true);
    network_handler_test_helper_->profile_test()->AddService(
        NetworkProfileHandler::GetSharedProfilePath(), kCellular0DevicePath);
    base::RunLoop().RunUntilIdle();
  }

  void CreateCellularDevice() {
    network_handler_test_helper_->manager_test()->AddTechnology(
        shill::kTypeCellular, true);
    network_handler_test_helper_->device_test()->AddDevice(
        kCellular0DevicePath, shill::kTypeCellular, kCellular0Name);

    base::RunLoop().RunUntilIdle();
  }

  void SetCellularProperty(std::string property, base::Value value) {
    network_handler_test_helper_->SetServiceProperty(kCellular0DevicePath,
                                                     property, value);
    base::RunLoop().RunUntilIdle();
  }

  void SetCellularIccid(std::string iccid) {
    SetCellularProperty(shill::kIccidProperty, base::Value(iccid));
  }

  void SetCellularNetworkTechnology(std::string technology) {
    SetCellularProperty(shill::kNetworkTechnologyProperty,
                        base::Value(technology));
  }

  void SetCellularEid(std::string eid) {
    SetCellularProperty(shill::kEidProperty, base::Value(eid));
  }

  void SetCellularSignalStrength(int signal_strength) {
    SetCellularProperty(shill::kSignalStrengthProperty,
                        base::Value(signal_strength));
  }

  void SetCellularSimLockStatus(std::string lock_type, bool sim_locked) {
    base::Value sim_lock_status(base::Value::Type::DICTIONARY);
    sim_lock_status.SetKey(shill::kSIMLockEnabledProperty,
                           base::Value(sim_locked));
    sim_lock_status.SetKey(shill::kSIMLockTypeProperty, base::Value(lock_type));
    sim_lock_status.SetKey(shill::kSIMLockRetriesLeftProperty, base::Value(3));
    network_handler_test_helper_->device_test()->SetDeviceProperty(
        kCellular0DevicePath, shill::kSIMLockStatusProperty,
        std::move(sim_lock_status),
        /*notify_changed=*/true);

    base::RunLoop().RunUntilIdle();
  }

  void SetCellularRoamingState(std::string roaming_state) {
    SetCellularProperty(shill::kRoamingStateProperty,
                        base::Value(roaming_state));
  }

  void SetWifiProperty(std::string property, base::Value value) {
    network_handler_test_helper_->SetServiceProperty(kWlan0DevicePath, property,
                                                     value);
    base::RunLoop().RunUntilIdle();
  }

  void SetWifiSignalStrength(int signal_strength) {
    SetWifiProperty(shill::kSignalStrengthProperty,
                    base::Value(signal_strength));
  }

  void SetWifiFrequency(int frequency) {
    SetWifiProperty(shill::kWifiFrequency, base::Value(frequency));
  }

  void SetWifiBssid(std::string bssid) {
    SetWifiProperty(shill::kWifiBSsid, base::Value(bssid));
  }

  void SetEthernetMacAddress(const std::string& mac_address) {
    network_handler_test_helper_->device_test()->SetDeviceProperty(
        kEth0DevicePath, shill::kAddressProperty, base::Value(mac_address),
        /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetWifiMacAddress(const std::string& mac_address) {
    network_handler_test_helper_->device_test()->SetDeviceProperty(
        kWlan0DevicePath, shill::kAddressProperty, base::Value(mac_address),
        /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetGatewayForIPConfig(const std::string& gateway) {
    chromeos::ShillIPConfigClient::Get()->SetProperty(
        dbus::ObjectPath(kTestIPConfigPath), shill::kGatewayProperty,
        base::Value(gateway), base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void SetIPAddressForIPConfig(const std::string& ip_address) {
    chromeos::ShillIPConfigClient::Get()->SetProperty(
        dbus::ObjectPath(kTestIPConfigPath), shill::kAddressProperty,
        base::Value(ip_address), base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void SetNameServersForIPConfig(const base::ListValue& dns_servers) {
    chromeos::ShillIPConfigClient::Get()->SetProperty(
        dbus::ObjectPath(kTestIPConfigPath), shill::kNameServersProperty,
        dns_servers, base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void SetRoutingPrefixForIPConfig(int routing_prefix) {
    chromeos::ShillIPConfigClient::Get()->SetProperty(
        dbus::ObjectPath(kTestIPConfigPath), shill::kPrefixlenProperty,
        base::Value(routing_prefix), base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void SetWifiSecurity(const std::string& securityClass,
                       const std::string& eapKeyMgmt) {
    SetWifiSecurity(securityClass);
    SetWifiProperty(shill::kEapKeyMgmtProperty, base::Value(eapKeyMgmt));
  }

  void SetWifiSecurity(const std::string& securityClass) {
    SetWifiProperty(shill::kSecurityClassProperty, base::Value(securityClass));
  }

  void SetupObserver(FakeNetworkListObserver* observer) {
    network_health_provider_->ObserveNetworkList(observer->pending_remote());
    base::RunLoop().RunUntilIdle();
  }

  void SetupObserver(FakeNetworkStateObserver* observer,
                     const std::string& observer_guid) {
    network_health_provider_->ObserveNetwork(observer->pending_remote(),
                                             observer_guid);
    base::RunLoop().RunUntilIdle();
  }

  void ClearDevicesAndServices() {
    // Clear test devices and services.
    task_environment_.RunUntilIdle();
    network_handler_test_helper_->ClearDevices();
    network_handler_test_helper_->ClearServices();
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<network_config::CrosNetworkConfig> cros_network_config_;
  std::unique_ptr<NetworkHealthProvider> network_health_provider_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(NetworkHealthProviderTest, ZeroNetworksAvailable) {
  FakeNetworkListObserver observer;
  SetupObserver(&observer);
  size_t prior_call_count = 0;
  ExpectListObserverFired(observer, &prior_call_count);
  EXPECT_TRUE(observer.observer_guids().empty());
  EXPECT_TRUE(observer.active_guid().empty());
}

TEST_F(NetworkHealthProviderTest, ObserveNonExistantNetwork) {
  // Observe a guid that doesn't exist and nothing happens.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, "unknown_guid");
  EXPECT_FALSE(observer.HasFired());
}

TEST_F(NetworkHealthProviderTest, UnsupportedNetworkTypeIgnored) {
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);

  // Create a VPN device, and it should not be visible to the observer.
  CreateVpnDevice();
  EXPECT_TRUE(list_observer.observer_guids().empty());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Associate and connect the VPN but it should still not be visible.
  AssociateAndConnectVpn();
  EXPECT_TRUE(list_observer.observer_guids().empty());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Create an ethernet device and verify that the observer list added the
  // network.
  CreateEthernetDevice();
  ExpectListObserverFired(list_observer, &list_call_count);
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string guid = list_observer.observer_guids()[0];
  ASSERT_FALSE(guid.empty());

  // Observe the guid and verify that it's the ethernet.
  FakeNetworkStateObserver eth_observer;
  SetupObserver(&eth_observer, guid);
  EXPECT_EQ(eth_observer.GetLatestState()->type, mojom::NetworkType::kEthernet);
}

// Test the setup and all intermediate states for ethernet network.
TEST_F(NetworkHealthProviderTest, SetupEthernetNetwork) {
  // Observe the network list.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);

  // No networks are present and no active network.
  ASSERT_EQ(0u, list_observer.observer_guids().size());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Create an ethernet device and verify `list_observer` fired.
  CreateEthernetDevice();
  ExpectListObserverFired(list_observer, &list_call_count);

  // Verify a new network is created, but there is no active guid because
  // the network isn't connected.
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string observer_guid = list_observer.observer_guids()[0];
  EXPECT_FALSE(observer_guid.empty());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Observe the network and verify the observer fired.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, observer_guid);
  size_t state_call_count = 0;
  ExpectStateObserverFired(observer, &state_call_count);

  // Get latest state and verify ethernet in not connected state.
  EXPECT_EQ(observer.GetLatestState()->observer_guid, observer_guid);
  EXPECT_EQ(observer.GetLatestState()->type, mojom::NetworkType::kEthernet);
  EXPECT_EQ(observer.GetLatestState()->state,
            mojom::NetworkState::kNotConnected);
  EXPECT_EQ(observer.GetLatestState()->type_properties.get(), nullptr);

  // Put the ethernet device into the connecting/associating state and verify
  // the new state and there is still no active guid.
  AssociateEthernet();
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kConnecting);
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Put ethernet into connected (but not online) state. It's guid should now
  // be the active one.
  SetEthernetConnected();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kConnected);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(observer_guid, list_observer.active_guid());
  // Verify ethernet authentication is none for non-EAP Ethernet.
  EXPECT_EQ(observer.GetLatestState()
                ->type_properties->get_ethernet()
                ->authentication,
            mojom::AuthenticationType::kNone);

  // TODO(ashleydp): Add test for authentication k8021x when fake shill service
  // can support and required configuration is known.

  // Put ethernet into online state. It's guid should remain active.
  SetEthernetOnline();
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(observer_guid, list_observer.active_guid());

  // Simulate unplug and network goes back to kNotConnected, and the active
  // guid should be cleared.
  SetEthernetDisconnected();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state,
            mojom::NetworkState::kNotConnected);
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Simulate plug in and back to online state. The active guid should be set.
  SetEthernetOnline();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(observer_guid, list_observer.active_guid());

  // AFter disabling ethernet, verify network state is kNotConnected and device
  // state is kDisabled.
  SetEthernetDisabled();
  EXPECT_TRUE(list_observer.active_guid().empty());
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kDisabled);

  // Enable ethernet and verify that it's guid becomes active again.
  SetEthernetEnabled();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(observer_guid, list_observer.active_guid());
}

// Test the setup and all intermediate states for ethernet network.
TEST_F(NetworkHealthProviderTest, SetupWifiNetwork) {
  // Observe the network list.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);

  // No networks are present and no active network.
  ASSERT_EQ(0u, list_observer.observer_guids().size());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Create a wifi device and verify `list_observer` fired.
  CreateWifiDevice();
  ExpectListObserverFired(list_observer, &list_call_count);

  // Verify a new network is created, but there is no active guid because
  // the network isn't connected.
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string observer_guid = list_observer.observer_guids()[0];
  EXPECT_FALSE(observer_guid.empty());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Observe the network and verify the observer fired.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, observer_guid);
  size_t state_call_count = 0;
  ExpectStateObserverFired(observer, &state_call_count);

  // Get latest state and verify wifi in not connected state.
  EXPECT_EQ(observer.GetLatestState()->observer_guid, observer_guid);
  EXPECT_EQ(observer.GetLatestState()->type, mojom::NetworkType::kWiFi);
  EXPECT_EQ(observer.GetLatestState()->state,
            mojom::NetworkState::kNotConnected);
  EXPECT_EQ(observer.GetLatestState()->type_properties.get(), nullptr);

  // Put the wifi device into the connecting/associating state and verify
  // the new state and there is still no active guid.
  AssociateWifi();
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kConnecting);
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Put wifi into connected (but not online) state. It's guid should now
  // be the active one.
  SetWifiConnected();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kConnected);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(observer_guid, list_observer.active_guid());
  // TODO(michaelcheco): Verify encryption properties once added to the API.

  // Put wifi into online state. It's guid should remain active.
  SetWifiOnline();
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(observer_guid, list_observer.active_guid());

  // Simulate disconnect and network goes back to kNotConnected, and the
  // active guid should be cleared.
  SetWifiDisconnected();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state,
            mojom::NetworkState::kNotConnected);
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Simulate reconnect and back to online state. The active guid should be
  // set.
  SetWifiOnline();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(observer_guid, list_observer.active_guid());

  // AFter disabling wifi, verify network state is kNotConnected and device
  // state is kDisabled.
  SetWifiDisabled();
  EXPECT_TRUE(list_observer.active_guid().empty());
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kDisabled);

  // Enable wifi and verify that it's guid becomes active again.
  SetWifiEnabled();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(observer_guid, list_observer.active_guid());
}

// Test the setup and all intermediate states for cellular network.
TEST_F(NetworkHealthProviderTest, SetupCellularNetwork) {
  // Observe the network list.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);

  // No networks are present and no active network.
  ASSERT_EQ(0u, list_observer.observer_guids().size());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Create a cellular device and verify `list_observer` fired.
  CreateCellularDevice();
  ExpectListObserverFired(list_observer, &list_call_count);

  // Verify a new network is created, but there is no active guid because
  // the network isn't connected.
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string observer_guid = list_observer.observer_guids()[0];
  EXPECT_FALSE(observer_guid.empty());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Observe the network and verify the observer fired.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, observer_guid);
  size_t state_call_count = 0;
  ExpectStateObserverFired(observer, &state_call_count);

  // Get latest state and verify wifi in not connected state.
  EXPECT_EQ(observer.GetLatestState()->observer_guid, observer_guid);
  EXPECT_EQ(observer.GetLatestState()->type, mojom::NetworkType::kCellular);
  EXPECT_EQ(observer.GetLatestState()->state,
            mojom::NetworkState::kNotConnected);

  // Put the cellular device into the connecting/associating state and verify
  // the new state and there is still no active guid.
  AssociateCellular();
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kConnecting);
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Put cellular into connected (but not online) state. It's guid should now
  // be the active one.
  SetCellularConnected();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kConnected);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(observer_guid, list_observer.active_guid());

  // Put cellular into online state. It's guid should remain active.
  SetCellularOnline();
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(observer_guid, list_observer.active_guid());

  // Simulate disconnect and network goes back to kNotConnected, and the
  // active guid should be cleared.
  SetCellularDisconnected();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state,
            mojom::NetworkState::kNotConnected);
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Simulate reconnect and back to online state. The active guid should be
  // set.
  SetCellularOnline();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(observer_guid, list_observer.active_guid());
}

// Test modifying wifi properties
TEST_F(NetworkHealthProviderTest, ChangingWifiProperties) {
  // Create a wifi device.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  CreateWifiDevice();
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string guid = list_observer.observer_guids()[0];

  // Put wifi online and validate it is active.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, guid);
  AssociateWifi();
  SetWifiOnline();
  size_t state_call_count = 0;
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());

  // Set signal strength.
  const int signal_strength_1 = 40;
  SetWifiSignalStrength(signal_strength_1);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_wifi()->signal_strength,
      signal_strength_1);

  // Change the signal strength.
  const int signal_strength_2 = 55;
  SetWifiSignalStrength(signal_strength_2);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_wifi()->signal_strength,
      signal_strength_2);

  // Set BSSID.
  const std::string bssid_1("01:23:cd:67:89:ab");
  const std::string bssid_formatted_1("01:23:CD:67:89:AB");
  SetWifiBssid(bssid_1);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->bssid,
            bssid_formatted_1);

  // Change BSSID.
  const std::string bssid_2("01:aa:45:67:89:ab");
  const std::string bssid_formatted_2("01:AA:45:67:89:AB");
  SetWifiBssid(bssid_2);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->bssid,
            bssid_formatted_2);

  // Set frequency.
  const int frequency_1 = 2400;
  SetWifiFrequency(frequency_1);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->frequency,
            frequency_1);

  // Change frequency.
  const int frequency_2 = 2450;
  SetWifiFrequency(frequency_2);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->frequency,
            frequency_2);

  // By default security should be NONE.
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->security,
            mojom::SecurityType::kNone);

  // Enable security as WEP_8021x.
  mojom::SecurityType security_2 = mojom::SecurityType::kWep8021x;
  SetWifiSecurity(shill::kSecurityWep, shill::kKeyManagementIEEE8021X);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->security,
            security_2);

  // Verify all properties are still set.
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_wifi()->signal_strength,
      signal_strength_2);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->bssid,
            bssid_formatted_2);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->frequency,
            frequency_2);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->security,
            security_2);
}

// Test modifying cellular properties
TEST_F(NetworkHealthProviderTest, ChangingCellularProperties) {
  // Create a cellular device.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  CreateCellularDevice();
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string guid = list_observer.observer_guids()[0];

  // Put cellular online and validate it is active.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, guid);
  AssociateCellular();
  SetCellularOnline();
  size_t state_call_count = 0;
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());

  // Set iccid.
  const std::string iccid_1 = "1234567890";
  SetCellularIccid(iccid_1);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_cellular()->iccid,
            iccid_1);

  // Change the iccid.
  const std::string iccid_2 = "0987654321";
  SetCellularIccid(iccid_2);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_cellular()->iccid,
            iccid_2);

  // Set network_technology.
  const std::string network_technology_1 = shill::kNetworkTechnologyEdge;
  SetCellularNetworkTechnology(network_technology_1);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()
                ->type_properties->get_cellular()
                ->network_technology,
            network_technology_1);

  // Change the network_technology.
  const std::string network_technology_2 = shill::kNetworkTechnologyLte;
  SetCellularNetworkTechnology(network_technology_2);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()
                ->type_properties->get_cellular()
                ->network_technology,
            network_technology_2);

  // Set eid.
  const std::string eid_1 = "03928509238502395213124";
  SetCellularEid(eid_1);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_cellular()->eid,
            eid_1);

  // Change the eid.
  const std::string eid_2 = "89012375908213750982315";
  SetCellularEid(eid_2);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_cellular()->eid,
            eid_2);

  // Set signal strength.
  const int signal_strength_1 = 40;
  SetCellularSignalStrength(signal_strength_1);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()
                ->type_properties->get_cellular()
                ->signal_strength,
            signal_strength_1);

  // Change the signal strength.
  const int signal_strength_2 = 55;
  SetCellularSignalStrength(signal_strength_2);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()
                ->type_properties->get_cellular()
                ->signal_strength,
            signal_strength_2);

  // Set roaming state.
  const std::string roaming_state_1 = shill::kRoamingStateHome;
  SetCellularRoamingState(roaming_state_1);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_cellular()->roaming_state,
      mojom::RoamingState::kHome);

  // Change roaming state.
  const std::string roaming_state_2 = shill::kRoamingStateRoaming;
  SetCellularRoamingState(roaming_state_2);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_cellular()->roaming_state,
      mojom::RoamingState::kRoaming);

  SetCellularSimLockStatus(shill::kSIMLockPin, /**sim_locked=*/true);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_cellular()->lock_type,
      mojom::LockType::kSimPin);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_cellular()->sim_locked,
      true);

  SetCellularSimLockStatus(/**lock_type=*/"", /**sim_locked=*/false);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_cellular()->lock_type,
      mojom::LockType::kNone);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_cellular()->sim_locked,
      false);

  // Verify all properties are still set.
  EXPECT_EQ(observer.GetLatestState()
                ->type_properties->get_cellular()
                ->signal_strength,
            signal_strength_2);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_cellular()->iccid,
            iccid_2);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_cellular()->eid,
            eid_2);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_cellular()->roaming_state,
      mojom::RoamingState::kRoaming);
  EXPECT_EQ(observer.GetLatestState()
                ->type_properties->get_cellular()
                ->network_technology,
            network_technology_2);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_cellular()->lock_type,
      mojom::LockType::kNone);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_cellular()->sim_locked,
      false);
}

// Start with an online ethernet connection and validate the interaction
// with a newly added wifi network.
TEST_F(NetworkHealthProviderTest, EthernetOnlineThenConnectWifi) {
  // Create an ethernet device.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  CreateEthernetDevice();
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string eth_guid = list_observer.observer_guids()[0];

  // Put ethernet online and validate it is active.
  FakeNetworkStateObserver eth_observer;
  SetupObserver(&eth_observer, eth_guid);
  AssociateEthernet();
  SetEthernetOnline();
  size_t state_call_count = 0;
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(eth_observer, &state_call_count);
  EXPECT_EQ(eth_observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(eth_guid, list_observer.active_guid());

  // Create Wifi device and verify it was added to the network list. The
  // ethernet network should remain active.
  CreateWifiDevice();
  ExpectListObserverFired(list_observer, &list_call_count);
  ASSERT_EQ(2u, list_observer.observer_guids().size());
  const std::string wifi_guid = (list_observer.observer_guids()[0] == eth_guid)
                                    ? list_observer.observer_guids()[1]
                                    : list_observer.observer_guids()[0];
  ASSERT_NE(eth_guid, wifi_guid);
  EXPECT_FALSE(list_observer.active_guid().empty());
  ASSERT_EQ(eth_guid, list_observer.active_guid());

  // Observe and associate the Wifi network. The wifi network should be in
  // the connecting state, and the active guid should still be ethernet.
  FakeNetworkStateObserver wifi_observer;
  SetupObserver(&wifi_observer, wifi_guid);
  AssociateWifi();
  state_call_count = 0;
  ExpectStateObserverFired(wifi_observer, &state_call_count);
  EXPECT_EQ(wifi_observer.GetLatestState()->state,
            mojom::NetworkState::kConnecting);
  EXPECT_EQ(eth_guid, list_observer.active_guid());

  // Put wifi network online. With both networks online, the ethernet should
  // still remain the active network.
  SetWifiOnline();
  ExpectStateObserverFired(wifi_observer, &state_call_count);
  EXPECT_EQ(wifi_observer.GetLatestState()->state,
            mojom::NetworkState::kOnline);
  EXPECT_EQ(eth_guid, list_observer.active_guid());

  // Disconnect ethernet and wifi should become the active network.
  SetEthernetDisconnected();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(eth_observer, &state_call_count);
  EXPECT_EQ(eth_observer.GetLatestState()->state,
            mojom::NetworkState::kNotConnected);
  EXPECT_EQ(wifi_observer.GetLatestState()->state,
            mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  ASSERT_EQ(wifi_guid, list_observer.active_guid());

  // Reconnect ethernet and it becomes the active network again.
  SetEthernetOnline();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(eth_observer, &state_call_count);
  EXPECT_EQ(eth_observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_EQ(wifi_observer.GetLatestState()->state,
            mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  ASSERT_EQ(eth_guid, list_observer.active_guid());
}

TEST_F(NetworkHealthProviderTest, SetupEthernetNetworkWithMacAddress) {
  // Create an ethernet device.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  CreateEthernetDevice();
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string eth_guid = list_observer.observer_guids()[0];

  // Put ethernet online and validate it is active.
  FakeNetworkStateObserver eth_observer;
  SetupObserver(&eth_observer, eth_guid);
  AssociateEthernet();
  SetEthernetOnline();
  size_t eth_state_call_count = 0;
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(eth_observer, &eth_state_call_count);
  EXPECT_EQ(eth_observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(eth_guid, list_observer.active_guid());

  SetEthernetMacAddress(kFormattedMacAddress);
  ExpectStateObserverFired(eth_observer, &eth_state_call_count);

  EXPECT_EQ(eth_observer.GetLatestState()->mac_address, kFormattedMacAddress);
}

TEST_F(NetworkHealthProviderTest, SetupWifiNetworkWithMacAddress) {
  // Create a wifi device.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  CreateWifiDevice();
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string guid = list_observer.observer_guids()[0];
  size_t state_call_count = 0;
  size_t list_call_count = 0;

  // Put wifi online and validate it is active.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, guid);
  AssociateWifi();
  SetWifiOnline();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());

  SetWifiMacAddress(kFormattedMacAddress);
  ExpectStateObserverFired(observer, &state_call_count);

  EXPECT_EQ(observer.GetLatestState()->mac_address, kFormattedMacAddress);
}

TEST_F(NetworkHealthProviderTest, IPConfig) {
  // Observe the network list.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);

  // No networks are present and no active network.
  ASSERT_EQ(0u, list_observer.observer_guids().size());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Create a wifi device and verify `list_observer` fired.
  CreateWifiDevice();
  AssociateIPConfigWithWifiDevice();
  ExpectListObserverFired(list_observer, &list_call_count);

  // Verify a new network is created, but there is no active guid because
  // the network isn't connected.
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string guid = list_observer.observer_guids()[0];
  EXPECT_FALSE(guid.empty());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Observe the network and verify the observer fired.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, guid);

  // Set IP Config properties.
  const std::string gateway("192.0.0.1");
  SetGatewayForIPConfig(gateway);
  const std::string ip_address("192.168.1.1");
  SetIPAddressForIPConfig(ip_address);
  const int routing_prefix = 1;
  SetRoutingPrefixForIPConfig(routing_prefix);
  base::ListValue dns_servers;
  const std::string dns_server_1 = "192.168.1.100";
  const std::string dns_server_2 = "192.168.1.101";
  dns_servers.Append(dns_server_1);
  dns_servers.Append(dns_server_2);
  SetNameServersForIPConfig(dns_servers);

  AssociateWifiWithIPConfig();
  SetWifiOnline();

  list_call_count = 0;
  size_t state_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());

  auto ip_config = observer.GetLatestState()->ip_config.Clone();
  EXPECT_EQ(ip_config->gateway.value(), gateway);
  EXPECT_EQ(ip_config->routing_prefix, routing_prefix);
  EXPECT_EQ(ip_config->ip_address.value(), ip_address);

  auto name_servers = ip_config->name_servers.value();
  EXPECT_EQ(name_servers.size(), 2U);
  EXPECT_EQ(name_servers[0], dns_server_1);
  EXPECT_EQ(name_servers[1], dns_server_2);
}

TEST_F(NetworkHealthProviderTest, SetupWifiNetworkWithSecurity) {
  // Create a wifi device.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  CreateWifiDevice();
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string guid = list_observer.observer_guids()[0];
  size_t state_call_count = 0;
  size_t list_call_count = 0;

  // Put wifi online and validate it is active.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, guid);
  AssociateWifi();
  SetWifiOnline();
  ExpectListObserverFired(list_observer,
                          /*prior_call_count=*/&list_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());

  // By default security should be NONE.
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->security,
            mojom::SecurityType::kNone);

  // Enable security as WEP_8021x.
  SetWifiSecurity(shill::kSecurityWep, shill::kKeyManagementIEEE8021X);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->security,
            mojom::SecurityType::kWep8021x);
  ExpectStateObserverFired(observer, &state_call_count);

  // Enable security as WEP_PSK.
  SetWifiSecurity(shill::kSecurityWep, std::string());
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->security,
            mojom::SecurityType::kWepPsk);
  ExpectStateObserverFired(observer, &state_call_count);

  // Enable security as WPA_EAP.
  SetWifiSecurity(shill::kSecurity8021x);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->security,
            mojom::SecurityType::kWpaEap);
  ExpectStateObserverFired(observer, &state_call_count);

  // Enable security as WPA_PSK.
  SetWifiSecurity(shill::kSecurityPsk);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->security,
            mojom::SecurityType::kWpaPsk);
  ExpectStateObserverFired(observer, &state_call_count);

  // Enable security as NONE.
  SetWifiSecurity(shill::kSecurityNone);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->security,
            mojom::SecurityType::kNone);
  ExpectStateObserverFired(observer, &state_call_count);
}

// Verifies that the list of observer guids and the active guid are set
// properly through various state transitions.
TEST_F(NetworkHealthProviderTest, EthernetAndWifiOrderedCorrectly) {
  // Create an ethernet device.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  CreateEthernetDevice();
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string eth_guid = list_observer.observer_guids()[0];

  // Put ethernet online and validate it is active.
  FakeNetworkStateObserver eth_observer;
  SetupObserver(&eth_observer, eth_guid);
  AssociateEthernet();
  SetEthernetOnline();
  EXPECT_EQ(eth_observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(eth_guid, list_observer.active_guid());

  CreateWifiDevice();
  const std::string wifi_guid = list_observer.observer_guids()[1];
  FakeNetworkStateObserver wifi_observer;
  SetupObserver(&wifi_observer, wifi_guid);
  AssociateWifi();
  // Ethernet should still be active and WiFi guid should be second in list of
  // observer guids.
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(eth_guid, list_observer.active_guid());
  EXPECT_EQ(wifi_guid, list_observer.observer_guids()[1]);

  // Ethernet should remain active, despite WiFi also being online.
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(eth_guid, list_observer.active_guid());
  EXPECT_EQ(wifi_guid, list_observer.observer_guids()[1]);
  SetWifiOnline();

  // Now that Ethernet is disconnected, WiFi should be active and Ethernet
  // should be the second guid in the list of observer guids.
  SetEthernetDisconnected();
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(wifi_guid, list_observer.active_guid());
  EXPECT_EQ(eth_guid, list_observer.observer_guids()[1]);

  // With both Ethernet and WiFi disconnected, neither of them should be
  // active and the Ethernet guid should be the first observer guid.
  SetWifiDisconnected();
  EXPECT_TRUE(list_observer.active_guid().empty());
  EXPECT_EQ(eth_guid, list_observer.observer_guids()[0]);
  EXPECT_EQ(wifi_guid, list_observer.observer_guids()[1]);

  // With WiFi in a portal state, it should be the active guid and
  // Ethernet should be the second guid in the list of observer guids.
  SetWifiPortal();
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(wifi_guid, list_observer.active_guid());
  EXPECT_EQ(eth_guid, list_observer.observer_guids()[1]);

  // With Ethernet online and WiFi in a portal state, Ethernet should
  // be the active guid and WiFi should be the second guid in the list
  // of observer guids.
  SetEthernetOnline();
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(eth_guid, list_observer.active_guid());
  EXPECT_EQ(wifi_guid, list_observer.observer_guids()[1]);

  // With Ethernet connected and WiFi in a portal state, WiFi should
  // be the active guid and Ethernet should be the second guid in the list
  // of observer guids.
  SetEthernetConnected();
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(wifi_guid, list_observer.active_guid());
  EXPECT_EQ(eth_guid, list_observer.observer_guids()[1]);
}

TEST_F(NetworkHealthProviderTest, NetworkingLog) {
  NetworkingLog log(temp_dir_.GetPath());
  network_health_provider_->SetNetworkingLogForTesting(&log);

  // Observe the network list.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);

  // Create a wifi device and verify `list_observer` fired.
  CreateWifiDevice();

  // Observe the network and verify the observer fired.
  FakeNetworkStateObserver observer;

  SetupObserver(&observer, list_observer.observer_guids()[0]);
  AssociateWifi();
  EXPECT_TRUE(list_observer.active_guid().empty());

  // The non-active network still appears in the log.
  EXPECT_FALSE(log.GetNetworkInfo().empty());

  // Put wifi into online state.
  SetWifiOnline();

  // Log is populated with network info now that WiFi is online.
  // Log contents tested in networking_log_unittest.cc -
  // NetworkingLogTest.DetailedLogContentsWiFi.
  EXPECT_FALSE(log.GetNetworkInfo().empty());
}

TEST_F(NetworkHealthProviderTest, ResetReceiverOnDisconnect) {
  // Ensure required features are enabled before binding to avoid DCHECK.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      std::vector<base::Feature>{features::kDiagnosticsAppNavigation,
                                 features::kEnableNetworkingInDiagnosticsApp,
                                 features::kDiagnosticsApp},
      std::vector<base::Feature>{});
  ASSERT_FALSE(network_health_provider_->ReceiverIsBound());
  mojo::Remote<mojom::NetworkHealthProvider> remote;
  network_health_provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(network_health_provider_->ReceiverIsBound());

  // Unbind remote to trigger disconnect and disconnect handler.
  remote.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(network_health_provider_->ReceiverIsBound());

  // Test intent is to ensure interface can be rebound when application is
  // reloaded using |CTRL + R|.  A disconnect should be signaled in which we
  // will reset the receiver to its unbound state.
  network_health_provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(network_health_provider_->ReceiverIsBound());
}

}  // namespace diagnostics
}  // namespace ash
