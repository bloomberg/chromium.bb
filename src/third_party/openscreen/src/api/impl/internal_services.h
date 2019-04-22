// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_INTERNAL_SERVICES_H_
#define API_IMPL_INTERNAL_SERVICES_H_

#include <memory>
#include <vector>

#include "api/impl/mdns_platform_service.h"
#include "api/impl/mdns_responder_service.h"
#include "api/impl/quic/quic_connection_factory.h"
#include "api/impl/service_listener_impl.h"
#include "api/impl/service_publisher_impl.h"
#include "api/public/mdns_service_listener_factory.h"
#include "api/public/mdns_service_publisher_factory.h"
#include "api/public/protocol_connection_client.h"
#include "api/public/protocol_connection_server.h"
#include "base/ip_address.h"
#include "base/macros.h"
#include "platform/api/event_waiter.h"
#include "platform/api/network_interface.h"
#include "platform/api/socket.h"
#include "platform/base/event_loop.h"

namespace openscreen {

// Factory for ServiceListener and ServicePublisher instances; owns internal
// objects needed to instantiate them such as MdnsResponderService and runs an
// event loop.
// TODO(btolsch): This may be renamed and/or split up once QUIC code lands and
// this use case is more concrete.
class InternalServices {
 public:
  static void RunEventLoopOnce();

  static std::unique_ptr<ServiceListener> CreateListener(
      const MdnsServiceListenerConfig& config,
      ServiceListener::Observer* observer);
  static std::unique_ptr<ServicePublisher> CreatePublisher(
      const ServicePublisher::Config& config,
      ServicePublisher::Observer* observer);

 private:
  class InternalPlatformLinkage final : public MdnsPlatformService {
   public:
    explicit InternalPlatformLinkage(InternalServices* parent);
    ~InternalPlatformLinkage() override;

    std::vector<BoundInterface> RegisterInterfaces(
        const std::vector<platform::NetworkInterfaceIndex>& whitelist) override;
    void DeregisterInterfaces(
        const std::vector<BoundInterface>& registered_interfaces) override;

   private:
    InternalServices* const parent_;
    std::vector<platform::UdpSocketUniquePtr> open_sockets_;
  };

  InternalServices();
  ~InternalServices();

  void RegisterMdnsSocket(platform::UdpSocket* socket);
  void DeregisterMdnsSocket(platform::UdpSocket* socket);

  static InternalServices* ReferenceSingleton();
  static void DereferenceSingleton(void* instance);

  MdnsResponderService mdns_service_;

  // TODO(btolsch): To support e.g. both QUIC and mDNS listening for separate
  // sockets, we need to either:
  //  - give them their own individual waiter objects
  //  - remember who registered for what in a wrapper here
  //  - something else...
  // Currently, RegisterMdnsSocket is our hook to do 1 or 2.
  platform::EventWaiterPtr mdns_waiter_;

  DISALLOW_COPY_AND_ASSIGN(InternalServices);
};

}  // namespace openscreen

#endif  // API_IMPL_INTERNAL_SERVICES_H_
