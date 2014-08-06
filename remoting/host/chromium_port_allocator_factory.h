// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_
#define REMOTING_HOST_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {

namespace protocol {
struct NetworkSettings;
}  // namespace protocol

class ChromiumPortAllocatorFactory
    : public webrtc::PortAllocatorFactoryInterface {
 public:
  static rtc::scoped_refptr<webrtc::PortAllocatorFactoryInterface> Create(
      const protocol::NetworkSettings& network_settings,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

  // webrtc::PortAllocatorFactoryInterface implementation.
  virtual cricket::PortAllocator* CreatePortAllocator(
      const std::vector<StunConfiguration>& stun_servers,
      const std::vector<TurnConfiguration>& turn_configurations) OVERRIDE;

 protected:
  ChromiumPortAllocatorFactory(
      const protocol::NetworkSettings& network_settings,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);
  virtual ~ChromiumPortAllocatorFactory();

 private:
  const protocol::NetworkSettings& network_settings_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumPortAllocatorFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_

