// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/http_port_allocator.h"

namespace remoting {

PortAllocatorSessionFactory::~PortAllocatorSessionFactory() {
}

HttpPortAllocator::HttpPortAllocator(
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory,
    PortAllocatorSessionFactory* session_factory,
    const std::string &user_agent)
    : cricket::HttpPortAllocator(network_manager,
                                 socket_factory,
                                 user_agent),
      session_factory_(session_factory) {
}

HttpPortAllocator::~HttpPortAllocator() {
}

cricket::PortAllocatorSession *HttpPortAllocator::CreateSession(
    const std::string& name, const std::string& session_type) {
  if (session_factory_) {
    return session_factory_->CreateSession(
        this, name, session_type, stun_hosts(), relay_hosts(), relay_token(),
        user_agent());
  }
  return new cricket::HttpPortAllocatorSession(
      this, name, session_type, stun_hosts(), relay_hosts(), relay_token(),
      user_agent());
}

}  // namespace remoting
