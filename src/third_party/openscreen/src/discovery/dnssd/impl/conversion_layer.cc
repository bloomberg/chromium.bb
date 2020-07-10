// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/conversion_layer.h"

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/impl/instance_key.h"
#include "discovery/dnssd/impl/service_key.h"
#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "discovery/mdns/mdns_records.h"

namespace openscreen {
namespace discovery {
namespace {

void AddServiceInfoToLabels(const std::string& service,
                            const std::string& domain,
                            std::vector<std::string>* labels) {
  std::vector<std::string> service_labels = absl::StrSplit(service, '.');
  labels->insert(labels->end(), service_labels.begin(), service_labels.end());

  std::vector<std::string> domain_labels = absl::StrSplit(domain, '.');
  labels->insert(labels->end(), domain_labels.begin(), domain_labels.end());
}

DomainName GetPtrDomainName(const std::string& service,
                            const std::string& domain) {
  std::vector<std::string> labels;
  AddServiceInfoToLabels(service, domain, &labels);
  return DomainName{std::move(labels)};
}

DomainName GetInstanceDomainName(const std::string& instance,
                                 const std::string& service,
                                 const std::string& domain) {
  std::vector<std::string> labels;
  labels.emplace_back(instance);
  AddServiceInfoToLabels(service, domain, &labels);
  return DomainName{std::move(labels)};
}

MdnsRecord CreatePtrRecord(const DnsSdInstanceRecord& record,
                           const DomainName& domain) {
  PtrRecordRdata data(domain);

  // TTL specified by RFC 6762 section 10.
  constexpr std::chrono::seconds ttl(120);
  auto outer_domain = GetPtrDomainName(record.service_id(), record.domain_id());
  return MdnsRecord(std::move(outer_domain), DnsType::kPTR, DnsClass::kIN,
                    RecordType::kShared, ttl, std::move(data));
}

MdnsRecord CreateSrvRecord(const DnsSdInstanceRecord& record,
                           const DomainName& domain) {
  uint16_t port = record.port();

  // TTL specified by RFC 6762 section 10.
  constexpr std::chrono::seconds ttl(120);
  SrvRecordRdata data(0, 0, port, domain);
  return MdnsRecord(domain, DnsType::kSRV, DnsClass::kIN, RecordType::kUnique,
                    ttl, std::move(data));
}

absl::optional<MdnsRecord> CreateARecord(const DnsSdInstanceRecord& record,
                                         const DomainName& domain) {
  if (!record.address_v4().has_value()) {
    return absl::nullopt;
  }

  // TTL specified by RFC 6762 section 10.
  constexpr std::chrono::seconds ttl(120);
  ARecordRdata data(record.address_v4().value().address);
  return MdnsRecord(domain, DnsType::kA, DnsClass::kIN, RecordType::kUnique,
                    ttl, std::move(data));
}

absl::optional<MdnsRecord> CreateAAAARecord(const DnsSdInstanceRecord& record,
                                            const DomainName& domain) {
  if (!record.address_v6().has_value()) {
    return absl::nullopt;
  }

  // TTL specified by RFC 6762 section 10.
  constexpr std::chrono::seconds ttl(120);
  AAAARecordRdata data(record.address_v6().value().address);
  return MdnsRecord(domain, DnsType::kAAAA, DnsClass::kIN, RecordType::kUnique,
                    ttl, std::move(data));
}

MdnsRecord CreateTxtRecord(const DnsSdInstanceRecord& record,
                           const DomainName& domain) {
  TxtRecordRdata data(record.txt().GetData());

  // TTL specified by RFC 6762 section 10.
  constexpr std::chrono::seconds ttl(75 * 60);
  return MdnsRecord(domain, DnsType::kTXT, DnsClass::kIN, RecordType::kUnique,
                    ttl, std::move(data));
}

}  // namespace

ErrorOr<DnsSdTxtRecord> CreateFromDnsTxt(const TxtRecordRdata& txt_data) {
  DnsSdTxtRecord txt;
  if (txt_data.texts().size() == 1 && txt_data.texts()[0] == "") {
    return txt;
  }

  // Iterate backwards so that the first key of each type is the one that is
  // present at the end, as pet spec.
  for (auto it = txt_data.texts().rbegin(); it != txt_data.texts().rend();
       it++) {
    const std::string& text = *it;
    size_t index_of_eq = text.find_first_of('=');
    if (index_of_eq != std::string::npos) {
      if (index_of_eq == 0) {
        return Error::Code::kParameterInvalid;
      }
      std::string key = text.substr(0, index_of_eq);
      std::string value = text.substr(index_of_eq + 1);
      absl::Span<const uint8_t> data(
          reinterpret_cast<const uint8_t*>(value.c_str()), value.size());
      const auto set_result = txt.SetValue(key, data);
      if (!set_result.ok()) {
        return set_result;
      }
    } else {
      const auto set_result = txt.SetFlag(text, true);
      if (!set_result.ok()) {
        return set_result;
      }
    }
  }

  return txt;
}

DnsQueryInfo GetInstanceQueryInfo(const InstanceKey& key) {
  auto domain = GetInstanceDomainName(key.instance_id(), key.service_id(),
                                      key.domain_id());
  return {std::move(domain), DnsType::kANY, DnsClass::kANY};
}

DnsQueryInfo GetPtrQueryInfo(const ServiceKey& key) {
  auto domain = GetPtrDomainName(key.service_id(), key.domain_id());
  return {std::move(domain), DnsType::kPTR, DnsClass::kANY};
}

bool HasValidDnsRecordAddress(const MdnsRecord& record) {
  return InstanceKey::CreateFromRecord(record).is_value();
}

bool IsPtrRecord(const MdnsRecord& record) {
  return record.dns_type() == DnsType::kPTR;
}

std::vector<MdnsRecord> GetDnsRecords(const DnsSdInstanceRecord& record) {
  auto domain = GetInstanceDomainName(record.instance_id(), record.service_id(),
                                      record.domain_id());

  std::vector<MdnsRecord> records{CreatePtrRecord(record, domain),
                                  CreateSrvRecord(record, domain),
                                  CreateTxtRecord(record, domain)};

  auto v4 = CreateARecord(record, domain);
  if (v4.has_value()) {
    records.push_back(std::move(v4.value()));
  }

  auto v6 = CreateAAAARecord(record, domain);
  if (v6.has_value()) {
    records.push_back(std::move(v6.value()));
  }

  return records;
}

}  // namespace discovery
}  // namespace openscreen
