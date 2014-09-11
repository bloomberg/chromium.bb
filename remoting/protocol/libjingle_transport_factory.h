// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_LIBJINGLE_TRANSPORT_FACTORY_H_
#define REMOTING_PROTOCOL_LIBJINGLE_TRANSPORT_FACTORY_H_

#include <list>

#include "base/callback_forward.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport.h"

namespace cricket {
class HttpPortAllocatorBase;
class PortAllocator;
}  // namespace cricket

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace rtc {
class NetworkManager;
class PacketSocketFactory;
class SocketAddress;
}  // namespace rtc

namespace remoting {

class SignalStrategy;
class JingleInfoRequest;

namespace protocol {

class LibjingleTransportFactory : public TransportFactory {
 public:
  // |signal_strategy| must outlive LibjingleTransportFactory. Need to use
  // cricket::HttpPortAllocatorBase pointer for the |port_allocator|, so that it
  // is possible to configure |port_allocator| with STUN/Relay addresses.
  LibjingleTransportFactory(
      SignalStrategy* signal_strategy,
      scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator,
      const NetworkSettings& network_settings);

  virtual ~LibjingleTransportFactory();

  // TransportFactory interface.
  virtual void PrepareTokens() OVERRIDE;
  virtual scoped_ptr<StreamTransport> CreateStreamTransport() OVERRIDE;
  virtual scoped_ptr<DatagramTransport> CreateDatagramTransport() OVERRIDE;

 private:
  void EnsureFreshJingleInfo();
  void OnJingleInfo(const std::string& relay_token,
                    const std::vector<std::string>& relay_hosts,
                    const std::vector<rtc::SocketAddress>& stun_hosts);

  SignalStrategy* signal_strategy_;
  scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator_;
  NetworkSettings network_settings_;

  base::TimeTicks last_jingle_info_update_time_;
  scoped_ptr<JingleInfoRequest> jingle_info_request_;

  // When there is an active |jingle_info_request_| stores list of callbacks to
  // be called once the |jingle_info_request_| is finished.
  std::list<base::Closure> on_jingle_info_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(LibjingleTransportFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_LIBJINGLE_TRANSPORT_FACTORY_H_
