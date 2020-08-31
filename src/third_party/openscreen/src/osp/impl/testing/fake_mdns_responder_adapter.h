// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_TESTING_FAKE_MDNS_RESPONDER_ADAPTER_H_
#define OSP_IMPL_TESTING_FAKE_MDNS_RESPONDER_ADAPTER_H_

#include <set>
#include <vector>

#include "osp/impl/discovery/mdns/mdns_responder_adapter.h"

namespace openscreen {
namespace osp {

class FakeMdnsResponderAdapter;

PtrEvent MakePtrEvent(const std::string& service_instance,
                      const std::string& service_type,
                      const std::string& service_protocol,
                      UdpSocket* socket);

SrvEvent MakeSrvEvent(const std::string& service_instance,
                      const std::string& service_type,
                      const std::string& service_protocol,
                      const std::string& hostname,
                      uint16_t port,
                      UdpSocket* socket);

TxtEvent MakeTxtEvent(const std::string& service_instance,
                      const std::string& service_type,
                      const std::string& service_protocol,
                      const std::vector<std::string>& txt_lines,
                      UdpSocket* socket);

AEvent MakeAEvent(const std::string& hostname,
                  IPAddress address,
                  UdpSocket* socket);

AaaaEvent MakeAaaaEvent(const std::string& hostname,
                        IPAddress address,
                        UdpSocket* socket);

void AddEventsForNewService(FakeMdnsResponderAdapter* mdns_responder,
                            const std::string& service_instance,
                            const std::string& service_name,
                            const std::string& service_protocol,
                            const std::string& hostname,
                            uint16_t port,
                            const std::vector<std::string>& txt_lines,
                            const IPAddress& address,
                            UdpSocket* socket);

class FakeMdnsResponderAdapter final : public MdnsResponderAdapter {
 public:
  struct RegisteredInterface {
    InterfaceInfo interface_info;
    IPSubnet interface_address;
    UdpSocket* socket;
  };

  struct RegisteredService {
    std::string service_instance;
    std::string service_name;
    std::string service_protocol;
    DomainName target_host;
    uint16_t target_port;
    std::map<std::string, std::string> txt_data;
  };

  class LifetimeObserver {
   public:
    virtual ~LifetimeObserver() = default;

    virtual void OnDestroyed() = 0;
  };

  virtual ~FakeMdnsResponderAdapter() override;

  void SetLifetimeObserver(LifetimeObserver* observer) { observer_ = observer; }

  void AddPtrEvent(PtrEvent&& ptr_event);
  void AddSrvEvent(SrvEvent&& srv_event);
  void AddTxtEvent(TxtEvent&& txt_event);
  void AddAEvent(AEvent&& a_event);
  void AddAaaaEvent(AaaaEvent&& aaaa_event);

  const std::vector<RegisteredInterface>& registered_interfaces() {
    return registered_interfaces_;
  }
  const std::vector<RegisteredService>& registered_services() {
    return registered_services_;
  }
  bool ptr_queries_empty() const;
  bool srv_queries_empty() const;
  bool txt_queries_empty() const;
  bool a_queries_empty() const;
  bool aaaa_queries_empty() const;
  bool running() const { return running_; }

  // UdpSocket::Client overrides.
  void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) override;
  void OnSendError(UdpSocket* socket, Error error) override;
  void OnError(UdpSocket* socket, Error error) override;

  // MdnsResponderAdapter overrides.
  Error Init() override;
  void Close() override;

  Error SetHostLabel(const std::string& host_label) override;

  // TODO(btolsch): Reject/OSP_CHECK events that don't match any registered
  // interface?
  Error RegisterInterface(const InterfaceInfo& interface_info,
                          const IPSubnet& interface_address,
                          UdpSocket* socket) override;
  Error DeregisterInterface(UdpSocket* socket) override;

  Clock::duration RunTasks() override;

  std::vector<PtrEvent> TakePtrResponses() override;
  std::vector<SrvEvent> TakeSrvResponses() override;
  std::vector<TxtEvent> TakeTxtResponses() override;
  std::vector<AEvent> TakeAResponses() override;
  std::vector<AaaaEvent> TakeAaaaResponses() override;

  MdnsResponderErrorCode StartPtrQuery(UdpSocket* socket,
                                       const DomainName& service_type) override;
  MdnsResponderErrorCode StartSrvQuery(
      UdpSocket* socket,
      const DomainName& service_instance) override;
  MdnsResponderErrorCode StartTxtQuery(
      UdpSocket* socket,
      const DomainName& service_instance) override;
  MdnsResponderErrorCode StartAQuery(UdpSocket* socket,
                                     const DomainName& domain_name) override;
  MdnsResponderErrorCode StartAaaaQuery(UdpSocket* socket,
                                        const DomainName& domain_name) override;

  MdnsResponderErrorCode StopPtrQuery(UdpSocket* socket,
                                      const DomainName& service_type) override;
  MdnsResponderErrorCode StopSrvQuery(
      UdpSocket* socket,
      const DomainName& service_instance) override;
  MdnsResponderErrorCode StopTxtQuery(
      UdpSocket* socket,
      const DomainName& service_instance) override;
  MdnsResponderErrorCode StopAQuery(UdpSocket* socket,
                                    const DomainName& domain_name) override;
  MdnsResponderErrorCode StopAaaaQuery(UdpSocket* socket,
                                       const DomainName& domain_name) override;

  MdnsResponderErrorCode RegisterService(
      const std::string& service_instance,
      const std::string& service_name,
      const std::string& service_protocol,
      const DomainName& target_host,
      uint16_t target_port,
      const std::map<std::string, std::string>& txt_data) override;
  MdnsResponderErrorCode DeregisterService(
      const std::string& service_instance,
      const std::string& service_name,
      const std::string& service_protocol) override;
  MdnsResponderErrorCode UpdateTxtData(
      const std::string& service_instance,
      const std::string& service_name,
      const std::string& service_protocol,
      const std::map<std::string, std::string>& txt_data) override;

 private:
  struct InterfaceQueries {
    std::set<DomainName, DomainNameComparator> a_queries;
    std::set<DomainName, DomainNameComparator> aaaa_queries;
    std::set<DomainName, DomainNameComparator> ptr_queries;
    std::set<DomainName, DomainNameComparator> srv_queries;
    std::set<DomainName, DomainNameComparator> txt_queries;
  };

  bool running_ = false;
  LifetimeObserver* observer_ = nullptr;

  std::map<UdpSocket*, InterfaceQueries> queries_;
  // NOTE: One of many simplifications here is that there is no cache.  This
  // means that calling StartQuery, StopQuery, StartQuery will only return an
  // event the first time, unless the test also adds the event a second time.
  std::vector<PtrEvent> ptr_events_;
  std::vector<SrvEvent> srv_events_;
  std::vector<TxtEvent> txt_events_;
  std::vector<AEvent> a_events_;
  std::vector<AaaaEvent> aaaa_events_;

  std::vector<RegisteredInterface> registered_interfaces_;
  std::vector<RegisteredService> registered_services_;
};

}  // namespace osp
}  // namespace openscreen

#endif  // OSP_IMPL_TESTING_FAKE_MDNS_RESPONDER_ADAPTER_H_
