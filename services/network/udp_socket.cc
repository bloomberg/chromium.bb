// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/udp_socket.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/log/net_log.h"
#include "net/socket/udp_socket.h"

namespace network {

namespace {

// TODO(xunjieli): Default read buffer size is too big. Make it customizable.
const int kMaxReadSize = 64 * 1024;
// The limit on data length for a UDP packet is 65,507 for IPv4 and 65,535 for
// IPv6.
const int kMaxPacketSize = kMaxReadSize - 1;

class SocketWrapperImpl : public UDPSocket::SocketWrapper {
 public:
  SocketWrapperImpl(net::DatagramSocket::BindType bind_type,
                    const net::RandIntCallback& rand_int_cb,
                    net::NetLog* net_log,
                    const net::NetLogSource& source)
      : socket_(bind_type, rand_int_cb, net_log, source) {}
  ~SocketWrapperImpl() override {}

  int Open(net::AddressFamily address_family) override {
    return socket_.Open(address_family);
  }
  int Connect(const net::IPEndPoint& remote_addr) override {
    return socket_.Connect(remote_addr);
  }
  int Bind(const net::IPEndPoint& local_addr) override {
    return socket_.Bind(local_addr);
  }
  int SetSendBufferSize(uint32_t size) override {
    return socket_.SetSendBufferSize(size);
  }
  int SetReceiveBufferSize(uint32_t size) override {
    return socket_.SetReceiveBufferSize(size);
  }
  int SendTo(
      net::IOBuffer* buf,
      int buf_len,
      const net::IPEndPoint& dest_addr,
      const net::CompletionCallback& callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    return socket_.SendTo(buf, buf_len, dest_addr, callback);
  }
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      const net::CompletionCallback& callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    return socket_.Write(buf, buf_len, callback, traffic_annotation);
  }
  int RecvFrom(net::IOBuffer* buf,
               int buf_len,
               net::IPEndPoint* address,
               const net::CompletionCallback& callback) override {
    return socket_.RecvFrom(buf, buf_len, address, callback);
  }
  int GetLocalAddress(net::IPEndPoint* address) const override {
    return socket_.GetLocalAddress(address);
  }

 private:
  net::UDPSocket socket_;

  DISALLOW_COPY_AND_ASSIGN(SocketWrapperImpl);
};

}  // namespace

UDPSocket::PendingSendRequest::PendingSendRequest() {}

UDPSocket::PendingSendRequest::~PendingSendRequest() {}

UDPSocket::UDPSocket(mojom::UDPSocketRequest request,
                     mojom::UDPSocketReceiverPtr receiver)
    : is_opened_(false),
      is_bound_(false),
      is_connected_(false),
      receiver_(std::move(receiver)),
      wrapped_socket_(std::make_unique<SocketWrapperImpl>(
          net::DatagramSocket::RANDOM_BIND,
          base::BindRepeating(&base::RandInt),
          nullptr,
          net::NetLogSource())),
      remaining_recv_slots_(0),
      binding_(this) {
  binding_.Bind(std::move(request));
}

UDPSocket::~UDPSocket() {}

void UDPSocket::set_connection_error_handler(base::OnceClosure handler) {
  binding_.set_connection_error_handler(std::move(handler));
}

void UDPSocket::Open(net::AddressFamily address_family, OpenCallback callback) {
  if (is_opened_) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  int result = wrapped_socket_->Open(address_family);
  is_opened_ = (result == net::OK);
  std::move(callback).Run(result);
}

void UDPSocket::Connect(const net::IPEndPoint& remote_addr,
                        ConnectCallback callback) {
  net::IPEndPoint local_addr_out;
  int result;
  if (!is_opened_) {
    result = net::ERR_UNEXPECTED;
    std::move(callback).Run(result, base::nullopt);
    return;
  }
  if (IsConnectedOrBound()) {
    result = net::ERR_SOCKET_IS_CONNECTED;
    std::move(callback).Run(result, base::nullopt);
    return;
  }
  result = wrapped_socket_->Connect(remote_addr);
  if (result == net::OK) {
    is_connected_ = true;
    result = wrapped_socket_->GetLocalAddress(&local_addr_out);
  }
  std::move(callback).Run(result, local_addr_out);
}

void UDPSocket::Bind(const net::IPEndPoint& local_addr, BindCallback callback) {
  net::IPEndPoint local_addr_out;

  int result;
  if (!is_opened_) {
    result = net::ERR_UNEXPECTED;
    std::move(callback).Run(result, base::nullopt);
    return;
  }
  if (IsConnectedOrBound()) {
    result = net::ERR_SOCKET_IS_CONNECTED;
    std::move(callback).Run(result, base::nullopt);
    return;
  }
  result = wrapped_socket_->Bind(local_addr);
  if (result == net::OK) {
    is_bound_ = true;
    result = wrapped_socket_->GetLocalAddress(&local_addr_out);
  }
  std::move(callback).Run(result, base::make_optional(local_addr_out));
}

void UDPSocket::SetSendBufferSize(uint32_t size,
                                  SetSendBufferSizeCallback callback) {
  if (!is_opened_) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  int net_result =
      wrapped_socket_->SetSendBufferSize(base::saturated_cast<int32_t>(size));
  std::move(callback).Run(net_result);
}

void UDPSocket::SetReceiveBufferSize(uint32_t size,
                                     SetReceiveBufferSizeCallback callback) {
  if (!is_opened_) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  int net_result = wrapped_socket_->SetReceiveBufferSize(
      base::saturated_cast<int32_t>(size));
  std::move(callback).Run(net_result);
}

void UDPSocket::ReceiveMore(uint32_t num_additional_datagrams) {
  if (!receiver_)
    return;
  if (!IsConnectedOrBound()) {
    receiver_->OnReceived(net::ERR_UNEXPECTED, base::nullopt, base::nullopt);
    return;
  }
  if (num_additional_datagrams == 0)
    return;
  // Check for overflow.
  if (!base::CheckAdd(remaining_recv_slots_, num_additional_datagrams)
           .AssignIfValid(&remaining_recv_slots_)) {
    return;
  }
  if (!recvfrom_buffer_) {
    DCHECK_EQ(num_additional_datagrams, remaining_recv_slots_);
    DoRecvFrom();
  }
}

void UDPSocket::SendTo(
    const net::IPEndPoint& dest_addr,
    base::span<const uint8_t> data,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    SendToCallback callback) {
  if (!is_bound_) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  DoSendToOrWrite(
      &dest_addr, data,
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(callback));
}

void UDPSocket::Send(
    base::span<const uint8_t> data,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    SendCallback callback) {
  if (!is_connected_) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  DoSendToOrWrite(
      nullptr, data,
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(callback));
}

bool UDPSocket::IsConnectedOrBound() const {
  return is_connected_ || is_bound_;
}

void UDPSocket::DoRecvFrom() {
  DCHECK(receiver_);
  DCHECK(!recvfrom_buffer_);
  DCHECK_GT(remaining_recv_slots_, 0u);

  recvfrom_buffer_ = new net::IOBuffer(kMaxReadSize);

  // base::Unretained(this) is safe because socket is owned by |this|.
  int net_result = wrapped_socket_->RecvFrom(
      recvfrom_buffer_.get(), kMaxReadSize, &recvfrom_address_,
      base::BindRepeating(&UDPSocket::OnRecvFromCompleted,
                          base::Unretained(this)));
  if (net_result != net::ERR_IO_PENDING)
    OnRecvFromCompleted(net_result);
}

void UDPSocket::DoSendToOrWrite(
    const net::IPEndPoint* dest_addr,
    const base::span<const uint8_t>& data,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    SendToCallback callback) {
  if (pending_send_requests_.size() >= kMaxPendingSendRequests) {
    std::move(callback).Run(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  if (data.size() > kMaxPacketSize) {
    std::move(callback).Run(net::ERR_MSG_TOO_BIG);
    return;
  }

  // |data| points to a range of bytes in the received message and will be
  // freed when this method returns, so copy out the bytes now.
  scoped_refptr<net::IOBufferWithSize> buffer =
      new net::IOBufferWithSize(data.size());
  memcpy(buffer.get()->data(), data.begin(), data.size());

  if (send_buffer_.get()) {
    auto request = std::make_unique<PendingSendRequest>();
    if (dest_addr)
      request->addr = std::make_unique<net::IPEndPoint>(*dest_addr);
    request->data = buffer;
    request->traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(traffic_annotation);
    request->callback = std::move(callback);
    pending_send_requests_.push_back(std::move(request));
    return;
  }

  DoSendToOrWriteBuffer(dest_addr, buffer, traffic_annotation,
                        std::move(callback));
}

void UDPSocket::DoSendToOrWriteBuffer(
    const net::IPEndPoint* dest_addr,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    SendToCallback callback) {
  DCHECK(!send_buffer_);
  DCHECK(send_callback_.is_null());

  send_buffer_ = buffer;
  send_callback_ = std::move(callback);
  // base::Unretained(this) is safe because socket is owned by |this|.
  int net_result;
  if (dest_addr) {
    net_result = wrapped_socket_->SendTo(
        buffer.get(), buffer->size(), *dest_addr,
        base::BindRepeating(&UDPSocket::OnSendToCompleted,
                            base::Unretained(this)),
        traffic_annotation);
  } else {
    net_result = wrapped_socket_->Write(
        buffer.get(), buffer->size(),
        base::BindRepeating(&UDPSocket::OnSendToCompleted,
                            base::Unretained(this)),
        traffic_annotation);
  }
  if (net_result != net::ERR_IO_PENDING)
    OnSendToCompleted(net_result);
}

void UDPSocket::OnRecvFromCompleted(int net_result) {
  DCHECK(recvfrom_buffer_);

  if (net_result >= 0) {
    receiver_->OnReceived(
        net::OK,
        is_bound_ ? base::make_optional(recvfrom_address_) : base::nullopt,
        base::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(recvfrom_buffer_->data()),
            static_cast<size_t>(net_result)));
  } else {
    receiver_->OnReceived(net_result, base::nullopt, base::nullopt);
  }
  recvfrom_buffer_ = nullptr;
  DCHECK_GT(remaining_recv_slots_, 0u);
  remaining_recv_slots_--;
  if (remaining_recv_slots_ > 0)
    DoRecvFrom();
}

void UDPSocket::OnSendToCompleted(int net_result) {
  DCHECK(send_buffer_.get());
  DCHECK(!send_callback_.is_null());

  if (net_result > net::OK)
    net_result = net::OK;
  send_buffer_ = nullptr;
  std::move(send_callback_).Run(net_result);

  if (pending_send_requests_.empty())
    return;
  std::unique_ptr<PendingSendRequest> request =
      std::move(pending_send_requests_.front());
  pending_send_requests_.pop_front();
  DoSendToOrWriteBuffer(request->addr.get(), request->data,
                        static_cast<net::NetworkTrafficAnnotationTag>(
                            request->traffic_annotation),
                        std::move(request->callback));
}

}  // namespace network
