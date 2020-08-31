// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/testing/mdns_test_util.h"

#include <utility>
#include <vector>

namespace openscreen {
namespace discovery {

TxtRecordRdata MakeTxtRecord(std::initializer_list<absl::string_view> strings) {
  std::vector<TxtRecordRdata::Entry> texts;
  for (const auto& string : strings) {
    texts.push_back(TxtRecordRdata::Entry(string.begin(), string.end()));
  }
  return TxtRecordRdata(std::move(texts));
}

MdnsRecord GetFakePtrRecord(const DomainName& target,
                            std::chrono::seconds ttl) {
  DomainName name(++target.labels().begin(), target.labels().end());
  PtrRecordRdata rdata(target);
  return MdnsRecord(std::move(name), DnsType::kPTR, DnsClass::kIN,
                    RecordType::kShared, ttl, rdata);
}

MdnsRecord GetFakeSrvRecord(const DomainName& name, std::chrono::seconds ttl) {
  SrvRecordRdata rdata(0, 0, 80, name);
  return MdnsRecord(name, DnsType::kSRV, DnsClass::kIN, RecordType::kUnique,
                    ttl, rdata);
}

MdnsRecord GetFakeTxtRecord(const DomainName& name, std::chrono::seconds ttl) {
  TxtRecordRdata rdata;
  return MdnsRecord(name, DnsType::kTXT, DnsClass::kIN, RecordType::kUnique,
                    ttl, rdata);
}

MdnsRecord GetFakeARecord(const DomainName& name, std::chrono::seconds ttl) {
  ARecordRdata rdata(IPAddress(192, 168, 0, 0));
  return MdnsRecord(name, DnsType::kA, DnsClass::kIN, RecordType::kUnique, ttl,
                    rdata);
}

MdnsRecord GetFakeAAAARecord(const DomainName& name, std::chrono::seconds ttl) {
  AAAARecordRdata rdata(IPAddress(1, 2, 3, 4, 5, 6, 7, 8));
  return MdnsRecord(name, DnsType::kAAAA, DnsClass::kIN, RecordType::kUnique,
                    ttl, rdata);
}

}  // namespace discovery
}  // namespace openscreen
