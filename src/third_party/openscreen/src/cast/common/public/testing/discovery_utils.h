// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_PUBLIC_TESTING_DISCOVERY_UTILS_H_
#define CAST_COMMON_PUBLIC_TESTING_DISCOVERY_UTILS_H_

#include "cast/common/public/service_info.h"
#include "discovery/dnssd/public/dns_sd_txt_record.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace cast {

// Constants used for testing.
static const IPAddress kAddressV4(192, 168, 0, 0);
static const IPAddress kAddressV6(1, 2, 3, 4, 5, 6, 7, 8);
static constexpr uint16_t kPort = 80;
static const IPEndpoint kEndpointV4{kAddressV4, kPort};
static const IPEndpoint kEndpointV6{kAddressV6, kPort};
static constexpr char kTestUniqueId[] = "1234";
static constexpr char kFriendlyName[] = "Friendly Name 123";
static constexpr char kModelName[] = "Openscreen";
static constexpr char kInstanceId[] = "Openscreen-1234";
static constexpr uint8_t kTestVersion = 0;
static constexpr char kCapabilitiesString[] = "3";
static constexpr char kCapabilitiesStringLong[] = "000003";
static constexpr ReceiverCapabilities kCapabilitiesParsed =
    static_cast<ReceiverCapabilities>(0x03);
static constexpr uint8_t kStatus = 0x01;
static constexpr ReceiverStatus kStatusParsed = ReceiverStatus::kBusy;

discovery::DnsSdTxtRecord CreateValidTxt();

void CompareTxtString(const discovery::DnsSdTxtRecord& txt,
                      const std::string& key,
                      const std::string& expected);

void CompareTxtInt(const discovery::DnsSdTxtRecord& txt,
                   const std::string& key,
                   uint8_t expected);

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_PUBLIC_TESTING_DISCOVERY_UTILS_H_
