// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/conversion_layer.h"

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/impl/instance_key.h"
#include "discovery/dnssd/impl/service_key.h"
#include "discovery/dnssd/public/dns_sd_instance.h"
#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/public/mdns_constants.h"

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

inline DomainName GetInstanceDomainName(const InstanceKey& key) {
  return GetInstanceDomainName(key.instance_id(), key.service_id(),
                               key.domain_id());
}

MdnsRecord CreatePtrRecord(const DnsSdInstance& instance,
                           const DomainName& domain) {
  PtrRecordRdata data(domain);
  auto outer_domain =
      GetPtrDomainName(instance.service_id(), instance.domain_id());
  return MdnsRecord(std::move(outer_domain), DnsType::kPTR, DnsClass::kIN,
                    RecordType::kShared, kPtrRecordTtl, std::move(data));
}

MdnsRecord CreateSrvRecord(const DnsSdInstance& instance,
                           const DomainName& domain) {
  uint16_t port = instance.port();
  SrvRecordRdata data(0, 0, port, domain);
  return MdnsRecord(domain, DnsType::kSRV, DnsClass::kIN, RecordType::kUnique,
                    kSrvRecordTtl, std::move(data));
}

absl::optional<MdnsRecord> CreateARecord(const DnsSdInstanceEndpoint& endpoint,
                                         const DomainName& domain) {
  if (!endpoint.address_v4()) {
    return absl::nullopt;
  }

  ARecordRdata data(endpoint.address_v4());
  return MdnsRecord(domain, DnsType::kA, DnsClass::kIN, RecordType::kUnique,
                    kARecordTtl, std::move(data));
}

absl::optional<MdnsRecord> CreateAAAARecord(
    const DnsSdInstanceEndpoint& endpoint,
    const DomainName& domain) {
  if (!endpoint.address_v6()) {
    return absl::nullopt;
  }

  AAAARecordRdata data(endpoint.address_v6());
  return MdnsRecord(domain, DnsType::kAAAA, DnsClass::kIN, RecordType::kUnique,
                    kAAAARecordTtl, std::move(data));
}

MdnsRecord CreateTxtRecord(const DnsSdInstance& endpoint,
                           const DomainName& domain) {
  TxtRecordRdata data(endpoint.txt().GetData());
  return MdnsRecord(domain, DnsType::kTXT, DnsClass::kIN, RecordType::kUnique,
                    kTXTRecordTtl, std::move(data));
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
          reinterpret_cast<const uint8_t*>(value.data()), value.size());
      const auto set_result =
          txt.SetValue(key, std::vector<uint8_t>(data.begin(), data.end()));
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

DomainName GetDomainName(const InstanceKey& key) {
  return GetInstanceDomainName(key.instance_id(), key.service_id(),
                               key.domain_id());
}

DomainName GetDomainName(const MdnsRecord& record) {
  return IsPtrRecord(record)
             ? absl::get<PtrRecordRdata>(record.rdata()).ptr_domain()
             : record.name();
}

DnsQueryInfo GetInstanceQueryInfo(const InstanceKey& key) {
  return {GetDomainName(key), DnsType::kANY, DnsClass::kANY};
}

DnsQueryInfo GetPtrQueryInfo(const ServiceKey& key) {
  auto domain = GetPtrDomainName(key.service_id(), key.domain_id());
  return {std::move(domain), DnsType::kPTR, DnsClass::kANY};
}

bool HasValidDnsRecordAddress(const MdnsRecord& record) {
  return HasValidDnsRecordAddress(GetDomainName(record));
}

bool HasValidDnsRecordAddress(const DomainName& domain) {
  return InstanceKey::TryCreate(domain).is_value() &&
         IsInstanceValid(domain.labels()[0]);
}

bool IsPtrRecord(const MdnsRecord& record) {
  return record.dns_type() == DnsType::kPTR;
}

std::vector<MdnsRecord> GetDnsRecords(const DnsSdInstance& instance) {
  auto domain = GetInstanceDomainName(InstanceKey(instance));

  return {CreatePtrRecord(instance, domain), CreateSrvRecord(instance, domain),
          CreateTxtRecord(instance, domain)};
}

std::vector<MdnsRecord> GetDnsRecords(const DnsSdInstanceEndpoint& endpoint) {
  auto domain = GetInstanceDomainName(InstanceKey(endpoint));

  std::vector<MdnsRecord> records =
      GetDnsRecords(static_cast<DnsSdInstance>(endpoint));

  auto v4 = CreateARecord(endpoint, domain);
  if (v4.has_value()) {
    records.push_back(std::move(v4.value()));
  }

  auto v6 = CreateAAAARecord(endpoint, domain);
  if (v6.has_value()) {
    records.push_back(std::move(v6.value()));
  }

  return records;
}

}  // namespace discovery
}  // namespace openscreen
