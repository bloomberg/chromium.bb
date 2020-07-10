// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/testing/fake_dns_record_factory.h"

namespace openscreen {
namespace discovery {

// static
MdnsRecord FakeDnsRecordFactory::CreateFullyPopulatedSrvRecord(uint16_t port) {
  const DomainName kTarget{kInstanceName, "_srv-name", "_udp", kDomainName};
  constexpr auto kType = DnsType::kSRV;
  constexpr auto kClazz = DnsClass::kIN;
  constexpr auto kRecordType = RecordType::kUnique;
  constexpr auto kTtl = std::chrono::seconds(0);
  SrvRecordRdata srv(0, 0, port, kTarget);
  return MdnsRecord(kTarget, kType, kClazz, kRecordType, kTtl, std::move(srv));
}

// static
const IPAddress FakeDnsRecordFactory::kV4Address = IPAddress(192, 168, 0, 0);

// static
const IPAddress FakeDnsRecordFactory::kV6Address =
    IPAddress(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

// static
const IPEndpoint FakeDnsRecordFactory::kV4Endpoint{
    FakeDnsRecordFactory::kV4Address, FakeDnsRecordFactory::kPortNum};

// static
const IPEndpoint FakeDnsRecordFactory::kV6Endpoint{
    FakeDnsRecordFactory::kV6Address, FakeDnsRecordFactory::kPortNum};

// static
const std::string FakeDnsRecordFactory::kInstanceName = "instance";

// static
const std::string FakeDnsRecordFactory::kServiceName = "_srv-name._udp";

// static
const std::string FakeDnsRecordFactory::kServiceNameProtocolPart = "_udp";

// static
const std::string FakeDnsRecordFactory::kServiceNameServicePart = "_srv-name";

// static
const std::string FakeDnsRecordFactory::kDomainName = "local";

// static
const InstanceKey FakeDnsRecordFactory::kKey =
    InstanceKey(kInstanceName, kServiceName, kDomainName);

}  // namespace discovery
}  // namespace openscreen
