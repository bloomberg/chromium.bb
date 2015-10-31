// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ICE_TRANSPORT_FACTORY_H_
#define REMOTING_PROTOCOL_ICE_TRANSPORT_FACTORY_H_

#include <list>

#include "base/macros.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/jingle_info_request.h"

namespace cricket {
class HttpPortAllocatorBase;
}  // namespace cricket

namespace remoting {

class SignalStrategy;

namespace protocol {

class IceTransportFactory : public TransportFactory {
 public:
  IceTransportFactory(
      SignalStrategy* signal_strategy,
      scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator,
      const NetworkSettings& network_settings,
      TransportRole role);
  ~IceTransportFactory() override;

  // TransportFactory interface.
  scoped_ptr<Transport> CreateTransport() override;

 private:
  void EnsureFreshJingleInfo();
  void OnJingleInfo(const std::string& relay_token,
                    const std::vector<std::string>& relay_hosts,
                    const std::vector<rtc::SocketAddress>& stun_hosts);

  SignalStrategy* signal_strategy_;
  scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator_;
  NetworkSettings network_settings_;
  TransportRole role_;

  base::TimeTicks last_jingle_info_update_time_;
  scoped_ptr<JingleInfoRequest> jingle_info_request_;

  // When there is an active |jingle_info_request_| stores list of callbacks to
  // be called once the |jingle_info_request_| is finished.
  std::list<base::Closure> on_jingle_info_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(IceTransportFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_ICE_TRANSPORT_FACTORY_H_
