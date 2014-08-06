// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromium_port_allocator_factory.h"

#include "base/logging.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/chromium_port_allocator.h"
#include "remoting/protocol/network_settings.h"

namespace remoting {

ChromiumPortAllocatorFactory::ChromiumPortAllocatorFactory(
    const protocol::NetworkSettings& network_settings,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : network_settings_(network_settings),
      url_request_context_getter_(url_request_context_getter) {
}

ChromiumPortAllocatorFactory::~ChromiumPortAllocatorFactory() {}

rtc::scoped_refptr<webrtc::PortAllocatorFactoryInterface>
ChromiumPortAllocatorFactory::Create(
    const protocol::NetworkSettings& network_settings,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
  rtc::RefCountedObject<ChromiumPortAllocatorFactory>* allocator_factory =
      new rtc::RefCountedObject<ChromiumPortAllocatorFactory>(
          network_settings, url_request_context_getter);
  return allocator_factory;
}

cricket::PortAllocator* ChromiumPortAllocatorFactory::CreatePortAllocator(
    const std::vector<StunConfiguration>& stun_servers,
    const std::vector<TurnConfiguration>& turn_configurations) {
  scoped_ptr<protocol::ChromiumPortAllocator> port_allocator(
      protocol::ChromiumPortAllocator::Create(url_request_context_getter_,
                                    network_settings_));

  std::vector<rtc::SocketAddress> stun_hosts;
  typedef std::vector<StunConfiguration>::const_iterator StunIt;
  for (StunIt stun_it = stun_servers.begin(); stun_it != stun_servers.end();
       ++stun_it) {
    stun_hosts.push_back(stun_it->server);
  }
  port_allocator->SetStunHosts(stun_hosts);

  // TODO(aiguha): Figure out how to translate |turn_configurations| into
  // turn hosts so we can set |port_allocator|'s relay hosts.

  return port_allocator.release();
}

}  // namespace remoting

