// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/tcp_connected_socket.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"

namespace network {

TCPConnectedSocket::TCPConnectedSocket(
    mojom::TCPConnectedSocketObserverPtr observer,
    net::NetLog* net_log,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : observer_(std::move(observer)),
      net_log_(net_log),
      readable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      traffic_annotation_(traffic_annotation) {
}

TCPConnectedSocket::TCPConnectedSocket(
    mojom::TCPConnectedSocketObserverPtr observer,
    std::unique_ptr<net::StreamSocket> socket,
    mojo::ScopedDataPipeProducerHandle receive_pipe_handle,
    mojo::ScopedDataPipeConsumerHandle send_pipe_handle,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : observer_(std::move(observer)),
      net_log_(nullptr),
      socket_(std::move(socket)),
      send_stream_(std::move(send_pipe_handle)),
      readable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      receive_stream_(std::move(receive_pipe_handle)),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      traffic_annotation_(traffic_annotation) {
  StartWatching();
}

TCPConnectedSocket::~TCPConnectedSocket() {}

void TCPConnectedSocket::Connect(
    const base::Optional<net::IPEndPoint>& local_addr,
    const net::AddressList& remote_addr_list,
    mojom::NetworkContext::CreateTCPConnectedSocketCallback callback) {
  DCHECK(!socket_);
  DCHECK(callback);

  auto socket = std::make_unique<net::TCPClientSocket>(
      remote_addr_list, nullptr /*socket_performance_watcher*/, net_log_,
      net::NetLogSource());
  connect_callback_ = std::move(callback);
  int result = net::OK;
  if (local_addr)
    result = socket->Bind(local_addr.value());
  if (result == net::OK) {
    result = socket->Connect(base::BindRepeating(
        &TCPConnectedSocket::OnConnectCompleted, base::Unretained(this)));
  }
  socket_ = std::move(socket);
  if (result == net::ERR_IO_PENDING)
    return;
  OnConnectCompleted(result);
}

void TCPConnectedSocket::GetLocalAddress(GetLocalAddressCallback callback) {
  DCHECK(socket_);

  net::IPEndPoint local_addr;
  int result = socket_->GetLocalAddress(&local_addr);
  if (result != net::OK) {
    std::move(callback).Run(result, base::nullopt);
    return;
  }
  std::move(callback).Run(result, local_addr);
}

void TCPConnectedSocket::OnConnectCompleted(int result) {
  DCHECK(!connect_callback_.is_null());
  DCHECK(!receive_stream_.is_valid());
  DCHECK(!send_stream_.is_valid());

  if (result != net::OK) {
    std::move(connect_callback_)
        .Run(result, mojo::ScopedDataPipeConsumerHandle(),
             mojo::ScopedDataPipeProducerHandle());
    return;
  }
  mojo::DataPipe send_pipe;
  mojo::DataPipe receive_pipe;
  receive_stream_ = std::move(receive_pipe.producer_handle);
  send_stream_ = std::move(send_pipe.consumer_handle);

  StartWatching();
  std::move(connect_callback_)
      .Run(net::OK, std::move(receive_pipe.consumer_handle),
           std::move(send_pipe.producer_handle));
}

void TCPConnectedSocket::StartWatching() {
  readable_handle_watcher_.Watch(
      send_stream_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&TCPConnectedSocket::OnSendStreamReadable,
                          base::Unretained(this)));
  writable_handle_watcher_.Watch(
      receive_stream_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&TCPConnectedSocket::OnReceiveStreamWritable,
                          base::Unretained(this)));
  ReceiveMore();
  SendMore();
}

void TCPConnectedSocket::ReceiveMore() {
  DCHECK(receive_stream_.is_valid());
  DCHECK(!pending_receive_);

  uint32_t num_bytes;
  MojoResult result = NetToMojoPendingBuffer::BeginWrite(
      &receive_stream_, &pending_receive_, &num_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    writable_handle_watcher_.ArmOrNotify();
    return;
  }
  if (result != MOJO_RESULT_OK) {
    ShutdownReceive();
    return;
  }
  DCHECK_EQ(MOJO_RESULT_OK, result);
  DCHECK(pending_receive_);
  scoped_refptr<net::IOBuffer> buf(
      new NetToMojoIOBuffer(pending_receive_.get()));
  int read_result = socket_->Read(
      buf.get(), base::saturated_cast<int>(num_bytes),
      base::BindRepeating(&TCPConnectedSocket::OnNetworkReadCompleted,
                          base::Unretained(this)));
  if (read_result == net::ERR_IO_PENDING)
    return;
  OnNetworkReadCompleted(read_result);
}

void TCPConnectedSocket::OnReceiveStreamWritable(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    ShutdownReceive();
    return;
  }
  ReceiveMore();
}

void TCPConnectedSocket::OnNetworkReadCompleted(int bytes_read) {
  DCHECK(pending_receive_);

  if (bytes_read < 0 && observer_)
    observer_->OnReadError(bytes_read);

  if (bytes_read <= 0) {
    ShutdownReceive();
    return;
  }
  if (bytes_read > 0) {
    receive_stream_ = pending_receive_->Complete(bytes_read);
    pending_receive_ = nullptr;
    ReceiveMore();
  }
}

void TCPConnectedSocket::ShutdownReceive() {
  writable_handle_watcher_.Cancel();
  pending_receive_ = nullptr;
  receive_stream_.reset();
}

void TCPConnectedSocket::SendMore() {
  DCHECK(send_stream_.is_valid());
  DCHECK(!pending_send_);

  uint32_t num_bytes = 0;
  MojoResult result = MojoToNetPendingBuffer::BeginRead(
      &send_stream_, &pending_send_, &num_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    readable_handle_watcher_.ArmOrNotify();
    return;
  }
  if (result != MOJO_RESULT_OK) {
    ShutdownSend();
    return;
  }
  DCHECK_EQ(MOJO_RESULT_OK, result);
  DCHECK(pending_send_);
  scoped_refptr<net::IOBuffer> buf(
      new net::WrappedIOBuffer(pending_send_->buffer()));
  int write_result = socket_->Write(
      buf.get(), static_cast<int>(num_bytes),
      base::BindRepeating(&TCPConnectedSocket::OnNetworkWriteCompleted,
                          base::Unretained(this)),
      traffic_annotation_);
  if (write_result == net::ERR_IO_PENDING)
    return;
  OnNetworkWriteCompleted(write_result);
}

void TCPConnectedSocket::OnSendStreamReadable(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    ShutdownSend();
    return;
  }
  SendMore();
}

void TCPConnectedSocket::OnNetworkWriteCompleted(int bytes_written) {
  DCHECK(pending_send_);

  if (bytes_written < 0 && observer_)
    observer_->OnWriteError(bytes_written);
  if (bytes_written <= 0) {
    ShutdownSend();
    return;
  }

  if (bytes_written > 0) {
    // Partial write is possible.
    pending_send_->CompleteRead(bytes_written);
    send_stream_ = pending_send_->ReleaseHandle();
    pending_send_ = nullptr;
    SendMore();
  }
}

void TCPConnectedSocket::ShutdownSend() {
  readable_handle_watcher_.Cancel();
  pending_send_ = nullptr;
  send_stream_.reset();
}

}  // namespace network
