// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_PACKET_SOCKET_FACTORY_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_PACKET_SOCKET_FACTORY_H_

#include "base/compiler_specific.h"
#include "ppapi/cpp/instance_handle.h"
#include "third_party/libjingle/source/talk/p2p/base/packetsocketfactory.h"

namespace remoting {

class PepperPacketSocketFactory : public talk_base::PacketSocketFactory {
 public:
  explicit PepperPacketSocketFactory(const pp::InstanceHandle& instance);
  virtual ~PepperPacketSocketFactory();

  virtual talk_base::AsyncPacketSocket* CreateUdpSocket(
      const talk_base::SocketAddress& local_address,
      int min_port, int max_port) OVERRIDE;
  virtual talk_base::AsyncPacketSocket* CreateServerTcpSocket(
      const talk_base::SocketAddress& local_address,
      int min_port,
      int max_port,
      int opts) OVERRIDE;
  virtual talk_base::AsyncPacketSocket* CreateClientTcpSocket(
      const talk_base::SocketAddress& local_address,
      const talk_base::SocketAddress& remote_address,
      const talk_base::ProxyInfo& proxy_info,
      const std::string& user_agent,
      int opts) OVERRIDE;
  virtual talk_base::AsyncResolverInterface* CreateAsyncResolver() OVERRIDE;

 private:
  const pp::InstanceHandle pp_instance_;

  DISALLOW_COPY_AND_ASSIGN(PepperPacketSocketFactory);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_PACKET_SOCKET_FACTORY_H_
