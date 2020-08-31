// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/p2p_socket_client.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "chrome/services/sharing/webrtc/p2p_socket_client_delegate.h"
#include "crypto/random.h"
#include "services/network/public/cpp/p2p_param_traits.h"

namespace {

uint64_t GetUniqueId(uint32_t random_socket_id, uint32_t packet_id) {
  uint64_t uid = random_socket_id;
  uid <<= 32;
  uid |= packet_id;
  return uid;
}

}  // namespace

namespace sharing {

P2PSocketClient::P2PSocketClient(
    network::mojom::P2PSocketManager* socket_manager,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : socket_manager_(socket_manager),
      socket_id_(0),
      delegate_(nullptr),
      state_(STATE_UNINITIALIZED),
      traffic_annotation_(traffic_annotation),
      random_socket_id_(0),
      next_packet_id_(0) {
  crypto::RandBytes(&random_socket_id_, sizeof(random_socket_id_));
}

P2PSocketClient::~P2PSocketClient() {
  CHECK(state_ == STATE_CLOSED || state_ == STATE_UNINITIALIZED);
}

void P2PSocketClient::Init(network::P2PSocketType type,
                           const net::IPEndPoint& local_address,
                           uint16_t min_port,
                           uint16_t max_port,
                           const network::P2PHostAndIPEndPoint& remote_address,
                           P2PSocketClientDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate);
  // |delegate_| is only accessesed on |delegate_message_loop_|.
  delegate_ = delegate;

  DCHECK_EQ(state_, STATE_UNINITIALIZED);
  state_ = STATE_OPENING;
  socket_manager_->CreateSocket(
      type, local_address, network::P2PPortRange(min_port, max_port),
      remote_address, receiver_.BindNewPipeAndPassRemote(),
      socket_.BindNewPipeAndPassReceiver());
  receiver_.set_disconnect_handler(base::BindOnce(
      &P2PSocketClient::OnConnectionError, base::Unretained(this)));
}

uint64_t P2PSocketClient::Send(const net::IPEndPoint& address,
                               const std::vector<int8_t>& data,
                               const rtc::PacketOptions& options) {
  uint64_t unique_id = GetUniqueId(random_socket_id_, ++next_packet_id_);

  // Can send data only when the socket is open.
  DCHECK(state_ == STATE_OPEN || state_ == STATE_ERROR);
  if (state_ == STATE_OPEN) {
    SendWithPacketId(address, data, options, unique_id);
  }

  return unique_id;
}

void P2PSocketClient::SendWithPacketId(const net::IPEndPoint& address,
                                       const std::vector<int8_t>& data,
                                       const rtc::PacketOptions& options,
                                       uint64_t packet_id) {
  socket_->Send(data, network::P2PPacketInfo(address, options, packet_id),
                net::MutableNetworkTrafficAnnotationTag(traffic_annotation_));
}

void P2PSocketClient::SetOption(network::P2PSocketOption option, int value) {
  DCHECK(state_ == STATE_OPEN || state_ == STATE_ERROR);
  if (state_ == STATE_OPEN)
    socket_->SetOption(option, value);
}

void P2PSocketClient::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  delegate_ = nullptr;
  if (socket_)
    socket_.reset();

  state_ = STATE_CLOSED;
}

int P2PSocketClient::GetSocketID() const {
  return socket_id_;
}

void P2PSocketClient::SetDelegate(P2PSocketClientDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_ = delegate;
}

void P2PSocketClient::SocketCreated(const net::IPEndPoint& local_address,
                                    const net::IPEndPoint& remote_address) {
  state_ = STATE_OPEN;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_)
    delegate_->OnOpen(local_address, remote_address);
}

void P2PSocketClient::SendComplete(
    const network::P2PSendPacketMetrics& send_metrics) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_)
    delegate_->OnSendComplete(send_metrics);
}

void P2PSocketClient::IncomingTcpConnection(
    const net::IPEndPoint& socket_address,
    mojo::PendingRemote<network::mojom::P2PSocket> socket,
    mojo::PendingReceiver<network::mojom::P2PSocketClient> client_receiver) {
  DCHECK_EQ(state_, STATE_OPEN);

  auto new_client =
      std::make_unique<P2PSocketClient>(socket_manager_, traffic_annotation_);
  new_client->state_ = STATE_OPEN;

  new_client->socket_.Bind(std::move(socket));
  new_client->receiver_.Bind(std::move(client_receiver));
  new_client->receiver_.set_disconnect_handler(base::BindOnce(
      &P2PSocketClient::OnConnectionError, base::Unretained(this)));

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_) {
    delegate_->OnIncomingTcpConnection(socket_address, std::move(new_client));
  } else {
    // Just close the socket if there is no delegate to accept it.
    new_client->Close();
  }
}

void P2PSocketClient::DataReceived(const net::IPEndPoint& socket_address,
                                   const std::vector<int8_t>& data,
                                   base::TimeTicks timestamp) {
  DCHECK_EQ(STATE_OPEN, state_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_)
    delegate_->OnDataReceived(socket_address, data, timestamp);
}

void P2PSocketClient::OnConnectionError() {
  state_ = STATE_ERROR;
  if (delegate_)
    delegate_->OnError();
}

}  // namespace sharing
