// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file overrides HttpPortAllocator defined in
// third_party/libjingle/talk/p2p/client/httpportallocator.h to inject a
// custom HTTP client.

#ifndef REMOTING_JINGLE_GLUE_HTTP_PORT_ALLOCATOR_H_
#define REMOTING_JINGLE_GLUE_HTTP_PORT_ALLOCATOR_H_

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"

namespace remoting {

// A factory class to generate cricket::PortAllocatorSession.
class PortAllocatorSessionFactory {
 public:
  PortAllocatorSessionFactory() {
  }

  virtual ~PortAllocatorSessionFactory();

  virtual cricket::PortAllocatorSession* CreateSession(
      cricket::BasicPortAllocator* allocator,
      const std::string& name,
      const std::string& session_type,
      const std::vector<talk_base::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay,
      const std::string& agent) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PortAllocatorSessionFactory);
};

class HttpPortAllocator : public cricket::HttpPortAllocator {
 public:
  HttpPortAllocator(talk_base::NetworkManager* network_manager,
                    talk_base::PacketSocketFactory* socket_factory,
                    PortAllocatorSessionFactory* session_factory,
                    const std::string& user_agent);
  virtual ~HttpPortAllocator();

  // Override CreateSession() to inject a custom HTTP session.
  virtual cricket::PortAllocatorSession* CreateSession(
      const std::string& name, const std::string& session_type);

 private:
  PortAllocatorSessionFactory* session_factory_;

  DISALLOW_COPY_AND_ASSIGN(HttpPortAllocator);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_HTTP_PORT_ALLOCATOR_H_
