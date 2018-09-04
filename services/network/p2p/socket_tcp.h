// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_P2P_SOCKET_TCP_H_
#define SERVICES_NETWORK_P2P_SOCKET_TCP_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/p2p/socket.h"
#include "services/network/public/cpp/p2p_socket_type.h"

namespace net {
class DrainableIOBuffer;
class GrowableIOBuffer;
class StreamSocket;
}  // namespace net

namespace network {
class ProxyResolvingClientSocketFactory;

class COMPONENT_EXPORT(NETWORK_SERVICE) P2PSocketTcpBase : public P2PSocket {
 public:
  P2PSocketTcpBase(
      P2PSocketManager* socket_manager,
      mojom::P2PSocketClientPtr client,
      mojom::P2PSocketRequest socket,
      P2PSocketType type,
      ProxyResolvingClientSocketFactory* proxy_resolving_socket_factory);
  ~P2PSocketTcpBase() override;

  bool InitAccepted(const net::IPEndPoint& remote_address,
                    std::unique_ptr<net::StreamSocket> socket);

  // P2PSocket overrides.
  bool Init(const net::IPEndPoint& local_address,
            uint16_t min_port,
            uint16_t max_port,
            const P2PHostAndIPEndPoint& remote_address) override;

  // mojom::P2PSocket implementation:
  void AcceptIncomingTcpConnection(const net::IPEndPoint& remote_address,
                                   mojom::P2PSocketClientPtr client,
                                   mojom::P2PSocketRequest socket) override;
  void Send(const std::vector<int8_t>& data,
            const P2PPacketInfo& packet_info,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void SetOption(P2PSocketOption option, int32_t value) override;

 protected:
  struct SendBuffer {
    SendBuffer();
    SendBuffer(int32_t packet_id,
               scoped_refptr<net::DrainableIOBuffer> buffer,
               const net::NetworkTrafficAnnotationTag traffic_annotation);
    SendBuffer(const SendBuffer& rhs);
    ~SendBuffer();

    int32_t rtc_packet_id;
    scoped_refptr<net::DrainableIOBuffer> buffer;
    net::MutableNetworkTrafficAnnotationTag traffic_annotation;
  };

  // Derived classes will provide the implementation.
  virtual int ProcessInput(char* input, int input_len) = 0;
  virtual void DoSend(
      const net::IPEndPoint& to,
      const std::vector<int8_t>& data,
      const rtc::PacketOptions& options,
      const net::NetworkTrafficAnnotationTag traffic_annotation) = 0;

  void WriteOrQueue(SendBuffer& send_buffer);
  void OnPacket(const std::vector<int8_t>& data);
  void OnError();

 private:
  friend class P2PSocketTcpTestBase;
  friend class P2PSocketTcpServerTest;

  void DidCompleteRead(int result);
  void DoRead();

  void DoWrite();
  void HandleWriteResult(int result);

  // Callbacks for Connect(), Read() and Write().
  void OnConnected(int result);
  void OnRead(int result);
  void OnWritten(int result);

  // Helper method to send socket create message and start read.
  void OnOpen();
  bool DoSendSocketCreateMsg();

  P2PHostAndIPEndPoint remote_address_;

  std::unique_ptr<net::StreamSocket> socket_;
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;
  base::queue<SendBuffer> write_queue_;
  SendBuffer write_buffer_;

  bool write_pending_;

  bool connected_;
  P2PSocketType type_;
  ProxyResolvingClientSocketFactory* proxy_resolving_socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketTcpBase);
};

class COMPONENT_EXPORT(NETWORK_SERVICE) P2PSocketTcp : public P2PSocketTcpBase {
 public:
  P2PSocketTcp(
      P2PSocketManager* socket_manager,
      mojom::P2PSocketClientPtr client,
      mojom::P2PSocketRequest socket,
      P2PSocketType type,
      ProxyResolvingClientSocketFactory* proxy_resolving_socket_factory);

  ~P2PSocketTcp() override;

 protected:
  int ProcessInput(char* input, int input_len) override;
  void DoSend(
      const net::IPEndPoint& to,
      const std::vector<int8_t>& data,
      const rtc::PacketOptions& options,
      const net::NetworkTrafficAnnotationTag traffic_annotation) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(P2PSocketTcp);
};

// P2PSocketStunTcp class provides the framing of STUN messages when used
// with TURN. These messages will not have length at front of the packet and
// are padded to multiple of 4 bytes.
// Formatting of messages is defined in RFC5766.
class COMPONENT_EXPORT(NETWORK_SERVICE) P2PSocketStunTcp
    : public P2PSocketTcpBase {
 public:
  P2PSocketStunTcp(
      P2PSocketManager* socket_manager,
      mojom::P2PSocketClientPtr client,
      mojom::P2PSocketRequest socket,
      P2PSocketType type,
      ProxyResolvingClientSocketFactory* proxy_resolving_socket_factory);

  ~P2PSocketStunTcp() override;

 protected:
  int ProcessInput(char* input, int input_len) override;
  void DoSend(
      const net::IPEndPoint& to,
      const std::vector<int8_t>& data,
      const rtc::PacketOptions& options,
      const net::NetworkTrafficAnnotationTag traffic_annotation) override;

 private:
  int GetExpectedPacketSize(const int8_t* data, int len, int* pad_bytes);

  DISALLOW_COPY_AND_ASSIGN(P2PSocketStunTcp);
};

}  // namespace network

#endif  // SERVICES_NETWORK_P2P_SOCKET_TCP_H_
