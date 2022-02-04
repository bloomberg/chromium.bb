// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/metrics/vpn_network_metrics_helper.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/shill_property_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kVpnHistogramConfigurationSourceArc[] =
    "Network.Ash.VPN.ARC.ConfigurationSource";
const char kVpnHistogramConfigurationSourceL2tpIpsec[] =
    "Network.Ash.VPN.L2TPIPsec.ConfigurationSource";
const char kVpnHistogramConfigurationSourceOpenVpn[] =
    "Network.Ash.VPN.OpenVPN.ConfigurationSource";
const char kVpnHistogramConfigurationSourceThirdParty[] =
    "Network.Ash.VPN.ThirdParty.ConfigurationSource";
const char kVpnHistogramConfigurationSourceWireGuard[] =
    "Network.Ash.VPN.WireGuard.ConfigurationSource";

std::string PrettyJson(const base::DictionaryValue& value) {
  std::string pretty;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &pretty);
  return pretty;
}

void ErrorCallback(const std::string& error_name,
                   std::unique_ptr<base::DictionaryValue> error_data) {
  ADD_FAILURE() << "Unexpected error: " << error_name
                << " with associated data: \n"
                << PrettyJson(*error_data);
}

}  // namespace

class VpnNetworkMetricsHelperTest : public testing::Test {
 public:
  VpnNetworkMetricsHelperTest() = default;
  VpnNetworkMetricsHelperTest(const VpnNetworkMetricsHelperTest&) = delete;
  VpnNetworkMetricsHelperTest& operator=(const VpnNetworkMetricsHelperTest&) =
      delete;
  ~VpnNetworkMetricsHelperTest() override = default;

  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    ClearServices();
  }

  void TearDown() override {
    ClearServices();
    network_handler_test_helper_.reset();
  }

 protected:
  void ClearServices() {
    ShillServiceClient::Get()->GetTestInterface()->ClearServices();
    base::RunLoop().RunUntilIdle();
  }

  // Helper function to create a VPN network using NetworkConfigurationHandler.
  void CreateTestShillConfiguration(const char* vpn_provider_type,
                                    bool is_managed) {
    base::Value properties(base::Value::Type::DICTIONARY);

    properties.SetKey(shill::kGuidProperty, base::Value("vpn_guid"));
    properties.SetKey(shill::kTypeProperty, base::Value(shill::kTypeVPN));
    properties.SetKey(shill::kStateProperty, base::Value(shill::kStateIdle));
    properties.SetKey(shill::kProviderHostProperty, base::Value("vpn_host"));
    properties.SetKey(shill::kProviderTypeProperty,
                      base::Value(vpn_provider_type));
    properties.SetKey(
        shill::kProfileProperty,
        base::Value(NetworkProfileHandler::GetSharedProfilePath()));

    if (is_managed) {
      properties.SetKey(shill::kONCSourceProperty,
                        base::Value(shill::kONCSourceDevicePolicy));
      std::unique_ptr<NetworkUIData> ui_data = NetworkUIData::CreateFromONC(
          ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
      properties.SetKey(shill::kUIDataProperty,
                        base::Value(ui_data->GetAsJson()));
    }

    NetworkHandler::Get()
        ->network_configuration_handler()
        ->CreateShillConfiguration(properties, base::DoNothing(),
                                   base::BindOnce(&ErrorCallback));
    base::RunLoop().RunUntilIdle();
  }

  void ExpectConfigurationSourceCounts(const char* histogram,
                                       size_t manual_count,
                                       size_t policy_count) {
    histogram_tester_->ExpectBucketCount<
        VpnNetworkMetricsHelper::VPNConfigurationSource>(
        histogram,
        VpnNetworkMetricsHelper::VPNConfigurationSource::kConfiguredManually,
        manual_count);
    histogram_tester_->ExpectBucketCount<
        VpnNetworkMetricsHelper::VPNConfigurationSource>(
        histogram,
        VpnNetworkMetricsHelper::VPNConfigurationSource::kConfiguredByPolicy,
        policy_count);
    histogram_tester_->ExpectTotalCount(histogram, manual_count + policy_count);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
};

TEST_F(VpnNetworkMetricsHelperTest, LogVpnVPNConfigurationSource) {
  const std::vector<std::pair<const char*, const char*>>
      kProvidersAndHistograms{{
          {shill::kProviderL2tpIpsec,
           kVpnHistogramConfigurationSourceL2tpIpsec},
          {shill::kProviderArcVpn, kVpnHistogramConfigurationSourceArc},
          {shill::kProviderOpenVpn, kVpnHistogramConfigurationSourceOpenVpn},
          {shill::kProviderThirdPartyVpn,
           kVpnHistogramConfigurationSourceThirdParty},
          {shill::kProviderWireGuard,
           kVpnHistogramConfigurationSourceWireGuard},
      }};

  for (const auto& it : kProvidersAndHistograms) {
    ExpectConfigurationSourceCounts(it.first, /*manual_count=*/0,
                                    /*policy_count=*/0);

    CreateTestShillConfiguration(it.first, /*is_managed=*/false);
    ExpectConfigurationSourceCounts(it.second, /*manual_count=*/1,
                                    /*policy_count=*/0);
    ClearServices();

    CreateTestShillConfiguration(it.first, /*is_managed=*/true);
    ExpectConfigurationSourceCounts(it.second, /*manual_count=*/1,
                                    /*policy_count=*/1);
    ClearServices();
  }
}

}  // namespace chromeos
