// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_H_
#define REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_H_

#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/port_allocator_factory.h"
#include "third_party/webrtc/p2p/client/httpportallocator.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {
namespace protocol {

struct NetworkSettings;

// An implementation of cricket::PortAllocator that uses Chromium's network
// stack.
class ChromiumPortAllocator : public cricket::HttpPortAllocatorBase {
 public:
  static scoped_ptr<ChromiumPortAllocator> Create(
      const scoped_refptr<net::URLRequestContextGetter>& url_context);

  ~ChromiumPortAllocator() override;

  // cricket::HttpPortAllocatorBase overrides.
  cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password) override;

 private:
  ChromiumPortAllocator(
      const scoped_refptr<net::URLRequestContextGetter>& url_context,
      scoped_ptr<rtc::NetworkManager> network_manager,
      scoped_ptr<rtc::PacketSocketFactory> socket_factory);

  scoped_refptr<net::URLRequestContextGetter> url_context_;
  scoped_ptr<rtc::NetworkManager> network_manager_;
  scoped_ptr<rtc::PacketSocketFactory> socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumPortAllocator);
};

class ChromiumPortAllocatorFactory : public PortAllocatorFactory {
 public:
  ChromiumPortAllocatorFactory(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);
  ~ChromiumPortAllocatorFactory() override;

   // PortAllocatorFactory interface.
  scoped_ptr<cricket::HttpPortAllocatorBase> CreatePortAllocator() override;

 private:
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumPortAllocatorFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_HOST_HOST_PORT_ALLOCATOR_H_
