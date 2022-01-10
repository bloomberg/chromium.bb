// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/shill_property_util.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chromeos/network/network_ui_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

class ShillPropertyUtilTest : public testing::Test {
 public:
  ShillPropertyUtilTest() = default;
  ~ShillPropertyUtilTest() override = default;

 protected:
  std::string GetPolicyFromSource(const ::onc::ONCSource& onc_source);
};

std::string ShillPropertyUtilTest::GetPolicyFromSource(
    const ::onc::ONCSource& onc_source) {
  base::DictionaryValue dictionary;
  dictionary.SetKey(shill::kTypeProperty, base::Value(shill::kTypeWifi));
  shill_property_util::SetRandomMACPolicy(onc_source, &dictionary);

  return *dictionary.FindStringKey(shill::kWifiRandomMACPolicy);
}

TEST_F(ShillPropertyUtilTest, MACRandomizationOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kWifiConnectMacAddressRandomization);

  EXPECT_EQ(GetPolicyFromSource(::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY),
            shill::kWifiRandomMacPolicyHardware);
  EXPECT_EQ(GetPolicyFromSource(::onc::ONCSource::ONC_SOURCE_USER_POLICY),
            shill::kWifiRandomMacPolicyHardware);
  EXPECT_EQ(GetPolicyFromSource(::onc::ONCSource::ONC_SOURCE_USER_IMPORT),
            shill::kWifiRandomMacPolicyHardware);
  EXPECT_EQ(GetPolicyFromSource(::onc::ONCSource::ONC_SOURCE_NONE),
            shill::kWifiRandomMacPolicyHardware);
  EXPECT_EQ(GetPolicyFromSource(::onc::ONCSource::ONC_SOURCE_UNKNOWN),
            shill::kWifiRandomMacPolicyHardware);
}

TEST_F(ShillPropertyUtilTest, MACRandomizationOn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kWifiConnectMacAddressRandomization);

  EXPECT_EQ(GetPolicyFromSource(::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY),
            shill::kWifiRandomMacPolicyHardware);
  EXPECT_EQ(GetPolicyFromSource(::onc::ONCSource::ONC_SOURCE_USER_POLICY),
            shill::kWifiRandomMacPolicyHardware);
  EXPECT_EQ(GetPolicyFromSource(::onc::ONCSource::ONC_SOURCE_USER_IMPORT),
            shill::kWifiRandomMacPolicyHardware);
  EXPECT_EQ(GetPolicyFromSource(::onc::ONCSource::ONC_SOURCE_NONE),
            shill::kWifiRandomMacPolicyPersistentRandom);
  EXPECT_EQ(GetPolicyFromSource(::onc::ONCSource::ONC_SOURCE_UNKNOWN),
            shill::kWifiRandomMacPolicyPersistentRandom);
}
}  // namespace chromeos
