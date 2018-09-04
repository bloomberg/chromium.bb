// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/tcp_bound_socket.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/socket/tcp_socket.h"
#include "services/network/socket_factory.h"
#include "services/network/tcp_connected_socket.h"

namespace network {

TCPBoundSocket::TCPBoundSocket(
    SocketFactory* socket_factory,
    net::NetLog* net_log,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : socket_factory_(socket_factory),
      socket_(std::make_unique<net::TCPSocket>(
          nullptr /*socket_performance_watcher*/,
          net_log,
          net::NetLogSource())),
      traffic_annotation_(traffic_annotation),
      weak_factory_(this) {}

TCPBoundSocket::~TCPBoundSocket() = default;

int TCPBoundSocket::Bind(const net::IPEndPoint& local_addr,
                         net::IPEndPoint* local_addr_out) {
  int result = socket_->Open(local_addr.GetFamily());
  if (result != net::OK)
    return result;

  // This is primarily intended for use with server sockets.
  result = socket_->SetDefaultOptionsForServer();
  if (result != net::OK)
    return result;

  result = socket_->Bind(local_addr);
  if (result != net::OK)
    return result;

  return socket_->GetLocalAddress(local_addr_out);
}

void TCPBoundSocket::Listen(uint32_t backlog,
                            mojom::TCPServerSocketRequest request,
                            ListenCallback callback) {
  DCHECK(socket_->IsValid());

  if (connect_callback_) {
    // Drop unexpected calls on the floor. Could destroy |this|, but as this is
    // currently only reachable from more trusted processes, doesn't seem too
    // useful.
    NOTREACHED();
    return;
  }

  int result = socket_->Listen(backlog);

  // Succeed or fail, pass the result back to the caller.
  std::move(callback).Run(result);

  // Tear down everything on error.
  if (result != net::OK) {
    socket_factory_->DestroyBoundSocket(binding_id_);
    return;
  }

  // On success, also set up the TCPServerSocket.
  std::unique_ptr<TCPServerSocket> server_socket =
      std::make_unique<TCPServerSocket>(
          std::make_unique<net::TCPServerSocket>(std::move(socket_)), backlog,
          socket_factory_, traffic_annotation_);
  socket_factory_->OnBoundSocketListening(binding_id_, std::move(server_socket),
                                          std::move(request));
  // The above call will have destroyed |this|.
}

void TCPBoundSocket::Connect(const net::IPEndPoint& remote_addr,
                             mojom::TCPConnectedSocketRequest request,
                             mojom::SocketObserverPtr observer,
                             ConnectCallback callback) {
  DCHECK(socket_->IsValid());

  if (connect_callback_) {
    // Drop unexpected calls on the floor. Could destroy |this|, but as this is
    // currently only reachable from more trusted processes, doesn't seem too
    // useful.
    NOTREACHED();
    return;
  }

  connected_socket_request_ = std::move(request);
  socket_observer_ = std::move(observer);
  connect_callback_ = std::move(callback);
  int result = socket_->Connect(
      remote_addr, base::BindOnce(&TCPBoundSocket::OnConnectComplete,
                                  base::Unretained(this)));

  if (result == net::ERR_IO_PENDING)
    return;

  OnConnectComplete(result);
}

void TCPBoundSocket::OnConnectComplete(int result) {
  DCHECK(connect_callback_);

  net::IPEndPoint peer_addr;
  net::IPEndPoint local_addr;
  if (result == net::OK)
    result = socket_->GetLocalAddress(&local_addr);
  if (result == net::OK)
    result = socket_->GetPeerAddress(&peer_addr);

  if (result != net::OK) {
    std::move(connect_callback_)
        .Run(result, base::nullopt /* local_addr */,
             base::nullopt /* peer_addr */,
             mojo::ScopedDataPipeConsumerHandle(),
             mojo::ScopedDataPipeProducerHandle());
    socket_factory_->DestroyBoundSocket(binding_id_);
    return;
  }

  mojo::DataPipe send_pipe;
  mojo::DataPipe receive_pipe;
  std::unique_ptr<TCPConnectedSocket> connected_socket =
      std::make_unique<TCPConnectedSocket>(
          std::move(socket_observer_),
          std::make_unique<net::TCPClientSocket>(std::move(socket_), peer_addr),
          std::move(receive_pipe.producer_handle),
          std::move(send_pipe.consumer_handle), traffic_annotation_);
  std::move(connect_callback_)
      .Run(result, local_addr, peer_addr,
           std::move(receive_pipe.consumer_handle),
           std::move(send_pipe.producer_handle));
  socket_factory_->OnBoundSocketConnected(binding_id_,
                                          std::move(connected_socket),
                                          std::move(connected_socket_request_));
  // The above call will have destroyed |this|.
}

}  // namespace network
