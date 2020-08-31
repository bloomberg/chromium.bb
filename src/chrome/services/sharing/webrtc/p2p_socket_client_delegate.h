// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_P2P_SOCKET_CLIENT_DELEGATE_H_
#define CHROME_SERVICES_SHARING_WEBRTC_P2P_SOCKET_CLIENT_DELEGATE_H_

#include <vector>

#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/p2p_socket_type.h"

namespace sharing {

class P2PSocketClient;

// TODO(crbug.com/1044522): reuse code from blink instead.
class P2PSocketClientDelegate {
 public:
  virtual ~P2PSocketClientDelegate() {}

  // Called after the socket has been opened with the local endpoint address
  // as argument. Please note that in the presence of multiple interfaces,
  // you should not rely on the local endpoint address if possible.
  virtual void OnOpen(const net::IPEndPoint& local_address,
                      const net::IPEndPoint& remote_address) = 0;

  // For a socket that is listening on incoming TCP connection, this function is
  // called when a new client connects.
  virtual void OnIncomingTcpConnection(
      const net::IPEndPoint& address,
      std::unique_ptr<P2PSocketClient> client) = 0;

  // Called once for each Send() call after the send is complete.
  virtual void OnSendComplete(
      const network::P2PSendPacketMetrics& send_metrics) = 0;

  // Called if an non-retryable error occurs.
  virtual void OnError() = 0;

  // Called when data is received on the socket.
  virtual void OnDataReceived(const net::IPEndPoint& address,
                              const std::vector<int8_t>& data,
                              const base::TimeTicks& timestamp) = 0;
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_P2P_SOCKET_CLIENT_DELEGATE_H_
