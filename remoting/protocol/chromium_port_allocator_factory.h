// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_
#define REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "remoting/protocol/network_settings.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {
namespace protocol {

class ChromiumPortAllocatorFactory
    : public webrtc::PortAllocatorFactoryInterface {
 public:
  static rtc::scoped_refptr<webrtc::PortAllocatorFactoryInterface> Create(
      const NetworkSettings& network_settings,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

  // webrtc::PortAllocatorFactoryInterface implementation.
  cricket::PortAllocator* CreatePortAllocator(
      const std::vector<StunConfiguration>& stun_servers,
      const std::vector<TurnConfiguration>& turn_configurations) override;

 protected:
  ChromiumPortAllocatorFactory(
      const NetworkSettings& network_settings,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);
  ~ChromiumPortAllocatorFactory() override;

 private:
  NetworkSettings network_settings_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumPortAllocatorFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_

