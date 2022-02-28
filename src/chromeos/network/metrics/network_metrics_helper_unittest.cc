// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/metrics/network_metrics_helper.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Note: All histogram names should be listed here.

// LogAllConnectionResult() Cellular histograms.
const char kCellularConnectResultAllHistogram[] =
    "Network.Cellular.ConnectionResult.All";
const char kCellularESimConnectResultAllHistogram[] =
    "Network.Cellular.ESim.ConnectionResult.All";
const char kCellularPSimConnectResultAllHistogram[] =
    "Network.Cellular.PSim.ConnectionResult.All";

// LogAllConnectionResult() VPN histograms.
const char kVpnConnectResultAllHistogram[] = "Network.VPN.ConnectionResult.All";
const char kVpnBuiltInConnectResultAllHistogram[] =
    "Network.VPN.TypeBuiltIn.ConnectionResult.All";
const char kVpnThirdPartyConnectResultAllHistogram[] =
    "Network.VPN.TypeThirdParty.ConnectionResult.All";

// LogAllConnectionResult() WiFi histograms.
const char kWifiConnectResultAllHistogram[] =
    "Network.WiFi.ConnectionResult.All";
const char kWifiOpenConnectResultAllHistogram[] =
    "Network.WiFi.SecurityOpen.ConnectionResult.All";
const char kWifiPasswordProtectedConnectResultAllHistogram[] =
    "Network.WiFi.SecurityPasswordProtected.ConnectionResult.All";

const char kTestGuid[] = "test_guid";
const char kTestServicePath[] = "/service/network";
const char kTestName[] = "network_name";
const char kTestVpnHost[] = "test host";

}  // namespace

class NetworkMetricsHelperTest : public testing::Test {
 public:
  NetworkMetricsHelperTest() {}

  NetworkMetricsHelperTest(const NetworkMetricsHelperTest&) = delete;
  NetworkMetricsHelperTest& operator=(const NetworkMetricsHelperTest&) = delete;

  ~NetworkMetricsHelperTest() override = default;

  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    shill_service_client_ = ShillServiceClient::Get()->GetTestInterface();
    shill_service_client_->ClearServices();
    base::RunLoop().RunUntilIdle();

    network_handler_test_helper_->RegisterPrefs(profile_prefs_.registry(),
                                                local_state_.registry());

    network_handler_test_helper_->InitializePrefs(&profile_prefs_,
                                                  &local_state_);
  }

  void TearDown() override {
    shill_service_client_->ClearServices();
    network_handler_test_helper_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  ShillServiceClient::TestInterface* shill_service_client_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(NetworkMetricsHelperTest, LogAllConnectionResultCellularESim) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeCellular, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kEidProperty, base::Value("eid"));
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(kTestGuid,
                                               shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectResultAllHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectResultAllHistogram,
                                      0);
}

TEST_F(NetworkMetricsHelperTest, LogAllConnectionResultCellularPSim) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeCellular, shill::kStateIdle,
                                    /*visible=*/true);
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(kTestGuid,
                                               shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectResultAllHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectResultAllHistogram,
                                      0);
}

TEST_F(NetworkMetricsHelperTest, LogAllConnectionResultVPN) {
  const std::vector<const std::string> kProviders{{
      shill::kProviderL2tpIpsec,
      shill::kProviderArcVpn,
      shill::kProviderOpenVpn,
      shill::kProviderThirdPartyVpn,
      shill::kProviderWireGuard,
  }};

  size_t expected_all_count = 0;
  size_t expected_built_in_count = 0;
  size_t expected_third_party_count = 0;

  for (const auto& provider : kProviders) {
    shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                      shill::kTypeVPN, shill::kStateIdle,
                                      /*visible=*/true);
    shill_service_client_->SetServiceProperty(
        kTestServicePath, shill::kProviderTypeProperty, base::Value(provider));
    shill_service_client_->SetServiceProperty(kTestServicePath,
                                              shill::kProviderHostProperty,
                                              base::Value(kTestVpnHost));
    base::RunLoop().RunUntilIdle();

    if (provider == shill::kProviderThirdPartyVpn ||
        provider == shill::kProviderArcVpn) {
      ++expected_third_party_count;
    } else {
      ++expected_built_in_count;
    }
    ++expected_all_count;

    NetworkMetricsHelper::LogAllConnectionResult(kTestGuid,
                                                 shill::kErrorNotRegistered);
    histogram_tester_->ExpectTotalCount(kVpnConnectResultAllHistogram,
                                        expected_all_count);
    histogram_tester_->ExpectTotalCount(kVpnBuiltInConnectResultAllHistogram,
                                        expected_built_in_count);
    histogram_tester_->ExpectTotalCount(kVpnThirdPartyConnectResultAllHistogram,
                                        expected_third_party_count);

    shill_service_client_->RemoveService(kTestServicePath);
    base::RunLoop().RunUntilIdle();
  }
}

TEST_F(NetworkMetricsHelperTest, LogAllConnectionResultWifiOpen) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeWifi, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->SetServiceProperty(kTestServicePath,
                                            shill::kSecurityClassProperty,
                                            base::Value(shill::kSecurityNone));
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(kTestGuid,
                                               shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultAllHistogram, 0);
}

TEST_F(NetworkMetricsHelperTest, LogAllConnectionResultWifiPasswordProtected) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeWifi, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->SetServiceProperty(kTestServicePath,
                                            shill::kSecurityClassProperty,
                                            base::Value(shill::kSecurityPsk));
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(kTestGuid,
                                               shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectResultAllHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultAllHistogram, 1);
}

}  // namespace chromeos
