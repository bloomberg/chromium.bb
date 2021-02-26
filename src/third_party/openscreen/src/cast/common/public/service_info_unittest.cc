// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/public/service_info.h"

#include "cast/common/public/testing/discovery_utils.h"
#include "discovery/dnssd/public/dns_sd_instance.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace cast {
namespace {

constexpr NetworkInterfaceIndex kNetworkInterface = 0;

}

TEST(ServiceInfoTests, ConvertValidFromDnsSd) {
  std::string instance = "InstanceId";
  discovery::DnsSdTxtRecord txt = CreateValidTxt();
  discovery::DnsSdInstanceEndpoint record(
      instance, kCastV2ServiceId, kCastV2DomainId, txt, kNetworkInterface,
      kEndpointV4, kEndpointV6);
  ErrorOr<ServiceInfo> info = DnsSdInstanceEndpointToServiceInfo(record);
  ASSERT_TRUE(info.is_value());
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_TRUE(info.value().v4_address);
  EXPECT_EQ(info.value().v4_address, kAddressV4);
  EXPECT_TRUE(info.value().v6_address);
  EXPECT_EQ(info.value().v6_address, kAddressV6);
  EXPECT_EQ(info.value().port, kPort);
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_EQ(info.value().protocol_version, kTestVersion);
  EXPECT_EQ(info.value().capabilities, kCapabilitiesParsed);
  EXPECT_EQ(info.value().status, kStatusParsed);
  EXPECT_EQ(info.value().model_name, kModelName);
  EXPECT_EQ(info.value().friendly_name, kFriendlyName);

  record = discovery::DnsSdInstanceEndpoint(instance, kCastV2ServiceId,
                                            kCastV2DomainId, txt,
                                            kNetworkInterface, kEndpointV4);
  info = DnsSdInstanceEndpointToServiceInfo(record);
  ASSERT_TRUE(info.is_value());
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_TRUE(info.value().v4_address);
  EXPECT_EQ(info.value().v4_address, kAddressV4);
  EXPECT_FALSE(info.value().v6_address);
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_EQ(info.value().protocol_version, kTestVersion);
  EXPECT_EQ(info.value().capabilities, kCapabilitiesParsed);
  EXPECT_EQ(info.value().status, kStatusParsed);
  EXPECT_EQ(info.value().model_name, kModelName);
  EXPECT_EQ(info.value().friendly_name, kFriendlyName);

  record = discovery::DnsSdInstanceEndpoint(instance, kCastV2ServiceId,
                                            kCastV2DomainId, txt,
                                            kNetworkInterface, kEndpointV6);
  info = DnsSdInstanceEndpointToServiceInfo(record);
  ASSERT_TRUE(info.is_value());
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_FALSE(info.value().v4_address);
  EXPECT_TRUE(info.value().v6_address);
  EXPECT_EQ(info.value().v6_address, kAddressV6);
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_EQ(info.value().protocol_version, kTestVersion);
  EXPECT_EQ(info.value().capabilities, kCapabilitiesParsed);
  EXPECT_EQ(info.value().status, kStatusParsed);
  EXPECT_EQ(info.value().model_name, kModelName);
  EXPECT_EQ(info.value().friendly_name, kFriendlyName);
}

TEST(ServiceInfoTests, ConvertInvalidFromDnsSd) {
  std::string instance = "InstanceId";
  discovery::DnsSdTxtRecord txt = CreateValidTxt();
  txt.ClearValue(kUniqueIdKey);
  discovery::DnsSdInstanceEndpoint record(
      instance, kCastV2ServiceId, kCastV2DomainId, txt, kNetworkInterface,
      kEndpointV4, kEndpointV6);
  EXPECT_TRUE(DnsSdInstanceEndpointToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kVersionId);
  record = discovery::DnsSdInstanceEndpoint(
      instance, kCastV2ServiceId, kCastV2DomainId, txt, kNetworkInterface,
      kEndpointV4, kEndpointV6);
  EXPECT_TRUE(DnsSdInstanceEndpointToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kCapabilitiesId);
  record = discovery::DnsSdInstanceEndpoint(
      instance, kCastV2ServiceId, kCastV2DomainId, txt, kNetworkInterface,
      kEndpointV4, kEndpointV6);
  EXPECT_TRUE(DnsSdInstanceEndpointToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kStatusId);
  record = discovery::DnsSdInstanceEndpoint(
      instance, kCastV2ServiceId, kCastV2DomainId, txt, kNetworkInterface,
      kEndpointV4, kEndpointV6);
  EXPECT_TRUE(DnsSdInstanceEndpointToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kFriendlyNameId);
  record = discovery::DnsSdInstanceEndpoint(
      instance, kCastV2ServiceId, kCastV2DomainId, txt, kNetworkInterface,
      kEndpointV4, kEndpointV6);
  EXPECT_TRUE(DnsSdInstanceEndpointToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kModelNameId);
  record = discovery::DnsSdInstanceEndpoint(
      instance, kCastV2ServiceId, kCastV2DomainId, txt, kNetworkInterface,
      kEndpointV4, kEndpointV6);
  EXPECT_TRUE(DnsSdInstanceEndpointToServiceInfo(record).is_error());
}

TEST(ServiceInfoTests, ConvertValidToDnsSd) {
  ServiceInfo info;
  info.v4_address = kAddressV4;
  info.v6_address = kAddressV6;
  info.port = kPort;
  info.unique_id = kTestUniqueId;
  info.protocol_version = kTestVersion;
  info.capabilities = kCapabilitiesParsed;
  info.status = kStatusParsed;
  info.model_name = kModelName;
  info.friendly_name = kFriendlyName;
  discovery::DnsSdInstance instance = ServiceInfoToDnsSdInstance(info);
  CompareTxtString(instance.txt(), kUniqueIdKey, kTestUniqueId);
  CompareTxtString(instance.txt(), kCapabilitiesId, kCapabilitiesString);
  CompareTxtString(instance.txt(), kModelNameId, kModelName);
  CompareTxtString(instance.txt(), kFriendlyNameId, kFriendlyName);
  CompareTxtInt(instance.txt(), kVersionId, kTestVersion);
  CompareTxtInt(instance.txt(), kStatusId, kStatus);
}

}  // namespace cast
}  // namespace openscreen
