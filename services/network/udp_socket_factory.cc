// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/udp_socket_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "net/base/net_errors.h"
#include "services/network/udp_socket.h"

namespace network {

UDPSocketFactory::UDPSocketFactory() {}

UDPSocketFactory::~UDPSocketFactory() {}

void UDPSocketFactory::CreateUDPSocket(mojom::UDPSocketRequest request,
                                       mojom::UDPSocketReceiverPtr receiver) {
  auto socket =
      std::make_unique<UDPSocket>(std::move(request), std::move(receiver));
  // base::Unretained is safe as the destruction of |this| will also destroy
  // |udp_sockets_| which owns this socket.
  socket->set_connection_error_handler(
      base::BindOnce(&UDPSocketFactory::OnPipeBroken, base::Unretained(this),
                     base::Unretained(socket.get())));
  udp_sockets_.push_back(std::move(socket));
}

void UDPSocketFactory::OnPipeBroken(UDPSocket* socket) {
  udp_sockets_.erase(
      std::find_if(udp_sockets_.begin(), udp_sockets_.end(),
                   [socket](const std::unique_ptr<network::UDPSocket>& ptr) {
                     return ptr.get() == socket;
                   }));
}

}  // namespace network
