// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_packet_socket_factory.h"

#include "base/bind.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/cpp/private/udp_socket_private.h"
#include "remoting/client/plugin/pepper_util.h"
#include "third_party/libjingle/source/talk/base/asyncpacketsocket.h"

namespace remoting {

namespace {

// Size of the buffer to allocate for RecvFrom().
const int kReceiveBufferSize = 65536;

// Maximum amount of data in the send buffers. This is necessary to
// prevent out-of-memory crashes if the caller sends data faster than
// Pepper's UDP API can handle it. This maximum should never be
// reached under normal conditions.
const int kMaxSendBufferSize = 256 * 1024;

class UdpPacketSocket : public talk_base::AsyncPacketSocket {
 public:
  explicit UdpPacketSocket(const pp::InstanceHandle& instance);
  virtual ~UdpPacketSocket();

  // |min_port| and |max_port| are set to zero if the port number
  // |should be assigned by the OS.
  bool Init(const talk_base::SocketAddress& local_address,
            int min_port,
            int max_port);

  // talk_base::AsyncPacketSocket interface.
  virtual talk_base::SocketAddress GetLocalAddress() const;
  virtual talk_base::SocketAddress GetRemoteAddress() const;
  virtual int Send(const void* data, size_t data_size);
  virtual int SendTo(const void* data,
                     size_t data_size,
                     const talk_base::SocketAddress& address);
  virtual int Close();
  virtual State GetState() const;
  virtual int GetOption(talk_base::Socket::Option opt, int* value);
  virtual int SetOption(talk_base::Socket::Option opt, int value);
  virtual int GetError() const;
  virtual void SetError(int error);

 private:
  struct PendingPacket {
    PendingPacket(const void* buffer,
                  int buffer_size,
                  const PP_NetAddress_Private& address);

    scoped_refptr<net::IOBufferWithSize> data;
    PP_NetAddress_Private address;
  };

  void OnBindCompleted(int error);

  void DoSend();
  void OnSendCompleted(int result);

  void DoRead();
  void OnReadCompleted(int result);
  void HandleReadResult(int result);

  pp::UDPSocketPrivate socket_;

  State state_;
  int error_;

  talk_base::SocketAddress local_address_;

  // Used to scan ports when necessary. Both values are set to 0 when
  // the port number is assigned by OS.
  uint16_t min_port_;
  uint16_t max_port_;

  std::vector<char> receive_buffer_;

  bool send_pending_;
  std::list<PendingPacket> send_queue_;
  int send_queue_size_;

  DISALLOW_COPY_AND_ASSIGN(UdpPacketSocket);
};

UdpPacketSocket::PendingPacket::PendingPacket(
    const void* buffer,
    int buffer_size,
    const PP_NetAddress_Private& address)
    : data(new net::IOBufferWithSize(buffer_size)),
      address(address) {
  memcpy(data->data(), buffer, buffer_size);
}

UdpPacketSocket::UdpPacketSocket(const pp::InstanceHandle& instance)
    : socket_(instance),
      state_(STATE_CLOSED),
      error_(0),
      min_port_(0),
      max_port_(0),
      send_pending_(false),
      send_queue_size_(0) {
}

UdpPacketSocket::~UdpPacketSocket() {
  Close();
}

bool UdpPacketSocket::Init(const talk_base::SocketAddress& local_address,
                           int min_port,
                           int max_port) {
  if (socket_.is_null()) {
    return false;
  }

  local_address_ = local_address;
  max_port_ = max_port;
  min_port_ = min_port;

  PP_NetAddress_Private pp_local_address;
  if (!SocketAddressToPpAddressWithPort(local_address_, &pp_local_address,
                                        min_port_)) {
    return false;
  }

  int result = socket_.Bind(&pp_local_address, PpCompletionCallback(
      base::Bind(&UdpPacketSocket::OnBindCompleted, base::Unretained(this))));
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
  state_ = STATE_BINDING;

  return true;
}

void UdpPacketSocket::OnBindCompleted(int result) {
  DCHECK(state_ == STATE_BINDING || state_ == STATE_CLOSED);

  if (result == PP_ERROR_ABORTED) {
    // Socket is being destroyed while binding.
    return;
  }

  if (result == PP_OK) {
    PP_NetAddress_Private address;
    if (socket_.GetBoundAddress(&address)) {
      PpAddressToSocketAddress(address, &local_address_);
    } else {
      LOG(ERROR) << "Failed to get bind address for bound socket?";
      error_ = EINVAL;
      return;
    }
    state_ = STATE_BOUND;
    SignalAddressReady(this, local_address_);
    DoRead();
    return;
  }

  if (min_port_ < max_port_) {
    // Try to bind to the next available port.
    ++min_port_;
    PP_NetAddress_Private pp_local_address;
    if (SocketAddressToPpAddressWithPort(local_address_, &pp_local_address,
                                         min_port_)) {
      int result = socket_.Bind(&pp_local_address, PpCompletionCallback(
          base::Bind(&UdpPacketSocket::OnBindCompleted,
                     base::Unretained(this))));
      DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
    }
  } else {
    LOG(ERROR) << "Failed to bind UDP socket: " << result;
  }
}

talk_base::SocketAddress UdpPacketSocket::GetLocalAddress() const {
  DCHECK_EQ(state_, STATE_BOUND);
  return local_address_;
}

talk_base::SocketAddress UdpPacketSocket::GetRemoteAddress() const {
  // UDP sockets are not connected - this method should never be called.
  NOTREACHED();
  return talk_base::SocketAddress();
}

int UdpPacketSocket::Send(const void* data, size_t data_size) {
  // UDP sockets are not connected - this method should never be called.
  NOTREACHED();
  return EWOULDBLOCK;
}

int UdpPacketSocket::SendTo(const void* data,
                            size_t data_size,
                            const talk_base::SocketAddress& address) {
  if (state_ != STATE_BOUND) {
    // TODO(sergeyu): StunPort may try to send stun request before we
    // are bound. Fix that problem and change this to DCHECK.
    return EINVAL;
  }

  if (error_ != 0) {
    return error_;
  }

  PP_NetAddress_Private pp_address;
  if (!SocketAddressToPpAddress(address, &pp_address)) {
    return EINVAL;
  }

  if (send_queue_size_ >= kMaxSendBufferSize) {
    return EWOULDBLOCK;
  }

  send_queue_.push_back(PendingPacket(data, data_size, pp_address));
  send_queue_size_ += data_size;
  DoSend();
  return data_size;
}

int UdpPacketSocket::Close() {
  state_ = STATE_CLOSED;
  socket_.Close();
  return 0;
}

talk_base::AsyncPacketSocket::State UdpPacketSocket::GetState() const {
  return state_;
}

int UdpPacketSocket::GetOption(talk_base::Socket::Option opt, int* value) {
  // Options are not supported for Pepper UDP sockets.
  return -1;
}

int UdpPacketSocket::SetOption(talk_base::Socket::Option opt, int value) {
  // Options are not supported for Pepper UDP sockets.
  return -1;
}

int UdpPacketSocket::GetError() const {
  return error_;
}

void UdpPacketSocket::SetError(int error) {
  error_ = error;
}

void UdpPacketSocket::DoSend() {
  if (send_pending_ || send_queue_.empty())
    return;

  int result = socket_.SendTo(
      send_queue_.front().data->data(), send_queue_.front().data->size(),
      &send_queue_.front().address,
      PpCompletionCallback(base::Bind(&UdpPacketSocket::OnSendCompleted,
                                      base::Unretained(this))));
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
  send_pending_ = true;
}

void UdpPacketSocket::OnSendCompleted(int result) {
  send_pending_ = false;

  if (result < 0) {
    if (result != PP_ERROR_ABORTED) {
      LOG(ERROR) << "Send failed on a UDP socket: " << result;
    }
    error_ = EINVAL;
    return;
  }

  send_queue_size_ -= send_queue_.front().data->size();
  send_queue_.pop_front();
  DoSend();
}

void UdpPacketSocket::DoRead() {
  receive_buffer_.resize(kReceiveBufferSize);
  int result = socket_.RecvFrom(
      &receive_buffer_[0], receive_buffer_.size(),
      PpCompletionCallback(base::Bind(&UdpPacketSocket::OnReadCompleted,
                                      base::Unretained(this))));
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
}

void UdpPacketSocket::OnReadCompleted(int result) {
  HandleReadResult(result);
  if (result > 0) {
    DoRead();
  }
}

void UdpPacketSocket::HandleReadResult(int result) {
  if (result > 0) {
    PP_NetAddress_Private pp_address;
    if (!socket_.GetRecvFromAddress(&pp_address)) {
      LOG(ERROR) << "GetRecvFromAddress() failed after successfull RecvFrom().";
      return;
    }
    talk_base::SocketAddress address;
    if (!PpAddressToSocketAddress(pp_address, &address)) {
      LOG(ERROR) << "Failed to covert address received from RecvFrom().";
      return;
    }
    SignalReadPacket(this, &receive_buffer_[0], result, address);
  } else if (result != PP_ERROR_ABORTED) {
    LOG(ERROR) << "Received error when reading from UDP socket: " << result;
  }
}

}  // namespace

PepperPacketSocketFactory::PepperPacketSocketFactory(
    const pp::InstanceHandle& instance)
    : pp_instance_(instance) {
}

PepperPacketSocketFactory::~PepperPacketSocketFactory() {
}

talk_base::AsyncPacketSocket* PepperPacketSocketFactory::CreateUdpSocket(
      const talk_base::SocketAddress& local_address,
      int min_port,
      int max_port) {
  scoped_ptr<UdpPacketSocket> result(new UdpPacketSocket(pp_instance_));
  if (!result->Init(local_address, min_port, max_port))
    return NULL;
  return result.release();
}

talk_base::AsyncPacketSocket* PepperPacketSocketFactory::CreateServerTcpSocket(
    const talk_base::SocketAddress& local_address,
    int min_port,
    int max_port,
    bool ssl) {
  // We don't use TCP sockets for remoting connections.
  NOTREACHED();
  return NULL;
}

talk_base::AsyncPacketSocket* PepperPacketSocketFactory::CreateClientTcpSocket(
      const talk_base::SocketAddress& local_address,
      const talk_base::SocketAddress& remote_address,
      const talk_base::ProxyInfo& proxy_info,
      const std::string& user_agent,
      bool ssl) {
  // We don't use TCP sockets for remoting connections.
  NOTREACHED();
  return NULL;
}

}  // namespace remoting
