// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_installer.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/hermes/hermes_clients.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "chromeos/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/network/cellular_connection_handler.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/fake_network_connection_handler.h"
#include "chromeos/network/fake_stub_cellular_networks_provider.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/test_cellular_esim_profile_handler.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

using InstallResultTuple = std::tuple<HermesResponseStatus,
                                      absl::optional<dbus::ObjectPath>,
                                      absl::optional<std::string>>;

const char kTestEuiccPath[] = "/org/chromium/Hermes/Euicc/0";
const char kTestEid[] = "12345678901234567890123456789012";
const char kInstallViaQrCodeHistogram[] =
    "Network.Cellular.ESim.InstallViaQrCode.Result";
const char kInstallViaQrCodeOperationHistogram[] =
    "Network.Cellular.ESim.InstallViaQrCode.OperationResult";
const char kInstallViaPolicyOperationHistogram[] =
    "Network.Cellular.ESim.Policy.ESimInstall.OperationResult";
const char kInstallESimResultHistogram[] =
    "Network.Cellular.ESim.InstallationResult";
const char kESimProfileDownloadLatencyHistogram[] =
    "Network.Cellular.ESim.ProfileDownload.ActivationCode.Latency";

base::DictionaryValue GetPolicyShillProperties() {
  base::DictionaryValue new_shill_properties;
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
  new_shill_properties.SetStringKey(shill::kUIDataProperty,
                                    ui_data->GetAsJson());
  return new_shill_properties;
}

}  // namespace

class CellularESimInstallerTest : public testing::Test {
 protected:
  CellularESimInstallerTest() = default;
  ~CellularESimInstallerTest() override = default;

  // testing::Test
  void SetUp() override {
    shill_clients::InitializeFakes();
    hermes_clients::InitializeFakes();

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        network_state_handler_.get());
    network_connection_handler_ =
        std::make_unique<FakeNetworkConnectionHandler>();
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
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
    stub_cellular_networks_provider_ =
        std::make_unique<FakeStubCellularNetworksProvider>();
    network_state_handler_->set_stub_cellular_networks_provider(
        stub_cellular_networks_provider_.get());
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
    stub_cellular_networks_provider_.reset();
    cellular_esim_installer_.reset();
    cellular_esim_profile_handler_.reset();
    cellular_connection_handler_.reset();
    cellular_inhibitor_.reset();
    network_profile_handler_.reset();
    network_device_handler_.reset();
    network_state_handler_.reset();
    network_connection_handler_.reset();

    hermes_clients::Shutdown();
    shill_clients::Shutdown();
  }

  InstallResultTuple InstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      const dbus::ObjectPath euicc_path,
      base::Value new_shill_properties,
      bool wait_for_connect,
      bool fail_connect) {
    HermesResponseStatus out_install_result;
    absl::optional<dbus::ObjectPath> out_esim_profile_path;
    absl::optional<std::string> out_service_path;

    base::RunLoop run_loop;
    cellular_esim_installer_->InstallProfileFromActivationCode(
        activation_code, confirmation_code, euicc_path,
        std::move(new_shill_properties),
        base::BindLambdaForTesting(
            [&](HermesResponseStatus install_result,
                absl::optional<dbus::ObjectPath> esim_profile_path,
                absl::optional<std::string> service_path) {
              out_install_result = install_result;
              out_esim_profile_path = esim_profile_path;
              out_service_path = service_path;
              run_loop.Quit();
            }));

    FastForwardProfileRefreshDelay();

    if (wait_for_connect) {
      base::RunLoop().RunUntilIdle();
      EXPECT_LE(1u, network_connection_handler_->connect_calls().size());
      if (fail_connect) {
        network_connection_handler_->connect_calls().back().InvokeErrorCallback(
            "fake_error_name", /*error_data=*/nullptr);
      } else {
        network_connection_handler_->connect_calls()
            .back()
            .InvokeSuccessCallback();
      }
    }

    run_loop.Run();
    return std::make_tuple(out_install_result, out_esim_profile_path,
                           out_service_path);
  }

  void CheckInstallSuccess(const InstallResultTuple& actual_result_tuple) {
    EXPECT_EQ(HermesResponseStatus::kSuccess, std::get<0>(actual_result_tuple));
    EXPECT_NE(std::get<1>(actual_result_tuple), absl::nullopt);
    EXPECT_NE(std::get<2>(actual_result_tuple), absl::nullopt);
    const base::Value* properties =
        ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
            *std::get<2>(actual_result_tuple));
    ASSERT_TRUE(properties);
    const std::string* type = properties->FindStringKey(shill::kTypeProperty);
    EXPECT_EQ(shill::kTypeCellular, *type);
    const std::string* iccid = properties->FindStringKey(shill::kIccidProperty);
    EXPECT_NE(std::string(), *iccid);
  }

  void FastForwardProfileRefreshDelay() {
    const base::TimeDelta kProfileRefreshCallbackDelay =
        base::Milliseconds(150);

    // Connect can result in two profile refresh calls before and after
    // enabling profile. Fast forward by delay after refresh.
    task_environment_.FastForwardBy(2 * kProfileRefreshCallbackDelay);
  }

  base::HistogramTester* HistogramTesterPtr() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<CellularConnectionHandler> cellular_connection_handler_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
  std::unique_ptr<FakeNetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<CellularESimInstaller> cellular_esim_installer_;
  std::unique_ptr<FakeStubCellularNetworksProvider>
      stub_cellular_networks_provider_;
};

TEST_F(CellularESimInstallerTest, InstallProfileInvalidActivationCode) {
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      /*activation_code=*/std::string(), /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::DictionaryValue(),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(HermesResponseStatus::kErrorInvalidActivationCode,
            std::get<0>(result_tuple));
  EXPECT_EQ(std::get<1>(result_tuple), absl::nullopt);
  EXPECT_EQ(std::get<2>(result_tuple), absl::nullopt);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaQrCodeHistogram,
      HermesResponseStatus::kErrorInvalidActivationCode,
      /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallESimResultHistogram,
      CellularESimInstaller::InstallESimProfileResult::kHermesInstallFailed,
      /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaQrCodeOperationHistogram,
      CellularESimInstaller::InstallESimProfileResult::kHermesInstallFailed,
      /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaPolicyOperationHistogram,
      CellularESimInstaller::InstallESimProfileResult::kHermesInstallFailed,
      /*expected_count=*/0);

  // Verify that install from policy are handled properly
  base::DictionaryValue new_shill_properties;
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
  new_shill_properties.SetStringKey(shill::kUIDataProperty,
                                    ui_data->GetAsJson());
  result_tuple = InstallProfileFromActivationCode(
      /*activation_code=*/std::string(), /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      std::move(GetPolicyShillProperties()),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(HermesResponseStatus::kErrorInvalidActivationCode,
            std::get<0>(result_tuple));
  EXPECT_EQ(std::get<1>(result_tuple), absl::nullopt);
  EXPECT_EQ(std::get<2>(result_tuple), absl::nullopt);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaQrCodeHistogram,
      HermesResponseStatus::kErrorInvalidActivationCode,
      /*expected_count=*/2);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallESimResultHistogram,
      CellularESimInstaller::InstallESimProfileResult::kHermesInstallFailed,
      /*expected_count=*/2);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaQrCodeOperationHistogram,
      CellularESimInstaller::InstallESimProfileResult::kHermesInstallFailed,
      /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaPolicyOperationHistogram,
      CellularESimInstaller::InstallESimProfileResult::kHermesInstallFailed,
      /*expected_count=*/1);
}

TEST_F(CellularESimInstallerTest, InstallProfileConnectFailure) {
  // Verify that connect failures are handled properly.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::DictionaryValue(),
      /*wait_for_connect=*/true, /*fail_connect=*/true);
  CheckInstallSuccess(result_tuple);
  HistogramTesterPtr()->ExpectBucketCount(kInstallViaQrCodeHistogram,
                                          HermesResponseStatus::kSuccess,
                                          /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallESimResultHistogram,
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaQrCodeOperationHistogram,
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaPolicyOperationHistogram,
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*expected_count=*/0);

  // Verify that install from policy are handled property.
  result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      std::move(GetPolicyShillProperties()),
      /*wait_for_connect=*/true, /*fail_connect=*/true);
  CheckInstallSuccess(result_tuple);
  HistogramTesterPtr()->ExpectBucketCount(kInstallViaQrCodeHistogram,
                                          HermesResponseStatus::kSuccess,
                                          /*expected_count=*/2);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallESimResultHistogram,
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*expected_count=*/2);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaQrCodeOperationHistogram,
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaPolicyOperationHistogram,
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*expected_count=*/1);
}

TEST_F(CellularESimInstallerTest, InstallProfileSuccess) {
  // Verify that install succeeds when valid activation code is passed.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::DictionaryValue(),
      /*wait_for_connect=*/true, /*fail_connect=*/false);
  CheckInstallSuccess(result_tuple);

  HistogramTesterPtr()->ExpectTotalCount(kESimProfileDownloadLatencyHistogram,
                                         1);
  HistogramTesterPtr()->ExpectBucketCount(kInstallViaQrCodeHistogram,
                                          HermesResponseStatus::kSuccess,
                                          /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallESimResultHistogram,
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaQrCodeOperationHistogram,
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaPolicyOperationHistogram,
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*expected_count=*/0);

  // Verify install from policy works properly
  result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      std::move(GetPolicyShillProperties()),
      /*wait_for_connect=*/true, /*fail_connect=*/false);
  CheckInstallSuccess(result_tuple);
  HistogramTesterPtr()->ExpectTotalCount(kESimProfileDownloadLatencyHistogram,
                                         2);
  HistogramTesterPtr()->ExpectBucketCount(kInstallViaQrCodeHistogram,
                                          HermesResponseStatus::kSuccess,
                                          /*expected_count=*/2);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallESimResultHistogram,
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*expected_count=*/2);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaQrCodeOperationHistogram,
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaPolicyOperationHistogram,
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*expected_count=*/1);
}

TEST_F(CellularESimInstallerTest, InstallProfileAlreadyConnected) {
  HermesProfileClient::Get()->GetTestInterface()->SetEnableProfileBehavior(
      HermesProfileClient::TestInterface::EnableProfileBehavior::
          kConnectableAndConnected);

  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::DictionaryValue(),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  CheckInstallSuccess(result_tuple);
}

TEST_F(CellularESimInstallerTest, InstallProfileCreateShillConfigFailure) {
  ShillManagerClient::Get()->GetTestInterface()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kFailure);
  // Verify that install succeeds when valid activation code is passed.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::DictionaryValue(),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  CheckInstallSuccess(result_tuple);
}

}  // namespace chromeos