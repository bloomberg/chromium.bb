// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/internal_services.h"

#include <algorithm>
#include <utility>

#include "osp/impl/discovery/mdns/mdns_responder_adapter_impl.h"
#include "osp/impl/mdns_responder_service.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "util/logging.h"

namespace openscreen {
namespace osp {
namespace {

constexpr char kServiceName[] = "_openscreen";
constexpr char kServiceProtocol[] = "_udp";
const IPAddress kMulticastAddress{224, 0, 0, 251};
const IPAddress kMulticastIPv6Address{
    // ff02::fb
    0xff02, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00fb,
};
const uint16_t kMulticastListeningPort = 5353;

class MdnsResponderAdapterImplFactory final
    : public MdnsResponderAdapterFactory {
 public:
  MdnsResponderAdapterImplFactory() = default;
  ~MdnsResponderAdapterImplFactory() override = default;

  std::unique_ptr<MdnsResponderAdapter> Create() override {
    return std::make_unique<MdnsResponderAdapterImpl>();
  }
};

Error SetUpMulticastSocket(UdpSocket* socket, NetworkInterfaceIndex ifindex) {
  const IPAddress broadcast_address =
      socket->IsIPv6() ? kMulticastIPv6Address : kMulticastAddress;

  socket->JoinMulticastGroup(broadcast_address, ifindex);
  socket->SetMulticastOutboundInterface(ifindex);
  socket->Bind();

  return Error::None();
}

// Ref-counted singleton instance of InternalServices. This lives only as long
// as there is at least one ServiceListener and/or ServicePublisher alive.
InternalServices* g_instance = nullptr;
int g_instance_ref_count = 0;

}  // namespace

// static
std::unique_ptr<ServiceListener> InternalServices::CreateListener(
    const MdnsServiceListenerConfig& config,
    ServiceListener::Observer* observer,
    TaskRunner* task_runner) {
  auto* services = ReferenceSingleton(task_runner);
  auto listener =
      std::make_unique<ServiceListenerImpl>(&services->mdns_service_);
  listener->AddObserver(observer);
  listener->SetDestructionCallback(&InternalServices::DereferenceSingleton,
                                   services);
  return listener;
}

// static
std::unique_ptr<ServicePublisher> InternalServices::CreatePublisher(
    const ServicePublisher::Config& config,
    ServicePublisher::Observer* observer,
    TaskRunner* task_runner) {
  auto* services = ReferenceSingleton(task_runner);
  services->mdns_service_.SetServiceConfig(
      config.hostname, config.service_instance_name,
      config.connection_server_port, config.network_interface_indices,
      {{"fn", config.friendly_name}});
  auto publisher = std::make_unique<ServicePublisherImpl>(
      observer, &services->mdns_service_);
  publisher->SetDestructionCallback(&InternalServices::DereferenceSingleton,
                                    services);
  return publisher;
}

InternalServices::InternalPlatformLinkage::InternalPlatformLinkage(
    InternalServices* parent)
    : parent_(parent) {}

InternalServices::InternalPlatformLinkage::~InternalPlatformLinkage() {
  // If there are open sockets, then there will be dangling references to
  // destroyed objects after destruction.
  OSP_CHECK(open_sockets_.empty());
}

std::vector<MdnsPlatformService::BoundInterface>
InternalServices::InternalPlatformLinkage::RegisterInterfaces(
    const std::vector<NetworkInterfaceIndex>& whitelist) {
  const std::vector<InterfaceInfo> interfaces = GetNetworkInterfaces();
  const bool do_filter_using_whitelist = !whitelist.empty();
  std::vector<NetworkInterfaceIndex> index_list;
  for (const auto& interface : interfaces) {
    OSP_VLOG << "Found interface: " << interface;
    if (do_filter_using_whitelist &&
        std::find(whitelist.begin(), whitelist.end(), interface.index) ==
            whitelist.end()) {
      OSP_VLOG << "Ignoring interface not in whitelist: " << interface;
      continue;
    }
    if (!interface.addresses.empty())
      index_list.push_back(interface.index);
  }
  OSP_LOG_IF(WARN, index_list.empty())
      << "No network interfaces had usable addresses for mDNS.";

  // Set up sockets to send and listen to mDNS multicast traffic on all
  // interfaces.
  std::vector<BoundInterface> result;
  for (NetworkInterfaceIndex index : index_list) {
    const auto& interface = *std::find_if(
        interfaces.begin(), interfaces.end(),
        [index](const InterfaceInfo& info) { return info.index == index; });
    if (interface.addresses.empty()) {
      continue;
    }

    // Pick any address for the given interface.
    const IPSubnet& primary_subnet = interface.addresses.front();

    auto create_result =
        UdpSocket::Create(parent_->task_runner_, parent_,
                          IPEndpoint{{}, kMulticastListeningPort});
    if (!create_result) {
      OSP_LOG_ERROR << "failed to create socket for interface " << index << ": "
                    << create_result.error().message();
      continue;
    }
    std::unique_ptr<UdpSocket> socket = std::move(create_result.value());
    if (!SetUpMulticastSocket(socket.get(), index).ok()) {
      continue;
    }
    result.emplace_back(interface, primary_subnet, socket.get());
    parent_->RegisterMdnsSocket(socket.get());

    open_sockets_.emplace_back(std::move(socket));
  }

  return result;
}

void InternalServices::InternalPlatformLinkage::DeregisterInterfaces(
    const std::vector<BoundInterface>& registered_interfaces) {
  for (const auto& interface : registered_interfaces) {
    UdpSocket* const socket = interface.socket;
    parent_->DeregisterMdnsSocket(socket);

    const auto it = std::find_if(open_sockets_.begin(), open_sockets_.end(),
                                 [socket](const std::unique_ptr<UdpSocket>& s) {
                                   return s.get() == socket;
                                 });
    OSP_DCHECK(it != open_sockets_.end());
    open_sockets_.erase(it);
  }
}

InternalServices::InternalServices(ClockNowFunctionPtr now_function,
                                   TaskRunner* task_runner)
    : mdns_service_(now_function,
                    task_runner,
                    kServiceName,
                    kServiceProtocol,
                    std::make_unique<MdnsResponderAdapterImplFactory>(),
                    std::make_unique<InternalPlatformLinkage>(this)),
      task_runner_(task_runner) {}

InternalServices::~InternalServices() = default;

void InternalServices::RegisterMdnsSocket(UdpSocket* socket) {
  OSP_CHECK(g_instance) << "No listener or publisher is alive.";
  // TODO(rwkeane): Hook this up to the new mDNS library once we swap out the
  // mDNSResponder.
}

void InternalServices::DeregisterMdnsSocket(UdpSocket* socket) {
  // TODO(rwkeane): Hook this up to the new mDNS library once we swap out the
  // mDNSResponder.
}

// static
InternalServices* InternalServices::ReferenceSingleton(
    TaskRunner* task_runner) {
  if (!g_instance) {
    OSP_CHECK_EQ(g_instance_ref_count, 0);
    g_instance = new InternalServices(&Clock::now, task_runner);
  }
  ++g_instance_ref_count;
  return g_instance;
}

// static
void InternalServices::DereferenceSingleton(void* instance) {
  OSP_CHECK_EQ(static_cast<InternalServices*>(instance), g_instance);
  OSP_CHECK_GT(g_instance_ref_count, 0);
  --g_instance_ref_count;
  if (g_instance_ref_count == 0) {
    delete g_instance;
    g_instance = nullptr;
  }
}

void InternalServices::OnError(UdpSocket* socket, Error error) {
  OSP_LOG_ERROR << "failed to configure socket " << error.message();
  this->DeregisterMdnsSocket(socket);
}

void InternalServices::OnSendError(UdpSocket* socket, Error error) {
  // TODO(crbug.com/openscreen/67): Implement this method.
  OSP_UNIMPLEMENTED();
}

void InternalServices::OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) {
  g_instance->mdns_service_.OnRead(socket, std::move(packet));
}

}  // namespace osp
}  // namespace openscreen
