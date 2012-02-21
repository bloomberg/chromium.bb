// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_LIBJINGLE_TRANSPORT_FACTORY_H_
#define REMOTING_PROTOCOL_LIBJINGLE_TRANSPORT_FACTORY_H_

#include "remoting/protocol/transport.h"

namespace talk_base {
class NetworkManager;
class PacketSocketFactory;
}  // namespace talk_base

namespace remoting {
namespace protocol {

class LibjingleTransportFactory : public TransportFactory {
 public:
  LibjingleTransportFactory();
  virtual ~LibjingleTransportFactory();

  virtual scoped_ptr<StreamTransport> CreateStreamTransport() OVERRIDE;
  virtual scoped_ptr<DatagramTransport> CreateDatagramTransport() OVERRIDE;

 private:
  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(LibjingleTransportFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_LIBJINGLE_TRANSPORT_FACTORY_H_
