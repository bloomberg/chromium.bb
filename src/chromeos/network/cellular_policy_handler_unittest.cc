// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_policy_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/dbus/hermes/hermes_clients.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/network/cellular_connection_handler.h"
#include "chromeos/network/cellular_esim_installer.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/fake_network_connection_handler.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "chromeos/network/test_cellular_esim_profile_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

const char kTestEuiccPath[] = "/org/chromium/Hermes/Euicc/0";
const char kTestEuiccPath2[] = "/org/chromium/Hermes/Euicc/1";
const char kTestEid[] = "12345678901234567890123456789012";
const char kTestEid2[] = "12345678901234567890123456789000";
const char kCellularGuid[] = "cellular_guid";
const char kICCID[] = "1000000000000000002";

void CheckShillConfiguration(bool is_installed) {
  std::string service_path =
      ShillServiceClient::Get()->GetTestInterface()->FindServiceMatchingGUID(
          kCellularGuid);
  const base::Value* properties =
      ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
          service_path);

  if (!is_installed) {
    EXPECT_EQ(properties, nullptr);
    return;
  }
  const std::string* guid = properties->FindStringKey(shill::kGuidProperty);
  EXPECT_EQ(kCellularGuid, *guid);
  // UIData should not be empty if cellular service is configured.
  const std::string* ui_data_value =
      properties->FindStringKey(shill::kUIDataProperty);
  EXPECT_NE(*ui_data_value, std::string());
  const std::string* iccid = properties->FindStringKey(shill::kIccidProperty);
  EXPECT_EQ(kICCID, *iccid);
}

std::string GenerateCellularPolicy(const std::string& smdp_address) {
  return base::StringPrintf(
      R"({"GUID": "%s", "Type": "Cellular",
                        "Name": "cellular1",
                        "Cellular": { "SMDPAddress": "%s"}})",
      kCellularGuid, smdp_address.c_str());
}

}  // namespace

class CellularPolicyHandlerTest : public testing::Test {
 protected:
  CellularPolicyHandlerTest() = default;
  ~CellularPolicyHandlerTest() override = default;

  // testing::Test
  void SetUp() override {
    shill_clients::InitializeFakes();
    hermes_clients::InitializeFakes();

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        network_state_handler_.get());
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
    network_connection_handler_ =
        std::make_unique<FakeNetworkConnectionHandler>();
    cellular_inhibitor_ = std::make_unique<CellularInhibitor>();
    cellular_inhibitor_->Init(network_state_handler_.get(),
                              network_device_handler_.get());
    cellular_esim_profile_handler_ =
        std::make_unique<TestCellularESimProfileHandler>();
    cellular_esim_profile_handler_->Init(network_state_handler_.get(),
                                         cellular_inhibitor_.get());
    cellular_connection_handler_ =
        std::make_unique<CellularConnectionHandler>();
    cellular_connection_handler_->Init(network_state_handler_.get(),
                                       cellular_inhibitor_.get(),
                                       cellular_esim_profile_handler_.get());
    cellular_esim_installer_ = std::make_unique<CellularESimInstaller>();
    cellular_esim_installer_->Init(
        cellular_connection_handler_.get(), cellular_inhibitor_.get(),
        network_connection_handler_.get(), network_profile_handler_.get(),
        network_state_handler_.get());
    network_configuration_handler_ =
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get(), network_device_handler_.get());
    managed_network_configuration_handler_ =
        ManagedNetworkConfigurationHandler::InitializeForTesting(
            network_state_handler_.get(), network_profile_handler_.get(),
            network_device_handler_.get(), network_configuration_handler_.get(),
            /*UIProxyConfigService=*/nullptr);
    cellular_policy_handler_ = std::make_unique<CellularPolicyHandler>();
    cellular_policy_handler_->Init(
        cellular_esim_installer_.get(), network_profile_handler_.get(),
        managed_network_configuration_handler_.get());
    SetupEuicc();
  }

  void SetupEuicc() {
    HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEid, /*is_active=*/true,
        /*physical_slot=*/0);
    base::RunLoop().RunUntilIdle();
  }

  // testing::Test
  void TearDown() override {
    cellular_policy_handler_.reset();
    cellular_esim_installer_.reset();
    cellular_esim_profile_handler_.reset();
    cellular_connection_handler_.reset();
    cellular_inhibitor_.reset();
    network_configuration_handler_.reset();
    managed_network_configuration_handler_.reset();
    network_profile_handler_.reset();
    network_device_handler_.reset();
    network_state_handler_.reset();
    network_connection_handler_.reset();

    hermes_clients::Shutdown();
    shill_clients::Shutdown();
  }

  void InstallESimPolicy(const std::string& onc_json,
                         const std::string& activation_code,
                         bool expect_install_success) {
    base::Value policy = onc::ReadDictionaryFromJson(onc_json);
    cellular_policy_handler_->InstallESim(
        activation_code, base::Value::AsDictionaryValue(policy));
    FastForwardProfileRefreshDelay();
    base::RunLoop().RunUntilIdle();

    if (expect_install_success) {
      EXPECT_LE(1u, network_connection_handler_->connect_calls().size());
      network_connection_handler_->connect_calls()
          .back()
          .InvokeSuccessCallback();
    } else {
      EXPECT_LE(0u, network_connection_handler_->connect_calls().size());
    }
    base::RunLoop().RunUntilIdle();
  }

  void FastForwardProfileRefreshDelay() {
    const base::TimeDelta kProfileRefreshCallbackDelay =
        base::Milliseconds(150);
    // Connect can result in two profile refresh calls before and after
    // enabling profile. Fast forward by delay after refresh.
    task_environment_.FastForwardBy(2 * kProfileRefreshCallbackDelay);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<CellularConnectionHandler> cellular_connection_handler_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
  std::unique_ptr<FakeNetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<CellularESimInstaller> cellular_esim_installer_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<CellularPolicyHandler> cellular_policy_handler_;
};

TEST_F(CellularPolicyHandlerTest, InstallProfileSuccess) {
  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode());
  // Verify esim profile get installed successfully when installing policy esim
  // with a fake SMDP address.
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/true);
  CheckShillConfiguration(/*is_installed=*/true);
}

TEST_F(CellularPolicyHandlerTest, InstallProfileFailure) {
  // Verify esim profile doesn't get installed when installing policy esim
  // with a invalid SMDP address.
  const std::string policy = GenerateCellularPolicy("000");
  InstallESimPolicy(policy,
                    /*activation_code=*/std::string(),
                    /*expect_install_success=*/false);
  CheckShillConfiguration(/*is_installed=*/false);
}

TEST_F(CellularPolicyHandlerTest, InstallOnExternalEUICC) {
  // Verify esim profile get installed successfully when installing policy
  // on the external EUICC.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kCellularUseExternalEuicc);
  HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
      dbus::ObjectPath(kTestEuiccPath2), kTestEid2, /*is_active=*/true,
      /*physical_slot=*/1);
  base::RunLoop().RunUntilIdle();
  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode());
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/true);
  CheckShillConfiguration(/*is_installed=*/true);
}

TEST_F(CellularPolicyHandlerTest, InstallNoEUICCAvailable) {
  // Verify esim profile doesn't get installed when installing policy esim
  // with no available EUICC.
  HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
  base::RunLoop().RunUntilIdle();
  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode());
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/false);
  CheckShillConfiguration(/*is_installed=*/false);
}

TEST_F(CellularPolicyHandlerTest, UpdateSMDPAddress) {
  // Verify that the first request should be invalidated when the second
  // request is queued.
  std::string policy = GenerateCellularPolicy("000");
  InstallESimPolicy(policy,
                    /*activation_code=*/"000",
                    /*expect_install_success=*/false);
  policy = GenerateCellularPolicy(HermesEuiccClient::Get()
                                      ->GetTestInterface()
                                      ->GenerateFakeActivationCode());

  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/true);
  CheckShillConfiguration(/*is_installed=*/true);
}

}  // namespace chromeos
