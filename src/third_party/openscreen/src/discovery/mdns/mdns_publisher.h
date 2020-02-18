// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_PUBLISHER_H_
#define DISCOVERY_MDNS_MDNS_PUBLISHER_H_

#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/mdns_responder.h"

namespace openscreen {

namespace platform {
class TaskRunner;
}  // namespace platform

namespace discovery {

// TODO(rwkeane): Add API for claiming a DomainName as described in RFC 6762
// Section 8.1's probing phase.
class MdnsPublisher : public MdnsResponder::RecordHandler {
 public:
  // |querier|, |sender|, |task_runner|, and |random_delay| must all persist for
  // the duration of this object's lifetime
  MdnsPublisher(MdnsQuerier* querier,
                MdnsSender* sender,
                platform::TaskRunner* task_runner,
                MdnsRandom* random_delay);
  ~MdnsPublisher();

  // Registers a new mDNS record for advertisement by this service. For A, AAAA,
  // SRV, and TXT records, the domain name must have already been claimed by the
  // ClaimExclusiveOwnership() method and for PTR records the name being pointed
  // to must have been claimed in the same fashion.
  Error RegisterRecord(const MdnsRecord& record);

  // Updates the existing record with name matching the name of the new record.
  Error UpdateRegisteredRecord(const MdnsRecord& old_record,
                               const MdnsRecord& new_record);

  // Stops advertising the provided record. If no more records with the provided
  // name are bing advertised after this call's completion, then ownership of
  // the name is released.
  Error UnregisterRecord(const MdnsRecord& record);

  OSP_DISALLOW_COPY_AND_ASSIGN(MdnsPublisher);

 private:
  // MdnsResponder::RecordHandler overrides.
  bool IsExclusiveOwner(const DomainName& name) override;
  bool HasRecords(const DomainName& name,
                  DnsType type,
                  DnsClass clazz) override;
  std::vector<MdnsRecord::ConstRef> GetRecords(const DomainName& name,
                                               DnsType type,
                                               DnsClass clazz) override;

  MdnsQuerier* const querier_;
  MdnsSender* const sender_;
  platform::TaskRunner* const task_runner_;
  MdnsRandom* const random_delay_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_PUBLISHER_H_
