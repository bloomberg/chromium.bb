// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/session_manager_factory.h"

#include "remoting/jingle_glue/chromium_port_allocator.h"
#include "remoting/jingle_glue/network_settings.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/libjingle_transport_factory.h"
#include "remoting/protocol/session_manager.h"

namespace remoting {

scoped_ptr<protocol::SessionManager> CreateHostSessionManager(
    const NetworkSettings& network_settings,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter) {
  scoped_ptr<protocol::TransportFactory> transport_factory(
      protocol::LibjingleTransportFactory::Create(network_settings,
                                                  url_request_context_getter));

  // Use the Jingle protocol for channel-negotiation signalling between
  // peer TransportFactories.
  bool fetch_stun_relay_info = network_settings.nat_traversal_mode ==
      NetworkSettings::NAT_TRAVERSAL_ENABLED;

  scoped_ptr<protocol::JingleSessionManager> session_manager(
      new protocol::JingleSessionManager(
          transport_factory.Pass(), fetch_stun_relay_info));
  return session_manager.PassAs<protocol::SessionManager>();
}

} // namespace remoting
