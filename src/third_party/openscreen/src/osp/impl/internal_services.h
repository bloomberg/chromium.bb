// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_INTERNAL_SERVICES_H_
#define OSP_IMPL_INTERNAL_SERVICES_H_

#include <memory>
#include <vector>

#include "osp/impl/mdns_platform_service.h"
#include "osp/impl/mdns_responder_service.h"
#include "osp/impl/quic/quic_connection_factory.h"
#include "osp/impl/service_listener_impl.h"
#include "osp/impl/service_publisher_impl.h"
#include "osp/public/mdns_service_listener_factory.h"
#include "osp/public/mdns_service_publisher_factory.h"
#include "osp/public/protocol_connection_client.h"
#include "osp/public/protocol_connection_server.h"
#include "platform/api/network_interface.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/base/ip_address.h"
#include "platform/base/macros.h"

namespace openscreen {

class TaskRunner;

namespace osp {

// Factory for ServiceListener and ServicePublisher instances; owns internal
// objects needed to instantiate them such as MdnsResponderService and runs an
// event loop.
// TODO(btolsch): This may be renamed and/or split up once QUIC code lands and
// this use case is more concrete.
class InternalServices : UdpSocket::Client {
 public:
  static std::unique_ptr<ServiceListener> CreateListener(
      const MdnsServiceListenerConfig& config,
      ServiceListener::Observer* observer,
      TaskRunner* task_runner);
  static std::unique_ptr<ServicePublisher> CreatePublisher(
      const ServicePublisher::Config& config,
      ServicePublisher::Observer* observer,
      TaskRunner* task_runner);

  // UdpSocket::Client overrides.
  void OnError(UdpSocket* socket, Error error) override;
  void OnSendError(UdpSocket* socket, Error error) override;
  void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) override;

 private:
  class InternalPlatformLinkage final : public MdnsPlatformService {
   public:
    explicit InternalPlatformLinkage(InternalServices* parent);
    ~InternalPlatformLinkage() override;

    std::vector<BoundInterface> RegisterInterfaces(
        const std::vector<NetworkInterfaceIndex>& whitelist) override;
    void DeregisterInterfaces(
        const std::vector<BoundInterface>& registered_interfaces) override;

   private:
    InternalServices* const parent_;
    std::vector<std::unique_ptr<UdpSocket>> open_sockets_;
  };

  // The TaskRunner provided here should live for the duration of this
  // InternalService object's lifetime.
  InternalServices(ClockNowFunctionPtr now_function, TaskRunner* task_runner);
  ~InternalServices() override;

  void RegisterMdnsSocket(UdpSocket* socket);
  void DeregisterMdnsSocket(UdpSocket* socket);

  static InternalServices* ReferenceSingleton(TaskRunner* task_runner);
  static void DereferenceSingleton(void* instance);

  MdnsResponderService mdns_service_;

  TaskRunner* const task_runner_;

  OSP_DISALLOW_COPY_AND_ASSIGN(InternalServices);
};

}  // namespace osp
}  // namespace openscreen

#endif  // OSP_IMPL_INTERNAL_SERVICES_H_
