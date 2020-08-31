// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_SERVICE_IMPL_H_
#define DISCOVERY_MDNS_MDNS_SERVICE_IMPL_H_

#include "discovery/common/config.h"
#include "discovery/mdns/mdns_domain_confirmed_provider.h"
#include "discovery/mdns/mdns_probe_manager.h"
#include "discovery/mdns/mdns_publisher.h"
#include "discovery/mdns/mdns_querier.h"
#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_reader.h"
#include "discovery/mdns/mdns_receiver.h"
#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/mdns_responder.h"
#include "discovery/mdns/mdns_sender.h"
#include "discovery/mdns/mdns_writer.h"
#include "discovery/mdns/public/mdns_constants.h"
#include "discovery/mdns/public/mdns_service.h"
#include "platform/api/udp_socket.h"

namespace openscreen {

class TaskRunner;

namespace discovery {

class NetworkConfig;
class ReportingClient;

class MdnsServiceImpl : public MdnsService, public UdpSocket::Client {
 public:
  // |task_runner|, |reporting_client|, and |config| must exist for the duration
  // of this instance's life.
  MdnsServiceImpl(TaskRunner* task_runner,
                  ClockNowFunctionPtr now_function,
                  ReportingClient* reporting_client,
                  const Config& config,
                  NetworkInterfaceIndex network_interface,
                  Config::NetworkInfo::AddressFamilies supported_address_types);
  ~MdnsServiceImpl() override;

  // MdnsService Overrides.
  void StartQuery(const DomainName& name,
                  DnsType dns_type,
                  DnsClass dns_class,
                  MdnsRecordChangedCallback* callback) override;
  void StopQuery(const DomainName& name,
                 DnsType dns_type,
                 DnsClass dns_class,
                 MdnsRecordChangedCallback* callback) override;
  void ReinitializeQueries(const DomainName& name) override;
  Error StartProbe(MdnsDomainConfirmedProvider* callback,
                   DomainName requested_name,
                   IPAddress address) override;

  Error RegisterRecord(const MdnsRecord& record) override;
  Error UpdateRegisteredRecord(const MdnsRecord& old_record,
                               const MdnsRecord& new_record) override;
  Error UnregisterRecord(const MdnsRecord& record) override;

  // UdpSocket::Client overrides.
  void OnError(UdpSocket* socket, Error error) override;
  void OnSendError(UdpSocket* socket, Error error) override;
  void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) override;

 private:
  TaskRunner* const task_runner_;
  ClockNowFunctionPtr now_function_;
  ReportingClient* const reporting_client_;

  MdnsRandom random_delay_;
  MdnsReceiver receiver_;

  // Sockets to send and receive mDNS Data according to RFC 6762.
  std::unique_ptr<UdpSocket> socket_v4_;
  std::unique_ptr<UdpSocket> socket_v6_;

  // unique_ptrs are used for the below objects so that they can be initialized
  // in the body of the ctor, after send_socket is initialized.
  std::unique_ptr<MdnsSender> sender_;
  std::unique_ptr<MdnsQuerier> querier_;
  std::unique_ptr<MdnsProbeManagerImpl> probe_manager_;
  std::unique_ptr<MdnsPublisher> publisher_;
  std::unique_ptr<MdnsResponder> responder_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_SERVICE_IMPL_H_
