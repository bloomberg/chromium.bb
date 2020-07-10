// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_QUERIER_H_
#define DISCOVERY_MDNS_MDNS_QUERIER_H_

#include <map>

#include "discovery/mdns/mdns_record_changed_callback.h"
#include "discovery/mdns/mdns_records.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {

class MdnsRandom;
class MdnsReceiver;
class MdnsSender;
class MdnsQuestionTracker;
class MdnsRecordTracker;

class MdnsQuerier {
 public:
  using ClockNowFunctionPtr = openscreen::platform::ClockNowFunctionPtr;
  using TaskRunner = openscreen::platform::TaskRunner;

  MdnsQuerier(MdnsSender* sender,
              MdnsReceiver* receiver,
              TaskRunner* task_runner,
              ClockNowFunctionPtr now_function,
              MdnsRandom* random_delay);
  MdnsQuerier(const MdnsQuerier& other) = delete;
  MdnsQuerier(MdnsQuerier&& other) noexcept = delete;
  MdnsQuerier& operator=(const MdnsQuerier& other) = delete;
  MdnsQuerier& operator=(MdnsQuerier&& other) noexcept = delete;
  ~MdnsQuerier();

  // Starts an mDNS query with the given name, DNS type, and DNS class.  Updated
  // records are passed to |callback|.  The caller must ensure |callback|
  // remains alive while it is registered with a query.
  void StartQuery(const DomainName& name,
                  DnsType dns_type,
                  DnsClass dns_class,
                  MdnsRecordChangedCallback* callback);

  // Stops an mDNS query with the given name, DNS type, and DNS class.
  // |callback| must be the same callback pointer that was previously passed to
  // StartQuery.
  void StopQuery(const DomainName& name,
                 DnsType dns_type,
                 DnsClass dns_class,
                 MdnsRecordChangedCallback* callback);

 private:
  struct CallbackInfo {
    MdnsRecordChangedCallback* const callback;
    const DnsType dns_type;
    const DnsClass dns_class;
  };

  // Callback passed to MdnsReceiver
  void OnMessageReceived(const MdnsMessage& message);

  // Callback passed to owned MdnsRecordTrackers
  void OnRecordExpired(const MdnsRecord& record);

  void ProcessRecords(const std::vector<MdnsRecord>& records);
  void ProcessSharedRecord(const MdnsRecord& record);
  void ProcessUniqueRecord(const MdnsRecord& record);
  void ProcessCallbacks(const MdnsRecord& record, RecordChangedEvent event);

  void AddQuestion(const MdnsQuestion& question);
  void AddRecord(const MdnsRecord& record);

  MdnsSender* const sender_;
  MdnsReceiver* const receiver_;
  TaskRunner* const task_runner_;
  const ClockNowFunctionPtr now_function_;
  MdnsRandom* const random_delay_;

  // A collection of active question trackers, each is uniquely identified by
  // domain name, DNS record type, and DNS record class. Multimap key is domain
  // name only to allow easy support for wildcard processing for DNS record type
  // and class. MdnsQuestionTracker instances are stored as unique_ptr so they
  // are not moved around in memory when the collection is modified. This allows
  // passing a pointer to MdnsQuestionTracker to a task running on the
  // TaskRunner.
  std::multimap<DomainName, std::unique_ptr<MdnsQuestionTracker>> questions_;

  // A collection of active known record trackers, each is identified by domain
  // name, DNS record type, and DNS record class. Multimap key is domain name
  // only to allow easy support for wildcard processing for DNS record type and
  // class and allow storing shared records that differ only in RDATA.
  // MdnsRecordTracker instances are stored as unique_ptr so they are not moved
  // around in memory when the collection is modified. This allows passing a
  // pointer to MdnsQuestionTracker to a task running on the TaskRunner.
  std::multimap<DomainName, std::unique_ptr<MdnsRecordTracker>> records_;

  // A collection of callbacks passed to StartQuery method. Each is identified
  // by domain name, DNS record type, and DNS record class, but there can be
  // more than one callback for a particular query. Multimap key is domain name
  // only to allow easy matching of records against callbacks that have wildcard
  // DNS class and/or DNS type.
  std::multimap<DomainName, CallbackInfo> callbacks_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_QUERIER_H_
