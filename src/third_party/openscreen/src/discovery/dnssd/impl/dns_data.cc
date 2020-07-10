// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/dns_data.h"

#include "absl/types/optional.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/mdns/mdns_records.h"

namespace openscreen {
namespace discovery {
namespace {

template <typename T>
inline Error CreateRecord(absl::optional<T>* stored, const MdnsRecord& record) {
  if (stored->has_value()) {
    return Error::Code::kItemAlreadyExists;
  }
  *stored = absl::get<T>(record.rdata());
  return Error::None();
}

template <typename T>
inline Error UpdateRecord(absl::optional<T>* stored, const MdnsRecord& record) {
  if (!stored->has_value()) {
    return Error::Code::kItemNotFound;
  }
  *stored = absl::get<T>(record.rdata());
  return Error::None();
}

template <typename T>
inline Error DeleteRecord(absl::optional<T>* stored) {
  if (!stored->has_value()) {
    return Error::Code::kItemNotFound;
  }
  *stored = absl::nullopt;
  return Error::None();
}

template <typename T>
inline Error ProcessRecord(absl::optional<T>* stored,
                           const MdnsRecord& record,
                           RecordChangedEvent event) {
  switch (event) {
    case RecordChangedEvent::kCreated:
      return CreateRecord(stored, record);
    case RecordChangedEvent::kUpdated:
      return UpdateRecord(stored, record);
    case RecordChangedEvent::kExpired:
      return DeleteRecord(stored);
  }
  return Error::Code::kUnknownError;
}

}  // namespace

DnsData::DnsData(const InstanceKey& instance_id) : instance_id_(instance_id) {}

ErrorOr<DnsSdInstanceRecord> DnsData::CreateRecord() {
  if (!srv_.has_value() || !txt_.has_value() ||
      (!a_.has_value() && !aaaa_.has_value())) {
    return Error::Code::kOperationInvalid;
  }

  ErrorOr<DnsSdTxtRecord> txt_or_error = CreateFromDnsTxt(txt_.value());
  if (txt_or_error.is_error()) {
    return txt_or_error.error();
  }

  if (a_.has_value() && aaaa_.has_value()) {
    return DnsSdInstanceRecord(
        instance_id_.instance_id(), instance_id_.service_id(),
        instance_id_.domain_id(),
        {a_.value().ipv4_address(), srv_.value().port()},
        {aaaa_.value().ipv6_address(), srv_.value().port()},
        std::move(txt_or_error.value()));
  } else {
    IPEndpoint ep =
        a_.has_value()
            ? IPEndpoint{a_.value().ipv4_address(), srv_.value().port()}
            : IPEndpoint{aaaa_.value().ipv6_address(), srv_.value().port()};
    return DnsSdInstanceRecord(instance_id_.instance_id(),
                               instance_id_.service_id(),
                               instance_id_.domain_id(), std::move(ep),
                               std::move(txt_or_error.value()));
  }
}

Error DnsData::ApplyDataRecordChange(const MdnsRecord& record,
                                     RecordChangedEvent event) {
  switch (record.dns_type()) {
    case DnsType::kSRV:
      return ProcessRecord(&srv_, record, event);
    case DnsType::kTXT:
      return ProcessRecord(&txt_, record, event);
    case DnsType::kA:
      return ProcessRecord(&a_, record, event);
    case DnsType::kAAAA:
      return ProcessRecord(&aaaa_, record, event);
    default:
      return Error::Code::kOperationInvalid;
  }
}

}  // namespace discovery
}  // namespace openscreen
