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

namespace cricket {
class PortAllocator;
}  // namespace cricket

namespace remoting {

class SignalStrategy;

namespace protocol {

class PortAllocatorFactory;

// TransportContext is responsible for storing all parameters required for
// P2P transport initialization. It's also responsible for making JingleInfo
// request and creation of PortAllocators configured according to the JingleInfo
// result.
class TransportContext : public base::RefCountedThreadSafe<TransportContext> {
 public:
  typedef base::Callback<void(scoped_ptr<cricket::PortAllocator>
                                  port_allocator)> CreatePortAllocatorCallback;

  TransportContext(
      SignalStrategy* signal_strategy,
      scoped_ptr<PortAllocatorFactory> port_allocator_factory,
      const NetworkSettings& network_settings,
      TransportRole role);

  // Prepares to create new PortAllocator instances. It may be called while
  // connection is being negotiated to minimize the chance that the following
  // CreatePortAllocator() will be blocking.
  void Prepare();

  // Creates new PortAllocator making sure that it has correct STUN and TURN
  // information.
  void CreatePortAllocator(const CreatePortAllocatorCallback& callback);

  const NetworkSettings& network_settings() { return network_settings_; }
  TransportRole role() { return role_; }

 private:
  friend class base::RefCountedThreadSafe<TransportContext>;

  ~TransportContext();

  void EnsureFreshJingleInfo();
  void OnJingleInfo(const std::string& relay_token,
                    const std::vector<std::string>& relay_hosts,
                    const std::vector<rtc::SocketAddress>& stun_hosts);

  scoped_ptr<cricket::PortAllocator> CreatePortAllocatorInternal();

  SignalStrategy* signal_strategy_;
  scoped_ptr<PortAllocatorFactory> port_allocator_factory_;
  NetworkSettings network_settings_;
  TransportRole role_;

  base::TimeTicks last_jingle_info_update_time_;
  scoped_ptr<JingleInfoRequest> jingle_info_request_;

  std::string relay_token_;
  std::vector<std::string> relay_hosts_;
  std::vector<rtc::SocketAddress> stun_hosts_;

  // When there is an active |jingle_info_request_| stores list of callbacks to
  // be called once the |jingle_info_request_| is finished.
  std::list<CreatePortAllocatorCallback> pending_port_allocator_requests_;

  DISALLOW_COPY_AND_ASSIGN(TransportContext);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_
