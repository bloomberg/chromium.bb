// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_H_
#define REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_H_

#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/port_allocator_base.h"
#include "remoting/protocol/port_allocator_factory.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {
namespace protocol {

struct NetworkSettings;

// An implementation of cricket::PortAllocator that uses Chromium's network
// stack.
class ChromiumPortAllocator : public PortAllocatorBase {
 public:
  ChromiumPortAllocator(
      scoped_ptr<rtc::NetworkManager> network_manager,
      scoped_ptr<rtc::PacketSocketFactory> socket_factory,
      scoped_refptr<TransportContext> transport_context,
      scoped_refptr<net::URLRequestContextGetter> url_context);
  ~ChromiumPortAllocator() override;

  scoped_refptr<net::URLRequestContextGetter> url_context() {
    return url_context_;
  }

  // PortAllocatorBase overrides.
  cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password) override;

 private:
  scoped_refptr<net::URLRequestContextGetter> url_context_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumPortAllocator);
};

class ChromiumPortAllocatorFactory : public PortAllocatorFactory {
 public:
  ChromiumPortAllocatorFactory(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);
  ~ChromiumPortAllocatorFactory() override;

   // PortAllocatorFactory interface.
  scoped_ptr<cricket::PortAllocator> CreatePortAllocator(
      scoped_refptr<TransportContext> transport_context) override;

 private:
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumPortAllocatorFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_HOST_HOST_PORT_ALLOCATOR_H_
