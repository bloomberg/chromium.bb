// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_UDP_SOCKET_FACTORY_H_
#define SERVICES_NETWORK_UDP_SOCKET_FACTORY_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "services/network/public/interfaces/udp_socket.mojom.h"

namespace network {

class UDPSocket;

// Helper class that handles UDPSocketRequest. It takes care of destroying the
// UDPSocket implementation instances when mojo pipes are broken.
class COMPONENT_EXPORT(NETWORK_SERVICE) UDPSocketFactory {
 public:
  UDPSocketFactory();
  virtual ~UDPSocketFactory();

  void CreateUDPSocket(mojom::UDPSocketRequest request,
                       mojom::UDPSocketReceiverPtr receiver);

 protected:
  // Handles connection errors. This is virtual for testing.
  virtual void OnPipeBroken(UDPSocket* client);

 private:
  std::vector<std::unique_ptr<UDPSocket>> udp_sockets_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocketFactory);
};

}  // namespace network

#endif  // SERVICES_NETWORK_UDP_SOCKET_FACTORY_H_
