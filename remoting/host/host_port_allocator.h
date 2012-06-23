// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_PORT_ALLOCATOR_H_
#define REMOTING_HOST_HOST_PORT_ALLOCATOR_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {

struct NetworkSettings;

// An implementation of cricket::PortAllocator for libjingle that is
// used by the remoting host. The main difference from
// cricket::HttpPortAllocator is that it uses Chromium's HTTP stack
// when creating relay sessions. It also configures itself according
// to the specified NetworkSettings.
class HostPortAllocator : public cricket::HttpPortAllocatorBase {
 public:
  static scoped_ptr<HostPortAllocator> Create(
      const scoped_refptr<net::URLRequestContextGetter>& url_context,
      const NetworkSettings& network_settings);

  virtual ~HostPortAllocator();

  // cricket::HttpPortAllocatorBase overrides.
  virtual cricket::PortAllocatorSession* CreateSessionInternal(
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password) OVERRIDE;

 private:
  HostPortAllocator(
      const scoped_refptr<net::URLRequestContextGetter>& url_context,
      scoped_ptr<talk_base::NetworkManager> network_manager,
      scoped_ptr<talk_base::PacketSocketFactory> socket_factory);

  scoped_refptr<net::URLRequestContextGetter> url_context_;
  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostPortAllocator);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_PORT_ALLOCATOR_H_
