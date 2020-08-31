// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/service_key.h"

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/public/mdns_constants.h"

namespace openscreen {
namespace discovery {

// The InstanceKey ctor used below cares about the Instance ID of the
// MdnsRecord, while this class doesn't, so it's possible that the InstanceKey
// ctor will fail for this reason. That's not a problem though - A failure when
// creating an InstanceKey would mean that the record we received was invalid,
// so there is no reason to continue processing.
ServiceKey::ServiceKey(const MdnsRecord& record) {
  ErrorOr<ServiceKey> key = TryCreate(record);
  OSP_DCHECK(key.is_value());
  *this = std::move(key.value());
}

ServiceKey::ServiceKey(const DomainName& domain) {
  ErrorOr<ServiceKey> key = TryCreate(domain);
  OSP_DCHECK(key.is_value());
  *this = std::move(key.value());
}

ServiceKey::ServiceKey(absl::string_view service, absl::string_view domain)
    : service_id_(service.data(), service.size()),
      domain_id_(domain.data(), domain.size()) {
  OSP_DCHECK(IsServiceValid(service_id_));
  OSP_DCHECK(IsDomainValid(domain_id_));
}

ServiceKey::ServiceKey(const ServiceKey& other) = default;
ServiceKey::ServiceKey(ServiceKey&& other) = default;

ServiceKey& ServiceKey::operator=(const ServiceKey& rhs) = default;
ServiceKey& ServiceKey::operator=(ServiceKey&& rhs) = default;

// static
ErrorOr<ServiceKey> ServiceKey::TryCreate(const MdnsRecord& record) {
  return TryCreate(GetDomainName(record));
}

// static
ErrorOr<ServiceKey> ServiceKey::TryCreate(const DomainName& names) {
  // Size must be at least 4, because the minimum valid label is of the form
  // <instance>.<service type>.<protocol>.<domain>
  if (names.labels().size() < 4) {
    return Error::Code::kParameterInvalid;
  }

  // Skip the InstanceId.
  auto it = ++names.labels().begin();

  std::string service_name = *it++;
  const std::string protocol = *it++;
  const std::string service_id = service_name.append(".").append(protocol);
  if (!IsServiceValid(service_id)) {
    return Error::Code::kParameterInvalid;
  }

  const std::string domain_id = absl::StrJoin(it, names.labels().end(), ".");
  if (!IsDomainValid(domain_id)) {
    return Error::Code::kParameterInvalid;
  }

  return ServiceKey(service_id, domain_id);
}

}  // namespace discovery
}  // namespace openscreen
