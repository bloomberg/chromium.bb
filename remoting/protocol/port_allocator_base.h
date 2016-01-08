// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PORT_ALLOCATOR_BASE_H_
#define REMOTING_PROTOCOL_PORT_ALLOCATOR_BASE_H_

#include <string>
#include <vector>

#include "third_party/webrtc/p2p/client/basicportallocator.h"

namespace remoting {
namespace protocol {

class PortAllocatorBase : public cricket::BasicPortAllocator {
 public:
  // The number of HTTP requests we should attempt before giving up.
  static const int kNumRetries;

  PortAllocatorBase(rtc::NetworkManager* network_manager,
                    rtc::PacketSocketFactory* socket_factory);
  ~PortAllocatorBase() override;

  // CreateSession is defined in cricket::BasicPortAllocator but is
  // redefined here as pure virtual.
  cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) override = 0;

  void SetStunHosts(const std::vector<rtc::SocketAddress>& hosts);
  void SetRelayHosts(const std::vector<std::string>& hosts);
  void SetRelayToken(const std::string& relay);

  const std::vector<rtc::SocketAddress>& stun_hosts() const {
    return stun_hosts_;
  }

  const std::vector<std::string>& relay_hosts() const { return relay_hosts_; }
  const std::string& relay_token() const { return relay_token_; }

 private:
  std::vector<rtc::SocketAddress> stun_hosts_;
  std::vector<std::string> relay_hosts_;
  std::string relay_token_;
};

class PortAllocatorSessionBase : public cricket::BasicPortAllocatorSession {
 public:
  PortAllocatorSessionBase(PortAllocatorBase* allocator,
                           const std::string& content_name,
                           int component,
                           const std::string& ice_ufrag,
                           const std::string& ice_pwd,
                           const std::vector<rtc::SocketAddress>& stun_hosts,
                           const std::vector<std::string>& relay_hosts,
                           const std::string& relay);
  ~PortAllocatorSessionBase() override;

  const std::string& relay_token() const { return relay_token_; }

  virtual void SendSessionRequest(const std::string& host) = 0;
  virtual void ReceiveSessionResponse(const std::string& response);

 protected:
  std::string GetSessionRequestUrl();
  void GetPortConfigurations() override;
  void TryCreateRelaySession();
  PortAllocatorBase* allocator() override;

 private:
  std::vector<std::string> relay_hosts_;
  std::vector<rtc::SocketAddress> stun_hosts_;
  std::string relay_token_;
  int attempts_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PORT_ALLOCATOR_BASE_H_
