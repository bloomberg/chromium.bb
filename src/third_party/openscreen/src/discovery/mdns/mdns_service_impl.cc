// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_service_impl.h"

namespace openscreen {
namespace discovery {

// static
std::unique_ptr<MdnsService> MdnsService::Create(TaskRunner* task_runner) {
  return std::make_unique<MdnsServiceImpl>();
}

void MdnsServiceImpl::StartQuery(const DomainName& name,
                                 DnsType dns_type,
                                 DnsClass dns_class,
                                 MdnsRecordChangedCallback* callback) {
  // TODO(yakimakha): Implement this method
}

void MdnsServiceImpl::StopQuery(const DomainName& name,
                                DnsType dns_type,
                                DnsClass dns_class,
                                MdnsRecordChangedCallback* callback) {
  // TODO(yakimakha): Implement this method
}

void MdnsServiceImpl::RegisterRecord(const MdnsRecord& record) {
  // TODO(yakimakha): Implement this method
}

void MdnsServiceImpl::DeregisterRecord(const MdnsRecord& record) {
  // TODO(yakimakha): Implement this method
}

}  // namespace discovery
}  // namespace openscreen
