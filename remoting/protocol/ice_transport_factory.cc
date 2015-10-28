// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_transport_factory.h"

#include "remoting/protocol/ice_transport_session.h"
#include "remoting/protocol/libjingle_transport_factory.h"
#include "third_party/webrtc/p2p/client/httpportallocator.h"

namespace remoting {
namespace protocol {

IceTransportFactory::IceTransportFactory(
    SignalStrategy* signal_strategy,
    scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator,
    const NetworkSettings& network_settings,
    TransportRole role)
    : libjingle_transport_factory_(
          new LibjingleTransportFactory(signal_strategy,
                                        port_allocator.Pass(),
                                        network_settings,
                                        role)) {}
IceTransportFactory::~IceTransportFactory() {}

void IceTransportFactory::PrepareTokens() {
  libjingle_transport_factory_->PrepareTokens();
}

scoped_ptr<TransportSession>
IceTransportFactory::CreateTransportSession() {
  return make_scoped_ptr(
      new IceTransportSession(libjingle_transport_factory_.get()));
}

}  // namespace protocol
}  // namespace remoting
