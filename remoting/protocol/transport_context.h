// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_
#define REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_

#include <list>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/jingle_info_request.h"


namespace remoting {

class SignalStrategy;
class UrlRequestFactory;

namespace protocol {

class PortAllocatorFactory;

// TransportContext is responsible for storing all parameters required for
// P2P transport initialization. It's also responsible for making JingleInfo
// request.
class TransportContext : public base::RefCountedThreadSafe<TransportContext> {
 public:
  typedef base::Callback<void(std::vector<rtc::SocketAddress> stun_hosts,
                              std::vector<std::string> relay_hosts,
                              std::string relay_token)> GetJingleInfoCallback;

  static scoped_refptr<TransportContext> ForTests(TransportRole role);

  TransportContext(SignalStrategy* signal_strategy,
                   scoped_ptr<PortAllocatorFactory> port_allocator_factory,
                   scoped_ptr<UrlRequestFactory> url_request_factory,
                   const NetworkSettings& network_settings,
                   TransportRole role);

  // Prepares fresh JingleInfo. It may be called while connection is being
  // negotiated to minimize the chance that the following GetJingleInfo() will
  // be blocking.
  void Prepare();

  // Requests fresh STUN and TURN information.
  void GetJingleInfo(const GetJingleInfoCallback& callback);

  PortAllocatorFactory* port_allocator_factory() {
    return port_allocator_factory_.get();
  }
  UrlRequestFactory* url_request_factory() {
    return url_request_factory_.get();
  }
  const NetworkSettings& network_settings() const { return network_settings_; }
  TransportRole role() const { return role_; }

 private:
  friend class base::RefCountedThreadSafe<TransportContext>;

  ~TransportContext();

  void EnsureFreshJingleInfo();
  void OnJingleInfo(const std::string& relay_token,
                    const std::vector<std::string>& relay_hosts,
                    const std::vector<rtc::SocketAddress>& stun_hosts);

  SignalStrategy* signal_strategy_;
  scoped_ptr<PortAllocatorFactory> port_allocator_factory_;
  scoped_ptr<UrlRequestFactory> url_request_factory_;
  NetworkSettings network_settings_;
  TransportRole role_;

  base::TimeTicks last_jingle_info_update_time_;
  scoped_ptr<JingleInfoRequest> jingle_info_request_;

  std::vector<rtc::SocketAddress> stun_hosts_;
  std::vector<std::string> relay_hosts_;
  std::string relay_token_;

  // When there is an active |jingle_info_request_| stores list of callbacks to
  // be called once the request is finished.
  std::list<GetJingleInfoCallback> pending_jingle_info_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(TransportContext);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_
