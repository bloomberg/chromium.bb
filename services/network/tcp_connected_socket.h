// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TCP_CONNECTED_SOCKET_H_
#define SERVICES_NETWORK_TCP_CONNECTED_SOCKET_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/address_family.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/interfaces/address_family.mojom.h"
#include "net/interfaces/ip_endpoint.mojom.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace net {
class NetLog;
class StreamSocket;
}  // namespace net

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) TCPConnectedSocket
    : public mojom::TCPConnectedSocket {
 public:
  TCPConnectedSocket(
      mojom::TCPConnectedSocketObserverPtr observer,
      net::NetLog* net_log,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  TCPConnectedSocket(
      mojom::TCPConnectedSocketObserverPtr observer,
      std::unique_ptr<net::StreamSocket> socket,
      mojo::ScopedDataPipeProducerHandle receive_pipe_handle,
      mojo::ScopedDataPipeConsumerHandle send_pipe_handle,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~TCPConnectedSocket() override;
  void Connect(
      const base::Optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      mojom::NetworkContext::CreateTCPConnectedSocketCallback callback);

  // mojom::TCPConnectedSocket implementation.
  void GetLocalAddress(GetLocalAddressCallback callback) override;

 private:
  // Invoked when net::TCPClientSocket::Connect() completes.
  void OnConnectCompleted(int net_result);

  // Helper to start watching |send_stream_| and |receive_stream_|.
  void StartWatching();

  // "Receiving" in this context means reading from |socket_| and writing to
  // the Mojo |receive_stream_|.
  void ReceiveMore();
  void OnReceiveStreamWritable(MojoResult result);
  void OnNetworkReadCompleted(int result);
  void ShutdownReceive();

  // "Writing" is reading from the Mojo |send_stream_| and writing to the
  // |socket_|.
  void SendMore();
  void OnSendStreamReadable(MojoResult result);
  void OnNetworkWriteCompleted(int result);
  void ShutdownSend();

  mojom::TCPConnectedSocketObserverPtr observer_;

  net::NetLog* net_log_;

  std::unique_ptr<net::StreamSocket> socket_;

  mojom::NetworkContext::CreateTCPConnectedSocketCallback connect_callback_;

  // The *stream handles will be null while there is an in-progress transation
  // between net and mojo. During this time, the handle will be owned by the
  // *PendingBuffer.

  // For reading from the Mojo pipe and writing to the network.
  mojo::ScopedDataPipeConsumerHandle send_stream_;
  scoped_refptr<MojoToNetPendingBuffer> pending_send_;
  mojo::SimpleWatcher readable_handle_watcher_;

  // For reading from the network and writing to Mojo pipe.
  mojo::ScopedDataPipeProducerHandle receive_stream_;
  scoped_refptr<NetToMojoPendingBuffer> pending_receive_;
  mojo::SimpleWatcher writable_handle_watcher_;

  net::NetworkTrafficAnnotationTag traffic_annotation_;

  DISALLOW_COPY_AND_ASSIGN(TCPConnectedSocket);
};

}  // namespace network

#endif  // SERVICES_NETWORK_TCP_CONNECTED_SOCKET_H_
