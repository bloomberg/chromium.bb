// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/querier_impl.h"

#include <string>
#include <vector>

#include "util/logging.h"

namespace openscreen {
namespace discovery {
namespace {

static constexpr char kLocalDomain[] = "local";

}  // namespace

QuerierImpl::QuerierImpl(MdnsService* mdns_querier)
    : mdns_querier_(mdns_querier) {
  OSP_DCHECK(mdns_querier_);
}

QuerierImpl::~QuerierImpl() = default;

void QuerierImpl::StartQuery(absl::string_view service, Callback* callback) {
  OSP_DCHECK(callback);

  ServiceKey key(service, kLocalDomain);
  DnsQueryInfo query = GetPtrQueryInfo(key);
  if (!IsQueryRunning(key)) {
    callback_map_[key] = {};
    StartDnsQuery(query);
  } else {
    const std::vector<InstanceKey> keys = GetMatchingInstances(key);
    for (const auto& key : keys) {
      auto it = received_records_.find(key);
      if (it == received_records_.end()) {
        continue;
      }

      ErrorOr<DnsSdInstanceRecord> record = it->second.CreateRecord();
      if (record.is_value()) {
        callback->OnInstanceCreated(record.value());
      }
    }
  }
  callback_map_[key].push_back(callback);
}

bool QuerierImpl::IsQueryRunning(absl::string_view service) const {
  return IsQueryRunning(ServiceKey(service, kLocalDomain));
}

void QuerierImpl::StopQuery(absl::string_view service, Callback* callback) {
  OSP_DCHECK(callback);

  ServiceKey key(service, kLocalDomain);
  DnsQueryInfo query = GetPtrQueryInfo(key);
  auto callback_it = callback_map_.find(key);
  if (callback_it == callback_map_.end()) {
    return;
  }
  std::vector<Callback*>* callbacks = &callback_it->second;

  const auto it = std::find(callbacks->begin(), callbacks->end(), callback);
  if (it != callbacks->end()) {
    callbacks->erase(it);
    if (callbacks->empty()) {
      EraseInstancesOf(key);
      callback_map_.erase(callback_it);
      StopDnsQuery(query);
    }
  }
}

void QuerierImpl::OnRecordChanged(const MdnsRecord& record,
                                  RecordChangedEvent event) {
  IsPtrRecord(record) ? HandlePtrRecordChange(record, event)
                      : HandleNonPtrRecordChange(record, event);
}

Error QuerierImpl::HandlePtrRecordChange(const MdnsRecord& record,
                                         RecordChangedEvent event) {
  if (!HasValidDnsRecordAddress(record)) {
    // This means that the received record is malformed.
    return Error::Code::kParameterInvalid;
  }

  InstanceKey key(record);
  DnsQueryInfo query = GetInstanceQueryInfo(key);
  switch (event) {
    case RecordChangedEvent::kCreated:
      StartDnsQuery(query);
      return Error::None();
    case RecordChangedEvent::kExpired:
      StopDnsQuery(query);
      EraseInstancesOf(ServiceKey(key));
      return Error::None();
    case RecordChangedEvent::kUpdated:
      return Error::Code::kOperationInvalid;
  }
  return Error::Code::kUnknownError;
}

Error QuerierImpl::HandleNonPtrRecordChange(const MdnsRecord& record,
                                            RecordChangedEvent event) {
  if (!HasValidDnsRecordAddress(record)) {
    // This means that the call received had malformed data.
    return Error::Code::kParameterInvalid;
  }

  const ServiceKey key(record);
  if (!IsQueryRunning(key)) {
    // This means that the call was already queued up on the TaskRunner when the
    // callback was removed. The caller no longer cares, so drop the record.
    return Error::Code::kOperationCancelled;
  }
  const std::vector<Callback*>& callbacks = callback_map_[key];

  // Get the current InstanceRecord data associated with the received record.
  const InstanceKey id(record);
  ErrorOr<DnsSdInstanceRecord> old_instance_record = Error::Code::kItemNotFound;
  auto it = received_records_.find(id);
  if (it == received_records_.end()) {
    it = received_records_.emplace(id, DnsData(id)).first;
  } else {
    old_instance_record = it->second.CreateRecord();
  }
  DnsData* data = &it->second;

  // Apply the changes specified by the received event to the stored
  // InstanceRecord.
  Error apply_result = data->ApplyDataRecordChange(record, event);
  if (!apply_result.ok()) {
    OSP_LOG_ERROR << "Received erroneous record change. Error: "
                  << apply_result;
    return apply_result;
  }

  // Send an update to the user, based on how the new and old records compare.
  ErrorOr<DnsSdInstanceRecord> new_instance_record = data->CreateRecord();
  NotifyCallbacks(callbacks, old_instance_record, new_instance_record);

  return Error::None();
}

void QuerierImpl::NotifyCallbacks(
    const std::vector<Callback*>& callbacks,
    const ErrorOr<DnsSdInstanceRecord>& old_record,
    const ErrorOr<DnsSdInstanceRecord>& new_record) {
  if (old_record.is_value() && new_record.is_value()) {
    for (Callback* callback : callbacks) {
      callback->OnInstanceUpdated(new_record.value());
    }
  } else if (old_record.is_value() && !new_record.is_value()) {
    for (Callback* callback : callbacks) {
      callback->OnInstanceDeleted(old_record.value());
    }
  } else if (!old_record.is_value() && new_record.is_value()) {
    for (Callback* callback : callbacks) {
      callback->OnInstanceCreated(new_record.value());
    }
  }
}

void QuerierImpl::EraseInstancesOf(const ServiceKey& key) {
  std::vector<InstanceKey> keys = GetMatchingInstances(key);
  const auto it = callback_map_.find(key);
  std::vector<Callback*> callbacks;
  if (it != callback_map_.end()) {
    callbacks = it->second;
  }

  for (const auto& key : keys) {
    auto recieved_record = received_records_.find(key);
    if (recieved_record == received_records_.end()) {
      continue;
    }

    ErrorOr<DnsSdInstanceRecord> instance_record =
        recieved_record->second.CreateRecord();
    if (instance_record.is_value()) {
      for (Callback* callback : callbacks) {
        callback->OnInstanceDeleted(instance_record.value());
      }
    }

    received_records_.erase(recieved_record);
  }
}

std::vector<InstanceKey> QuerierImpl::GetMatchingInstances(
    const ServiceKey& key) {
  // Because only one or two PTR queries are expected at a time, expect >=1/2 of
  // the records to be associated with a given PTR. They can't be removed in
  // less than O(n) time, so just iterate across them all.
  std::vector<InstanceKey> keys;
  for (auto it = received_records_.begin(); it != received_records_.end();
       it++) {
    if (it->first.IsInstanceOf(key)) {
      keys.push_back(it->first);
    }
  }

  return keys;
}

void QuerierImpl::StartDnsQuery(const DnsQueryInfo& query) {
  mdns_querier_->StartQuery(query.name, query.dns_type, query.dns_class, this);
}

void QuerierImpl::StopDnsQuery(const DnsQueryInfo& query) {
  mdns_querier_->StopQuery(query.name, query.dns_type, query.dns_class, this);
}

}  // namespace discovery
}  // namespace openscreen
