// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_publisher.h"

#include "discovery/mdns/mdns_sender.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {

MdnsPublisher::MdnsPublisher(MdnsQuerier* querier,
                             MdnsSender* sender,
                             platform::TaskRunner* task_runner,
                             MdnsRandom* random_delay)
    : querier_(querier),
      sender_(sender),
      task_runner_(task_runner),
      random_delay_(random_delay) {
  OSP_DCHECK(querier_);
  OSP_DCHECK(sender_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(random_delay_);
}

MdnsPublisher::~MdnsPublisher() = default;

Error MdnsPublisher::RegisterRecord(const MdnsRecord& record) {
  // TODO(rwkeane): Implement this method.
  return Error::None();
}

Error MdnsPublisher::UpdateRegisteredRecord(const MdnsRecord& old_record,
                                            const MdnsRecord& new_record) {
  // TODO(rwkeane): Implement this method.
  return Error::None();
}

Error MdnsPublisher::UnregisterRecord(const MdnsRecord& record) {
  // TODO(rwkeane): Implement this method.
  return Error::None();
}

bool MdnsPublisher::IsExclusiveOwner(const DomainName& name) {
  // TODO(rwkeane): Implement this method.
  return false;
}

bool MdnsPublisher::HasRecords(const DomainName& name,
                               DnsType type,
                               DnsClass clazz) {
  // TODO(rwkeane): Implement this method.
  return false;
}
std::vector<MdnsRecord::ConstRef> MdnsPublisher::GetRecords(
    const DomainName& name,
    DnsType type,
    DnsClass clazz) {
  // TODO(rwkeane): Implement this method.
  return std::vector<MdnsRecord::ConstRef>();
}

}  // namespace discovery
}  // namespace openscreen
