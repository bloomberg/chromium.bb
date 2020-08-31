// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_QUERIER_IMPL_H_
#define DISCOVERY_DNSSD_IMPL_QUERIER_IMPL_H_

#include <map>
#include <unordered_map>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/impl/dns_data.h"
#include "discovery/dnssd/impl/instance_key.h"
#include "discovery/dnssd/impl/service_key.h"
#include "discovery/dnssd/public/dns_sd_instance_endpoint.h"
#include "discovery/dnssd/public/dns_sd_querier.h"
#include "discovery/mdns/mdns_record_changed_callback.h"
#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/public/mdns_service.h"

namespace openscreen {
namespace discovery {

class NetworkInterfaceConfig;

class QuerierImpl : public DnsSdQuerier, public MdnsRecordChangedCallback {
 public:
  // |querier|, |task_runner|, and |network_config| must outlive the QuerierImpl
  // instance constructed.
  QuerierImpl(MdnsService* querier,
              TaskRunner* task_runner,
              const NetworkInterfaceConfig* network_config);
  ~QuerierImpl() override;

  bool IsQueryRunning(const std::string& service) const;

  // DnsSdQuerier overrides.
  void StartQuery(const std::string& service, Callback* callback) override;
  void StopQuery(const std::string& service, Callback* callback) override;
  void ReinitializeQueries(const std::string& service) override;

  // MdnsRecordChangedCallback overrides.
  std::vector<PendingQueryChange> OnRecordChanged(
      const MdnsRecord& record,
      RecordChangedEvent event) override;

 private:
  // Process an OnRecordChanged event for a PTR record.
  ErrorOr<std::vector<PendingQueryChange>> HandlePtrRecordChange(
      const MdnsRecord& record,
      RecordChangedEvent event);

  // Process an OnRecordChanged event for non-PTR records (SRV, TXT, A, and AAAA
  // records).
  Error HandleNonPtrRecordChange(const MdnsRecord& record,
                                 RecordChangedEvent event);

  inline bool IsQueryRunning(const ServiceKey& key) const {
    return callback_map_.find(key) != callback_map_.end();
  }

  std::vector<DnsQueryInfo> GetDataToStopDnsQuery(ServiceKey key);
  std::vector<DnsQueryInfo> GetDataToStartDnsQuery(ServiceKey key);
  std::vector<DnsQueryInfo> GetDataToStopDnsQuery(
      InstanceKey key,
      bool should_inform_callbacks = true);
  std::vector<DnsQueryInfo> GetDataToStartDnsQuery(InstanceKey key);

  void StartDnsQueriesImmediately(const std::vector<DnsQueryInfo>& query_infos);
  void StopDnsQueriesImmediately(const std::vector<DnsQueryInfo>& query_infos);

  std::vector<PendingQueryChange> StartDnsQueriesDelayed(
      std::vector<DnsQueryInfo> query_infos);
  std::vector<PendingQueryChange> StopDnsQueriesDelayed(
      std::vector<DnsQueryInfo> query_infos);

  // Calls the appropriate callback method based on the provided Instance
  // Endpoint values.
  void NotifyCallbacks(const std::vector<Callback*>& callbacks,
                       const ErrorOr<DnsSdInstanceEndpoint>& old_endpoint,
                       const ErrorOr<DnsSdInstanceEndpoint>& new_endpoint);

  // Map from a specific service instance to the data received so far about
  // that instance. The keys in this map are the instances for which an
  // associated PTR record has been received, and the values are the set of
  // non-PTR records received which describe that service (if any). Note that,
  // with this definition, it is possible for a InstanceKey to be mapped to an
  // empty DnsData if the instance has no associated records yet.
  std::unordered_map<InstanceKey, DnsData, absl::Hash<InstanceKey>>
      received_records_;

  // Map from the (service, domain) pairs currently being queried for to the
  // callbacks to call when new InstanceEndpoints are available.
  std::map<ServiceKey, std::vector<Callback*>> callback_map_;

  MdnsService* const mdns_querier_;
  TaskRunner* const task_runner_;

  const NetworkInterfaceConfig* const network_config_;

  friend class QuerierImplTesting;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_QUERIER_IMPL_H_
