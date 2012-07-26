// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_LIBJINGLE_TRANSPORT_FACTORY_H_
#define REMOTING_PROTOCOL_LIBJINGLE_TRANSPORT_FACTORY_H_

#include "remoting/protocol/transport.h"

namespace cricket {
class HttpPortAllocatorBase;
class PortAllocator;
}  // namespace cricket

namespace talk_base {
class NetworkManager;
class PacketSocketFactory;
}  // namespace talk_base

namespace remoting {
namespace protocol {

class LibjingleTransportFactory : public TransportFactory {
 public:
  // Need to use cricket::HttpPortAllocatorBase pointer for the
  // |port_allocator|, so that it is possible to configure
  // |port_allocator| with STUN/Relay addresses.
  // TODO(sergeyu): Reconsider this design.
  LibjingleTransportFactory(
      scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator,
      bool incoming_only);

  // Creates BasicNetworkManager, BasicPacketSocketFactory and
  // BasicPortAllocator.
  LibjingleTransportFactory();

  virtual ~LibjingleTransportFactory();

  // TransportFactory interface.
  virtual void SetTransportConfig(const TransportConfig& config) OVERRIDE;
  virtual scoped_ptr<StreamTransport> CreateStreamTransport() OVERRIDE;
  virtual scoped_ptr<DatagramTransport> CreateDatagramTransport() OVERRIDE;

 private:
  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory_;
  // Points to the same port allocator as |port_allocator_| or NULL if
  // |port_allocator_| is not HttpPortAllocatorBase.
  cricket::HttpPortAllocatorBase* http_port_allocator_;
  scoped_ptr<cricket::PortAllocator> port_allocator_;
  bool incoming_only_;

  DISALLOW_COPY_AND_ASSIGN(LibjingleTransportFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_LIBJINGLE_TRANSPORT_FACTORY_H_
