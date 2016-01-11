// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PORT_ALLOCATOR_BASE_H_
#define REMOTING_PROTOCOL_PORT_ALLOCATOR_BASE_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/p2p/client/basicportallocator.h"

namespace remoting {
namespace protocol {

class TransportContext;

class PortAllocatorBase : public cricket::BasicPortAllocator {
 public:
  // The number of HTTP requests we should attempt before giving up.
  static const int kNumRetries;

  PortAllocatorBase(scoped_ptr<rtc::NetworkManager> network_manager,
                    scoped_ptr<rtc::PacketSocketFactory> socket_factory,
                    scoped_refptr<TransportContext> transport_context);
  ~PortAllocatorBase() override;

  scoped_refptr<TransportContext> transport_context() {
    return transport_context_;
  }

  // CreateSession is defined in cricket::BasicPortAllocator but is
  // redefined here as pure virtual.
  cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) override = 0;

 private:
  scoped_ptr<rtc::NetworkManager> network_manager_;
  scoped_ptr<rtc::PacketSocketFactory> socket_factory_;
  scoped_refptr<TransportContext> transport_context_;
};

class PortAllocatorSessionBase : public cricket::BasicPortAllocatorSession {
 public:
  PortAllocatorSessionBase(PortAllocatorBase* allocator,
                           const std::string& content_name,
                           int component,
                           const std::string& ice_ufrag,
                           const std::string& ice_pwd);
  ~PortAllocatorSessionBase() override;

  virtual void SendSessionRequest(const std::string& host) = 0;
  void ReceiveSessionResponse(const std::string& response);

 protected:
  std::string GetSessionRequestUrl();

  void GetPortConfigurations() override;
  void OnJingleInfo(std::vector<rtc::SocketAddress> stun_hosts,
                    std::vector<std::string> relay_hosts,
                    std::string relay_token);
  void TryCreateRelaySession();

  const std::string& relay_token() const { return relay_token_; }

 private:
  scoped_refptr<TransportContext> transport_context_;

  std::vector<rtc::SocketAddress> stun_hosts_;
  std::vector<std::string> relay_hosts_;
  std::string relay_token_;

  int attempts_ = 0;

  base::WeakPtrFactory<PortAllocatorSessionBase> weak_factory_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PORT_ALLOCATOR_BASE_H_
