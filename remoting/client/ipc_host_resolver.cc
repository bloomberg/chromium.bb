// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ipc_host_resolver.h"

#include "base/bind.h"
#include "content/renderer/p2p/host_address_request.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "net/base/ip_endpoint.h"
#include "jingle/glue/utils.h"

namespace remoting {

class IpcHostResolver : public HostResolver {
 public:
  IpcHostResolver(content::P2PSocketDispatcher* socket_dispatcher)
      : socket_dispatcher_(socket_dispatcher) {
  }

  virtual ~IpcHostResolver() {
    if (request_)
      request_->Cancel();
  }

  virtual void Resolve(const talk_base::SocketAddress& address) OVERRIDE {
    if (address.IsUnresolved()) {
      port_ = address.port();
      request_ = new content::P2PHostAddressRequest(socket_dispatcher_);
      request_->Request(address.hostname(), base::Bind(
          &IpcHostResolver::OnDone, base::Unretained(this)));
    } else {
      SignalDone(this, address);
    }
  }

 private:
  void OnDone(const net::IPAddressNumber& address) {
    talk_base::SocketAddress socket_address;
    if (address.empty() ||
        !jingle_glue::IPEndPointToSocketAddress(
            net::IPEndPoint(address, port_), &socket_address)) {
      // Return nil address if the request has failed.
      SignalDone(this, talk_base::SocketAddress());
      return;
    }

    request_ = NULL;
    SignalDone(this, socket_address);
  }

  content::P2PSocketDispatcher* socket_dispatcher_;
  scoped_refptr<content::P2PHostAddressRequest> request_;
  uint16 port_;
};

IpcHostResolverFactory::IpcHostResolverFactory(
    content::P2PSocketDispatcher* socket_dispatcher)
    : socket_dispatcher_(socket_dispatcher) {
}

IpcHostResolverFactory::~IpcHostResolverFactory() {
}

HostResolver* IpcHostResolverFactory::CreateHostResolver() {
  return new IpcHostResolver(socket_dispatcher_);
}

}  // namespace remoting
