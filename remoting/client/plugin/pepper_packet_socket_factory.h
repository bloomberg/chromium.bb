// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_PACKET_SOCKET_FACTORY_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_PACKET_SOCKET_FACTORY_H_

#include "base/compiler_specific.h"
#include "ppapi/cpp/instance_handle.h"
#include "third_party/libjingle/source/talk/p2p/base/packetsocketfactory.h"

namespace remoting {

class PepperPacketSocketFactory : public rtc::PacketSocketFactory {
 public:
  explicit PepperPacketSocketFactory(const pp::InstanceHandle& instance);
  virtual ~PepperPacketSocketFactory();

  virtual rtc::AsyncPacketSocket* CreateUdpSocket(
      const rtc::SocketAddress& local_address,
      int min_port, int max_port) OVERRIDE;
  virtual rtc::AsyncPacketSocket* CreateServerTcpSocket(
      const rtc::SocketAddress& local_address,
      int min_port,
      int max_port,
      int opts) OVERRIDE;
  virtual rtc::AsyncPacketSocket* CreateClientTcpSocket(
      const rtc::SocketAddress& local_address,
      const rtc::SocketAddress& remote_address,
      const rtc::ProxyInfo& proxy_info,
      const std::string& user_agent,
      int opts) OVERRIDE;
  virtual rtc::AsyncResolverInterface* CreateAsyncResolver() OVERRIDE;

 private:
  const pp::InstanceHandle pp_instance_;

  DISALLOW_COPY_AND_ASSIGN(PepperPacketSocketFactory);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_PACKET_SOCKET_FACTORY_H_
