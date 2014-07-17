// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/session_manager_factory.h"

#include "remoting/protocol/chromium_port_allocator.h"
#include "remoting/protocol/chromium_socket_factory.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/libjingle_transport_factory.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/session_manager.h"

namespace remoting {

scoped_ptr<protocol::SessionManager> CreateHostSessionManager(
    SignalStrategy* signal_strategy,
    const protocol::NetworkSettings& network_settings,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter) {
  // Use Chrome's network stack to allocate ports for peer-to-peer channels.
  scoped_ptr<protocol::ChromiumPortAllocator> port_allocator(
      protocol::ChromiumPortAllocator::Create(url_request_context_getter,
                                              network_settings));

  scoped_ptr<protocol::TransportFactory> transport_factory(
      new protocol::LibjingleTransportFactory(
          signal_strategy,
          port_allocator.PassAs<cricket::HttpPortAllocatorBase>(),
          network_settings));

  scoped_ptr<protocol::JingleSessionManager> session_manager(
      new protocol::JingleSessionManager(transport_factory.Pass()));
  return session_manager.PassAs<protocol::SessionManager>();
}

} // namespace remoting
