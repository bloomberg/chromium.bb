// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_uninstall_handler.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/hermes/hermes_clients.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/fake_network_connection_handler.h"
#include "chromeos/network/fake_stub_cellular_networks_provider.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/test_cellular_esim_profile_handler.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kDefaultCellularDevicePath[] = "test_cellular_device";
const char kDefaultEuiccPath[] = "/org/chromium/Hermes/Euicc/0";
const char kDefaultEid[] = "12345678901234567890123456789012";

const char kTestCarrierProfilePath[] = "/org/chromium/Hermes/Profile/123";
const char kTestNetworkServicePath[] = "/service/cellular123";
const char kTestCellularIccid[] = "100000000000000001";
const char kTestProfileName[] = "TestCellularNetwork";
const char kTestServiceProvider[] = "Test Wireless";

const char kTestCarrierProfilePath2[] = "/org/chromium/Hermes/Profile/124";
const char kTestNetworkServicePath2[] = "/service/cellular124";
const char kTestCellularIccid2[] = "100000000000000002";

}  // namespace

class CellularESimUninstallHandlerTest : public testing::Test {
 protected:
  CellularESimUninstallHandlerTest() = default;
  ~CellularESimUninstallHandlerTest() override = default;

  // testing::Test
  void SetUp() override {
    shill_clients::InitializeFakes();
    hermes_clients::InitializeFakes();

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        network_state_handler_.get());
    network_configuration_handler_ =
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get(), network_device_handler_.get());
    network_connection_handler_ =
        std::make_unique<FakeNetworkConnectionHandler>();
    cellular_inhibitor_ = std::make_unique<CellularInhibitor>();
    cellular_inhibitor_->Init(network_state_handler_.get(),
                              network_device_handler_.get());
    cellular_esim_profile_handler_ =
        std::make_unique<TestCellularESimProfileHandler>();
    cellular_esim_profile_handler_->Init(network_state_handler_.get(),
                                         cellular_inhibitor_.get());

    cellular_esim_uninstall_handler_ =
        std::make_unique<CellularESimUninstallHandler>();
    cellular_esim_uninstall_handler_->Init(
        cellular_inhibitor_.get(), cellular_esim_profile_handler_.get(),
        network_configuration_handler_.get(), network_connection_handler_.get(),
        network_state_handler_.get());

    stub_cellular_networks_provider_ =
        std::make_unique<FakeStubCellularNetworksProvider>();
    network_state_handler_->set_stub_cellular_networks_provider(
        stub_cellular_networks_provider_.get());
  }

  void TearDown() override {
    stub_cellular_networks_provider_.reset();
    cellular_esim_uninstall_handler_.reset();
    cellular_esim_profile_handler_.reset();
    cellular_inhibitor_.reset();
    network_device_handler_.reset();
    network_state_handler_.reset();
    network_configuration_handler_.reset();
    network_connection_handler_.reset();
    hermes_clients::Shutdown();
    shill_clients::Shutdown();
  }

  void Init(bool is_first_profile_active = true) {
    auto first_profile_state = is_first_profile_active
                                   ? hermes::profile::State::kActive
                                   : hermes::profile::State::kInactive;

    ShillDeviceClient::Get()->GetTestInterface()->AddDevice(
        kDefaultCellularDevicePath, shill::kTypeCellular, "cellular1");
    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kDefaultEuiccPath), kDefaultEid, /*is_active=*/true,
        /*physical_slot=*/0);
    HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
        dbus::ObjectPath(kTestCarrierProfilePath),
        dbus::ObjectPath(kDefaultEuiccPath), kTestCellularIccid,
        kTestProfileName, kTestServiceProvider, "", kTestNetworkServicePath,
        first_profile_state, hermes::profile::ProfileClass::kOperational,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);
    HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
        dbus::ObjectPath(kTestCarrierProfilePath2),
        dbus::ObjectPath(kDefaultEuiccPath), kTestCellularIccid2,
        kTestProfileName, kTestServiceProvider, "", kTestNetworkServicePath2,
        hermes::profile::State::kInactive,
        hermes::profile::ProfileClass::kOperational,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);
    base::RunLoop().RunUntilIdle();

    ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
        kTestNetworkServicePath, shill::kStateProperty,
        base::Value(shill::kStateOnline));
    base::RunLoop().RunUntilIdle();
  }

  void UninstallESim(base::RunLoop& run_loop,
                     const std::string& iccid,
                     const std::string& carrier_profile_path,
                     bool& status) {
    cellular_esim_uninstall_handler_->UninstallESim(
        iccid, dbus::ObjectPath(carrier_profile_path),
        dbus::ObjectPath(kDefaultEuiccPath),
        base::BindLambdaForTesting([&](bool status_result) {
          status = status_result;
          std::move(run_loop.QuitClosure()).Run();
        }));
  }

  void HandleNetworkDisconnect(bool should_fail) {
    // Run until the uninstallation state hits the disconnect on the
    // FakeNetworkConnectionHandler.
    base::RunLoop().RunUntilIdle();
    EXPECT_LT(0u, network_connection_handler_->disconnect_calls().size());
    if (should_fail) {
      network_connection_handler_->disconnect_calls()
          .front()
          .InvokeErrorCallback("disconnect_error", nullptr);
    } else {
      network_connection_handler_->disconnect_calls()
          .front()
          .InvokeSuccessCallback();
    }
  }

  bool ESimServiceConfigExists(const std::string& service_path) {
    std::vector<std::string> profile_paths;
    ShillProfileClient::Get()
        ->GetTestInterface()
        ->GetProfilePathsContainingService(service_path, &profile_paths);
    return !profile_paths.empty();
  }

  void AddStub(const std::string& stub_iccid, const std::string& eid) {
    stub_cellular_networks_provider_->AddStub(stub_iccid, eid);
    network_state_handler_->SyncStubCellularNetworks();
  }

  void SetHasRefreshedProfiles(bool has_refreshed) {
    cellular_esim_profile_handler_->SetHasRefreshedProfilesForEuicc(
        kDefaultEid, has_refreshed);
  }

  void ExpectResult(CellularESimUninstallHandler::UninstallESimResult result,
                    int expected_count = 1) {
    histogram_tester_.ExpectBucketCount(
        "Network.Cellular.ESim.UninstallProfile.OperationResult", result,
        expected_count);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  base::HistogramTester histogram_tester_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<FakeNetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<CellularESimUninstallHandler>
      cellular_esim_uninstall_handler_;
  std::unique_ptr<FakeStubCellularNetworksProvider>
      stub_cellular_networks_provider_;
};

TEST_F(CellularESimUninstallHandlerTest, Success) {
  Init();
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath));

  base::RunLoop run_loop;
  bool status;
  UninstallESim(run_loop, kTestCellularIccid, kTestCarrierProfilePath, status);
  HandleNetworkDisconnect(/*should_fail=*/false);
  run_loop.Run();

  // Verify that the esim profile and shill service configuration are removed
  // properly.
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(
          dbus::ObjectPath(kDefaultEuiccPath));
  ASSERT_TRUE(euicc_properties);
  EXPECT_EQ(1u, euicc_properties->installed_carrier_profiles().value().size());
  EXPECT_FALSE(ESimServiceConfigExists(kTestNetworkServicePath));
  EXPECT_TRUE(status);

  ExpectResult(CellularESimUninstallHandler::UninstallESimResult::kSuccess);
}

TEST_F(CellularESimUninstallHandlerTest, Success_AlreadyDisabled) {
  Init(/*is_first_profile_active=*/false);
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath));

  base::RunLoop run_loop;
  bool status;
  UninstallESim(run_loop, kTestCellularIccid, kTestCarrierProfilePath, status);
  HandleNetworkDisconnect(/*should_fail=*/false);
  run_loop.Run();

  // Verify that the esim profile and shill service configuration are removed
  // properly.
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(
          dbus::ObjectPath(kDefaultEuiccPath));
  ASSERT_TRUE(euicc_properties);
  EXPECT_EQ(1u, euicc_properties->installed_carrier_profiles().value().size());
  EXPECT_FALSE(ESimServiceConfigExists(kTestNetworkServicePath));
  EXPECT_TRUE(status);

  ExpectResult(CellularESimUninstallHandler::UninstallESimResult::kSuccess);
}

TEST_F(CellularESimUninstallHandlerTest, DisconnectFailure) {
  Init();
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath));

  bool status;
  base::RunLoop run_loop;
  UninstallESim(run_loop, kTestCellularIccid, kTestCarrierProfilePath, status);
  HandleNetworkDisconnect(/*should_fail=*/true);
  run_loop.Run();
  EXPECT_FALSE(status);
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath));

  ExpectResult(
      CellularESimUninstallHandler::UninstallESimResult::kDisconnectFailed);
}

TEST_F(CellularESimUninstallHandlerTest, HermesFailure) {
  Init();
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath));

  HermesEuiccClient::Get()->GetTestInterface()->QueueHermesErrorStatus(
      HermesResponseStatus::kErrorUnknown);
  bool status;
  base::RunLoop run_loop;
  UninstallESim(run_loop, kTestCellularIccid, kTestCarrierProfilePath, status);
  HandleNetworkDisconnect(/*should_fail=*/false);
  run_loop.Run();
  EXPECT_FALSE(status);
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath));

  ExpectResult(CellularESimUninstallHandler::UninstallESimResult::
                   kRefreshProfilesFailed);
}

TEST_F(CellularESimUninstallHandlerTest, MultipleRequests) {
  Init();
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath));
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath2));

  // Make two uninstall requests back to back.
  bool status1, status2;
  base::RunLoop run_loop1, run_loop2;

  UninstallESim(run_loop1, kTestCellularIccid, kTestCarrierProfilePath,
                status1);
  UninstallESim(run_loop2, kTestCellularIccid2, kTestCarrierProfilePath2,
                status2);

  // Only the first profile is connected, so only one disconnect handler is
  // needed.
  HandleNetworkDisconnect(/*should_fail=*/false);

  run_loop1.Run();
  run_loop2.Run();

  // Verify that both requests succeeded.
  EXPECT_TRUE(status1);
  EXPECT_TRUE(status2);
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(
          dbus::ObjectPath(kDefaultEuiccPath));
  ASSERT_TRUE(euicc_properties);
  EXPECT_TRUE(euicc_properties->installed_carrier_profiles().value().empty());
  EXPECT_FALSE(ESimServiceConfigExists(kTestNetworkServicePath));
  EXPECT_FALSE(ESimServiceConfigExists(kTestNetworkServicePath2));

  ExpectResult(CellularESimUninstallHandler::UninstallESimResult::kSuccess,
               /*expected_count=*/2);
}

TEST_F(CellularESimUninstallHandlerTest, StubCellularNetwork) {
  Init();

  // Remove shill eSIM service and add a corresponding stub service.
  ShillServiceClient::Get()->GetTestInterface()->RemoveService(
      kTestNetworkServicePath);
  base::RunLoop().RunUntilIdle();
  AddStub(kTestCellularIccid, kDefaultEid);

  // Verify that removing the eSIM profile succeeds.
  base::RunLoop run_loop;
  bool success;
  UninstallESim(run_loop, kTestCellularIccid, kTestCarrierProfilePath, success);
  run_loop.Run();
  EXPECT_TRUE(success);

  ExpectResult(CellularESimUninstallHandler::UninstallESimResult::kSuccess);
}

}  // namespace chromeos
