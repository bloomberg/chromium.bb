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

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace talk_base {
class NetworkManager;
class PacketSocketFactory;
}  // namespace talk_base

namespace remoting {

struct NetworkSettings;

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

  virtual ~LibjingleTransportFactory();

  // TransportFactory interface.
  virtual void SetTransportConfig(const TransportConfig& config) OVERRIDE;
  virtual scoped_ptr<StreamTransport> CreateStreamTransport() OVERRIDE;
  virtual scoped_ptr<DatagramTransport> CreateDatagramTransport() OVERRIDE;

 private:
  scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator_;
  bool incoming_only_;

  DISALLOW_COPY_AND_ASSIGN(LibjingleTransportFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_LIBJINGLE_TRANSPORT_FACTORY_H_
