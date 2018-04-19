// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SOCKET_DATA_PUMP_H_
#define SERVICES_NETWORK_SOCKET_DATA_PUMP_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/address_family.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace network {

// This class handles reading/writing logic between a network socket and a mojo
// pipe. Specifically, it (1) reads from the network socket and writes to a mojo
// producer pipe, and (2) reads from a mojo consumer pipe and writes to the
// network socket. On network read/write errors, it (3) also notifies the
// mojom::TCPConnectedSocketObserverPtr appropriately.
class COMPONENT_EXPORT(NETWORK_SERVICE) SocketDataPump {
 public:
  // Constructs a data pump that pumps data between |socket| and mojo data
  // pipe handles. Data are read from |send_pipe_handle| and sent to |socket|.
  // Data are read from |socket| and written to |receive_pipe_handle|.
  // |traffic_annotation| is attached to all writes to |socket|. Note that
  // |socket| must outlive |this|.
  SocketDataPump(mojom::TCPConnectedSocketObserverPtr observer,
                 net::StreamSocket* socket,
                 mojo::ScopedDataPipeProducerHandle receive_pipe_handle,
                 mojo::ScopedDataPipeConsumerHandle send_pipe_handle,
                 const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~SocketDataPump();

 private:
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

  net::StreamSocket* socket_;

  // The *stream handles will be null when there's a pending read from |socket_|
  // to |pending_receive_buffer_|, or while there is a pending write from
  // |pending_send_buffer_| to |socket_|. During this time, the handles will
  // be owned by the *PendingBuffer.
  //
  // The watchers need to be declared after the corresponding handle, so they
  // can be cleaned up before the handles are.

  // For reading from the network and writing to Mojo pipe.
  mojo::ScopedDataPipeProducerHandle receive_stream_;
  scoped_refptr<NetToMojoPendingBuffer> pending_receive_buffer_;
  mojo::SimpleWatcher receive_stream_watcher_;

  // For reading from the Mojo pipe and writing to the network.
  mojo::ScopedDataPipeConsumerHandle send_stream_;
  scoped_refptr<MojoToNetPendingBuffer> pending_send_buffer_;
  mojo::SimpleWatcher send_stream_watcher_;

  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  base::WeakPtrFactory<SocketDataPump> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SocketDataPump);
};

}  // namespace network

#endif  // SERVICES_NETWORK_SOCKET_DATA_PUMP_H_
