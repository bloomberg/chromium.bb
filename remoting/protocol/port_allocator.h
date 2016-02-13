// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PORT_ALLOCATOR_H_
#define REMOTING_PROTOCOL_PORT_ALLOCATOR_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "remoting/base/url_request.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/protocol/transport_context.h"
#include "third_party/webrtc/p2p/client/basicportallocator.h"

namespace remoting {
namespace protocol {

class PortAllocator : public cricket::BasicPortAllocator {
 public:
  PortAllocator(scoped_ptr<rtc::NetworkManager> network_manager,
                scoped_ptr<rtc::PacketSocketFactory> socket_factory,
                scoped_refptr<TransportContext> transport_context);
  ~PortAllocator() override;

  scoped_refptr<TransportContext> transport_context() {
    return transport_context_;
  }

  cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) override;

 private:
  scoped_ptr<rtc::NetworkManager> network_manager_;
  scoped_ptr<rtc::PacketSocketFactory> socket_factory_;
  scoped_refptr<TransportContext> transport_context_;
};

class PortAllocatorSession : public cricket::BasicPortAllocatorSession {
 public:
  PortAllocatorSession(PortAllocator* allocator,
                       const std::string& content_name,
                       int component,
                       const std::string& ice_ufrag,
                       const std::string& ice_pwd);
  ~PortAllocatorSession() override;

private:
  void GetPortConfigurations() override;
  void OnIceConfig(const IceConfig& ice_config);
  void TryCreateRelaySession();
  void OnSessionRequestResult(const UrlRequest::Result& result);

  scoped_refptr<TransportContext> transport_context_;

  IceConfig ice_config_;

  int attempts_ = 0;

  std::set<scoped_ptr<UrlRequest>> url_requests_;

  base::WeakPtrFactory<PortAllocatorSession> weak_factory_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PORT_ALLOCATOR_H_
