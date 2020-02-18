// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_MDNS_RESPONDER_SERVICE_H_
#define OSP_IMPL_MDNS_RESPONDER_SERVICE_H_

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "osp/impl/discovery/mdns/mdns_responder_adapter.h"
#include "osp/impl/mdns_platform_service.h"
#include "osp/impl/service_listener_impl.h"
#include "osp/impl/service_publisher_impl.h"
#include "platform/api/network_interface.h"
#include "platform/api/network_runner.h"
#include "platform/base/ip_address.h"

namespace openscreen {

class MdnsResponderAdapterFactory {
 public:
  virtual ~MdnsResponderAdapterFactory() = default;

  virtual std::unique_ptr<mdns::MdnsResponderAdapter> Create() = 0;
};

class MdnsResponderService : public ServiceListenerImpl::Delegate,
                             public ServicePublisherImpl::Delegate,
                             public platform::UdpReadCallback {
 public:
  MdnsResponderService(
      platform::NetworkRunner* network_runner,
      const std::string& service_name,
      const std::string& service_protocol,
      std::unique_ptr<MdnsResponderAdapterFactory> mdns_responder_factory,
      std::unique_ptr<MdnsPlatformService> platform);
  virtual ~MdnsResponderService() override;

  void SetServiceConfig(
      const std::string& hostname,
      const std::string& instance,
      uint16_t port,
      const std::vector<platform::NetworkInterfaceIndex> whitelist,
      const std::map<std::string, std::string>& txt_data);

  // UdpReadCallback overrides.
  void OnRead(platform::UdpPacket packet,
              platform::NetworkRunner* network_runner) override;

  // ServiceListenerImpl::Delegate overrides.
  void StartListener() override;
  void StartAndSuspendListener() override;
  void StopListener() override;
  void SuspendListener() override;
  void ResumeListener() override;
  void SearchNow(ServiceListener::State from) override;

  // ServicePublisherImpl::Delegate overrides.
  void StartPublisher() override;
  void StartAndSuspendPublisher() override;
  void StopPublisher() override;
  void SuspendPublisher() override;
  void ResumePublisher() override;

 protected:
  void HandleMdnsEvents();

  std::unique_ptr<mdns::MdnsResponderAdapter> mdns_responder_;

 private:
  // Create internal versions of all public methods. These are used to push all
  // calls to these methods to the task runner.
  // TODO(rwkeane): Clean up these methods. Some result in multiple pushes to
  // the task runner when just one would suffice.
  // ServiceListenerImpl::Delegate overrides.
  void StartListenerInternal();
  void StartAndSuspendListenerInternal();
  void StopListenerInternal();
  void SuspendListenerInternal();
  void ResumeListenerInternal();
  void SearchNowInternal(ServiceListener::State from);
  void StartPublisherInternal();
  void StartAndSuspendPublisherInternal();
  void StopPublisherInternal();
  void SuspendPublisherInternal();
  void ResumePublisherInternal();

  // NOTE: service_instance implicit in map key.
  struct ServiceInstance {
    platform::UdpSocket* ptr_socket = nullptr;
    mdns::DomainName domain_name;
    uint16_t port = 0;
    bool has_ptr_record = false;
    std::vector<std::string> txt_info;

    // |port| == 0 signals that we have no SRV record.
    bool has_srv() const { return port != 0; }
  };

  // NOTE: hostname implicit in map key.
  struct HostInfo {
    std::vector<ServiceInstance*> services;
    IPAddress v4_address;
    IPAddress v6_address;
  };

  struct NetworkScopedDomainName {
    platform::UdpSocket* socket;
    mdns::DomainName domain_name;
  };

  struct NetworkScopedDomainNameComparator {
    bool operator()(const NetworkScopedDomainName& a,
                    const NetworkScopedDomainName& b) const;
  };

  using InstanceNameSet =
      std::set<mdns::DomainName, mdns::DomainNameComparator>;

  void StartListening();
  void StopListening();
  void StartService();
  void StopService();
  void StopMdnsResponder();
  void UpdatePendingServiceInfoSet(InstanceNameSet* modified_instance_names,
                                   const mdns::DomainName& domain_name);
  void RemoveAllReceivers();

  // NOTE: |modified_instance_names| is used to track which service instances
  // are modified by the record events.  See HandleMdnsEvents for more details.
  bool HandlePtrEvent(const mdns::PtrEvent& ptr_event,
                      InstanceNameSet* modified_instance_names);
  bool HandleSrvEvent(const mdns::SrvEvent& srv_event,
                      InstanceNameSet* modified_instance_names);
  bool HandleTxtEvent(const mdns::TxtEvent& txt_event,
                      InstanceNameSet* modified_instance_names);
  bool HandleAddressEvent(platform::UdpSocket* socket,
                          mdns::QueryEventHeader::Type response_type,
                          const mdns::DomainName& domain_name,
                          bool a_event,
                          const IPAddress& address,
                          InstanceNameSet* modified_instance_names);
  bool HandleAEvent(const mdns::AEvent& a_event,
                    InstanceNameSet* modified_instance_names);
  bool HandleAaaaEvent(const mdns::AaaaEvent& aaaa_event,
                       InstanceNameSet* modified_instance_names);

  HostInfo* AddOrGetHostInfo(platform::UdpSocket* socket,
                             const mdns::DomainName& domain_name);
  HostInfo* GetHostInfo(platform::UdpSocket* socket,
                        const mdns::DomainName& domain_name);
  bool IsServiceReady(const ServiceInstance& instance, HostInfo* host) const;
  platform::NetworkInterfaceIndex GetNetworkInterfaceIndexFromSocket(
      const platform::UdpSocket* socket) const;

  // Service type separated as service name and service protocol for both
  // listening and publishing (e.g. {"_openscreen", "_udp"}).
  std::array<std::string, 2> service_type_;

  // The following variables all relate to what MdnsResponderService publishes,
  // if anything.
  std::string service_hostname_;
  std::string service_instance_name_;
  uint16_t service_port_;
  std::vector<platform::NetworkInterfaceIndex> interface_index_whitelist_;
  std::map<std::string, std::string> service_txt_data_;

  std::unique_ptr<MdnsResponderAdapterFactory> mdns_responder_factory_;
  std::unique_ptr<MdnsPlatformService> platform_;
  std::vector<MdnsPlatformService::BoundInterface> bound_interfaces_;

  // A map of service information collected from PTR, SRV, and TXT records.  It
  // is keyed by service instance names.
  std::map<mdns::DomainName,
           std::unique_ptr<ServiceInstance>,
           mdns::DomainNameComparator>
      service_by_name_;

  // The map key is a combination of the interface to which the address records
  // belong and the hostname of the address records.  The values are IPAddresses
  // for the given hostname on the given network and pointers to dependent
  // service instances.  The service instance pointers act as a reference count
  // to keep the A/AAAA queries alive, when more than one service refers to the
  // same hostname.  This is not currently used by openscreen, but is used by
  // Cast, so may be supported in openscreen in the future.
  std::map<NetworkScopedDomainName, HostInfo, NetworkScopedDomainNameComparator>
      network_scoped_domain_to_host_;

  std::map<std::string, ServiceInfo> receiver_info_;

  platform::NetworkRunner* network_runner_;

  friend class TestingMdnsResponderService;
};

}  // namespace openscreen

#endif  // OSP_IMPL_MDNS_RESPONDER_SERVICE_H_
