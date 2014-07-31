// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMIUM_SOCKET_FACTORY_H_
#define REMOTING_PROTOCOL_CHROMIUM_SOCKET_FACTORY_H_

#include "base/compiler_specific.h"
#include "third_party/libjingle/source/talk/p2p/base/packetsocketfactory.h"

namespace remoting {
namespace protocol {

class ChromiumPacketSocketFactory : public talk_base::PacketSocketFactory {
 public:
  explicit ChromiumPacketSocketFactory();
  virtual ~ChromiumPacketSocketFactory();

  virtual talk_base::AsyncPacketSocket* CreateUdpSocket(
      const talk_base::SocketAddress& local_address,
      int min_port, int max_port) OVERRIDE;
  virtual talk_base::AsyncPacketSocket* CreateServerTcpSocket(
      const talk_base::SocketAddress& local_address,
      int min_port, int max_port,
      int opts) OVERRIDE;
  virtual talk_base::AsyncPacketSocket* CreateClientTcpSocket(
      const talk_base::SocketAddress& local_address,
      const talk_base::SocketAddress& remote_address,
      const talk_base::ProxyInfo& proxy_info,
      const std::string& user_agent,
      int opts) OVERRIDE;
  virtual talk_base::AsyncResolverInterface* CreateAsyncResolver() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromiumPacketSocketFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHROMIUM_SOCKET_FACTORY_H_
