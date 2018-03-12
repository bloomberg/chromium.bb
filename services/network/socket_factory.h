// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SOCKET_FACTORY_H_
#define SERVICES_NETWORK_SOCKET_FACTORY_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "net/socket/tcp_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/tcp_server_socket.h"

namespace net {
class NetLog;
}  // namespace net

namespace network {

class UDPSocket;
class TCPConnectedSocket;

// Helper class that handles UDPSocketRequest. It takes care of destroying the
// UDPSocket implementation instances when mojo pipes are broken.
class COMPONENT_EXPORT(NETWORK_SERVICE) SocketFactory
    : public TCPServerSocket::Delegate {
 public:
  // Constructs a SocketFactory. If |net_log| is non-null, it is used to
  // log NetLog events when logging is enabled. |net_log| used to must outlive
  // |this|.
  explicit SocketFactory(net::NetLog* net_log);
  virtual ~SocketFactory();

  void CreateUDPSocket(mojom::UDPSocketRequest request,
                       mojom::UDPSocketReceiverPtr receiver);
  void CreateTCPServerSocket(
      const net::IPEndPoint& local_addr,
      int backlog,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojom::TCPServerSocketRequest request,
      mojom::NetworkContext::CreateTCPServerSocketCallback callback);
  void CreateTCPConnectedSocket(
      const base::Optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojom::TCPConnectedSocketRequest request,
      mojom::TCPConnectedSocketObserverPtr observer,
      mojom::NetworkContext::CreateTCPConnectedSocketCallback callback);

 private:
  // TCPServerSocket::Delegate implementation:
  void OnAccept(std::unique_ptr<TCPConnectedSocket> socket,
                mojom::TCPConnectedSocketRequest request) override;

  void OnConnectCompleted(int result);

  net::NetLog* net_log_;
  mojo::StrongBindingSet<mojom::UDPSocket> udp_socket_bindings_;
  mojo::StrongBindingSet<mojom::TCPServerSocket> tcp_server_socket_bindings_;
  mojo::StrongBindingSet<mojom::TCPConnectedSocket>
      tcp_connected_socket_bindings_;

  DISALLOW_COPY_AND_ASSIGN(SocketFactory);
};

}  // namespace network

#endif  // SERVICES_NETWORK_SOCKET_FACTORY_H_
