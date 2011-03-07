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
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"

namespace remoting {

class HttpPortAllocator : public cricket::HttpPortAllocator {
 public:
  HttpPortAllocator(talk_base::NetworkManager* network_manager,
                    talk_base::PacketSocketFactory* socket_factory,
                    const std::string& user_agent);
  virtual ~HttpPortAllocator();

  // Override CreateSession() to inject a custom HTTP session.
  virtual cricket::PortAllocatorSession* CreateSession(
      const std::string& name, const std::string& session_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpPortAllocator);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_HTTP_PORT_ALLOCATOR_H_
