// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_normalizer.h"

#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

// Validate that StaticIPConfig IPAddress and dependent fields will be removed
// if IPAddressConfigType is not 'Static'.
TEST(ONCNormalizerTest, RemoveUnnecessaryAddressStaticIPConfigFields) {
  Normalizer normalizer(true);
  base::Value data =
      test_utils::ReadTestDictionaryValue("settings_with_normalization.json");

  const base::Value* original =
      data.FindDictKey("unnecessary-address-staticipconfig");
  const base::Value* expected_normalized =
      data.FindDictKey("unnecessary-address-staticipconfig-normalized");

  base::Value actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}

// Validate that StaticIPConfig fields other than NameServers and IPAddress &
// friends will be retained even without static
// {NameServers,IPAddress}ConfigType.
TEST(ONCNormalizerTest, RetainExtraStaticIPConfigFields) {
  Normalizer normalizer(true);
  base::Value data =
      test_utils::ReadTestDictionaryValue("settings_with_normalization.json");

  const base::Value* original =
      data.FindDictKey("unnecessary-address-staticipconfig");
  const base::Value* expected_normalized =
      data.FindDictKey("unnecessary-address-staticipconfig-normalized");

  base::Value actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}

// Validate that irrelevant fields of the StaticIPConfig dictionary will be
// removed.
TEST(ONCNormalizerTest, RemoveStaticIPConfigFields) {
  Normalizer normalizer(true);
  base::Value data =
      test_utils::ReadTestDictionaryValue("settings_with_normalization.json");

  const base::Value* original =
      data.FindDictKey("irrelevant-staticipconfig-fields");
  const base::Value* expected_normalized =
      data.FindDictKey("irrelevant-staticipconfig-fields-normalized");

  base::Value actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}

// Validate that StatticIPConfig.NameServers is removed when
// NameServersConfigType is 'DHCP'.
TEST(ONCNormalizerTest, RemoveNameServers) {
  Normalizer normalizer(true);
  base::Value data =
      test_utils::ReadTestDictionaryValue("settings_with_normalization.json");

  const base::Value* original = data.FindDictKey("irrelevant-nameservers");
  const base::Value* expected_normalized =
      data.FindDictKey("irrelevant-nameservers-normalized");

  base::Value actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}

// Validate that IPConfig related fields are removed when IPAddressConfigType
// is 'Static', but some required fields are missing.
TEST(ONCNormalizerTest, RemoveIPFieldsForIncompleteConfig) {
  Normalizer normalizer(true);
  base::Value data =
      test_utils::ReadTestDictionaryValue("settings_with_normalization.json");

  const base::Value* original = data.FindDictKey("missing-ip-fields");
  const base::Value* expected_normalized =
      data.FindDictKey("missing-ip-fields-normalized");

  base::Value actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}
// This test case is about validating valid ONC objects.
TEST(ONCNormalizerTest, NormalizeNetworkConfigurationEthernetAndVPN) {
  Normalizer normalizer(true);
  base::Value data =
      test_utils::ReadTestDictionaryValue("settings_with_normalization.json");

  const base::Value* original = data.FindDictKey("ethernet-and-vpn");
  const base::Value* expected_normalized =
      data.FindDictKey("ethernet-and-vpn-normalized");

  base::Value actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}

// This test case is about validating valid ONC objects.
TEST(ONCNormalizerTest, NormalizeNetworkConfigurationWifi) {
  Normalizer normalizer(true);
  base::Value data =
      test_utils::ReadTestDictionaryValue("settings_with_normalization.json");

  const base::Value* original = data.FindDictKey("wifi");
  const base::Value* expected_normalized = data.FindDictKey("wifi-normalized");

  base::Value actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}

}  // namespace onc
}  // namespace chromeos
