// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_resolving_socket_mojo.h"

#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "net/base/net_errors.h"
#include "services/network/socket_data_pump.h"

namespace network {

ProxyResolvingSocketMojo::ProxyResolvingSocketMojo(
    std::unique_ptr<ProxyResolvingClientSocket> socket,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : socket_(std::move(socket)), traffic_annotation_(traffic_annotation) {}

ProxyResolvingSocketMojo::~ProxyResolvingSocketMojo() {
  if (connect_callback_) {
    // If |this| is destroyed when connect hasn't completed, tell the consumer
    // that request has been aborted.
    std::move(connect_callback_)
        .Run(net::ERR_ABORTED, base::nullopt, base::nullopt,
             mojo::ScopedDataPipeConsumerHandle(),
             mojo::ScopedDataPipeProducerHandle());
  }
}

void ProxyResolvingSocketMojo::Connect(
    mojom::ProxyResolvingSocketFactory::CreateProxyResolvingSocketCallback
        callback) {
  DCHECK(socket_);
  DCHECK(callback);
  DCHECK(!connect_callback_);

  connect_callback_ = std::move(callback);
  int result = socket_->Connect(base::BindOnce(
      &ProxyResolvingSocketMojo::OnConnectCompleted, base::Unretained(this)));
  if (result == net::ERR_IO_PENDING)
    return;
  OnConnectCompleted(result);
}

void ProxyResolvingSocketMojo::OnConnectCompleted(int result) {
  DCHECK(!connect_callback_.is_null());
  DCHECK(!socket_data_pump_);

  net::IPEndPoint local_addr;
  if (result == net::OK)
    result = socket_->GetLocalAddress(&local_addr);

  net::IPEndPoint peer_addr;
  // If |socket_| is connected through a proxy, GetPeerAddress returns
  // net::ERR_NAME_NOT_RESOLVED.
  bool get_peer_address_success =
      result == net::OK && (socket_->GetPeerAddress(&peer_addr) == net::OK);

  if (result != net::OK) {
    std::move(connect_callback_)
        .Run(result, base::nullopt, base::nullopt,
             mojo::ScopedDataPipeConsumerHandle(),
             mojo::ScopedDataPipeProducerHandle());
    return;
  }
  mojo::DataPipe send_pipe;
  mojo::DataPipe receive_pipe;
  socket_data_pump_ = std::make_unique<SocketDataPump>(
      socket_.get(), nullptr /*delegate*/,
      std::move(receive_pipe.producer_handle),
      std::move(send_pipe.consumer_handle), traffic_annotation_);
  std::move(connect_callback_)
      .Run(net::OK, local_addr,
           get_peer_address_success
               ? base::make_optional<net::IPEndPoint>(peer_addr)
               : base::nullopt,
           std::move(receive_pipe.consumer_handle),
           std::move(send_pipe.producer_handle));
}

}  // namespace network
