// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/socket_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "services/network/tcp_connected_socket.h"
#include "services/network/udp_socket.h"

namespace network {

SocketFactory::SocketFactory(net::NetLog* net_log) : net_log_(net_log) {}

SocketFactory::~SocketFactory() {}

void SocketFactory::CreateUDPSocket(mojom::UDPSocketRequest request,
                                    mojom::UDPSocketReceiverPtr receiver) {
  udp_socket_bindings_.AddBinding(
      std::make_unique<UDPSocket>(std::move(receiver), net_log_),
      std::move(request));
}

void SocketFactory::CreateTCPServerSocket(
    const net::IPEndPoint& local_addr,
    int backlog,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojom::TCPServerSocketRequest request,
    mojom::NetworkContext::CreateTCPServerSocketCallback callback) {
  auto socket =
      std::make_unique<TCPServerSocket>(this, net_log_, traffic_annotation);
  net::IPEndPoint local_addr_out;
  int result = socket->Listen(local_addr, backlog, &local_addr_out);
  if (result != net::OK) {
    std::move(callback).Run(result, base::nullopt);
    return;
  }
  tcp_server_socket_bindings_.AddBinding(std::move(socket), std::move(request));
  std::move(callback).Run(result, local_addr_out);
}

void SocketFactory::CreateTCPConnectedSocket(
    const base::Optional<net::IPEndPoint>& local_addr,
    const net::AddressList& remote_addr_list,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojom::TCPConnectedSocketRequest request,
    mojom::TCPConnectedSocketObserverPtr observer,
    mojom::NetworkContext::CreateTCPConnectedSocketCallback callback) {
  auto socket = std::make_unique<TCPConnectedSocket>(
      std::move(observer), net_log_, traffic_annotation);
  TCPConnectedSocket* socket_raw = socket.get();
  tcp_connected_socket_bindings_.AddBinding(std::move(socket),
                                            std::move(request));
  socket_raw->Connect(local_addr, remote_addr_list, std::move(callback));
}

void SocketFactory::OnAccept(std::unique_ptr<TCPConnectedSocket> socket,
                             mojom::TCPConnectedSocketRequest request) {
  tcp_connected_socket_bindings_.AddBinding(std::move(socket),
                                            std::move(request));
}

}  // namespace network
