// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FAKE_PORT_ALLOCATOR_H_
#define REMOTING_TEST_FAKE_PORT_ALLOCATOR_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"

namespace remoting {

class FakeNetworkDispatcher;
class FakePacketSocketFactory;

class FakePortAllocator : public cricket::HttpPortAllocatorBase {
 public:
  static scoped_ptr<FakePortAllocator> Create(
      scoped_refptr<FakeNetworkDispatcher> fake_network_dispatcher);

  virtual ~FakePortAllocator();

  FakePacketSocketFactory* socket_factory() { return socket_factory_.get(); }

  // cricket::BasicPortAllocator overrides.
  virtual cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password) OVERRIDE;

 private:
  FakePortAllocator(scoped_ptr<rtc::NetworkManager> network_manager,
                    scoped_ptr<FakePacketSocketFactory> socket_factory);

  scoped_ptr<rtc::NetworkManager> network_manager_;
  scoped_ptr<FakePacketSocketFactory> socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakePortAllocator);
};

}  // namespace remoting

#endif  // REMOTING_TEST_FAKE_PORT_ALLOCATOR_H_
