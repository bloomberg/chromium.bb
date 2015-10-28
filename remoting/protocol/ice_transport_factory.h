// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ICE_TRANSPORT_FACTORY_H_
#define REMOTING_PROTOCOL_ICE_TRANSPORT_FACTORY_H_

#include "base/macros.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport.h"

namespace cricket {
class HttpPortAllocatorBase;
}  // namespace cricket

namespace remoting {

class SignalStrategy;

namespace protocol {

class LibjingleTransportFactory;

class IceTransportFactory : public TransportFactory {
 public:
  IceTransportFactory(
      SignalStrategy* signal_strategy,
      scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator,
      const NetworkSettings& network_settings,
      TransportRole role);
  ~IceTransportFactory() override;

  // TransportFactory interface.
  void PrepareTokens() override;
  scoped_ptr<TransportSession> CreateTransportSession() override;

 private:
  scoped_ptr<LibjingleTransportFactory> libjingle_transport_factory_;

  DISALLOW_COPY_AND_ASSIGN(IceTransportFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_ICE_TRANSPORT_FACTORY_H_
