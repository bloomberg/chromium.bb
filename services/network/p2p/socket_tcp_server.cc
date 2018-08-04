// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/p2p/socket_tcp_server.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"
#include "services/network/p2p/socket_manager.h"
#include "services/network/p2p/socket_tcp.h"
#include "services/network/public/cpp/p2p_param_traits.h"

namespace {
const int kListenBacklog = 5;
}  // namespace

namespace network {

P2PSocketTcpServer::P2PSocketTcpServer(P2PSocketManager* socket_manager,
                                       mojom::P2PSocketClientPtr client,
                                       mojom::P2PSocketRequest socket,
                                       P2PSocketType client_type)
    : P2PSocket(socket_manager,
                std::move(client),
                std::move(socket),
                P2PSocket::TCP),
      client_type_(client_type),
      socket_(new net::TCPServerSocket(nullptr, net::NetLogSource())),
      accept_callback_(base::BindRepeating(&P2PSocketTcpServer::OnAccepted,
                                           base::Unretained(this))) {}

P2PSocketTcpServer::~P2PSocketTcpServer() {
  if (state_ == STATE_OPEN) {
    DCHECK(socket_.get());
    socket_.reset();
  }
}

// TODO(guidou): Add support for port range.
bool P2PSocketTcpServer::Init(const net::IPEndPoint& local_address,
                              uint16_t min_port,
                              uint16_t max_port,
                              const P2PHostAndIPEndPoint& remote_address) {
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  int result = socket_->Listen(local_address, kListenBacklog);
  if (result < 0) {
    LOG(ERROR) << "Listen() failed: " << result;
    OnError();
    return false;
  }

  result = socket_->GetLocalAddress(&local_address_);
  if (result < 0) {
    LOG(ERROR) << "P2PSocketTcpServer::Init(): can't to get local address: "
               << result;
    OnError();
    return false;
  }
  VLOG(1) << "Local address: " << local_address_.ToString();

  state_ = STATE_OPEN;
  // NOTE: Remote address can be empty as socket is just listening
  // in this state.
  client_->SocketCreated(local_address_, remote_address.ip_address);
  DoAccept();
  return true;
}

std::unique_ptr<P2PSocketTcpBase>
P2PSocketTcpServer::AcceptIncomingTcpConnectionInternal(
    const net::IPEndPoint& remote_address,
    mojom::P2PSocketClientPtr client,
    mojom::P2PSocketRequest request) {
  auto it = accepted_sockets_.find(remote_address);
  if (it == accepted_sockets_.end())
    return nullptr;

  std::unique_ptr<net::StreamSocket> socket = std::move(it->second);
  accepted_sockets_.erase(it);

  std::unique_ptr<P2PSocketTcpBase> result;
  if (client_type_ == P2P_SOCKET_TCP_CLIENT) {
    result.reset(new P2PSocketTcp(socket_manager_, std::move(client),
                                  std::move(request), client_type_, nullptr));
  } else {
    result.reset(new P2PSocketStunTcp(socket_manager_, std::move(client),
                                      std::move(request), client_type_,
                                      nullptr));
  }
  if (!result->InitAccepted(remote_address, std::move(socket)))
    return nullptr;

  return result;
}

void P2PSocketTcpServer::OnError() {
  socket_.reset();

  if (state_ == STATE_UNINITIALIZED || state_ == STATE_OPEN) {
    binding_.Close();
    client_.reset();
  }

  state_ = STATE_ERROR;
}

void P2PSocketTcpServer::DoAccept() {
  while (true) {
    int result = socket_->Accept(&accept_socket_, accept_callback_);
    if (result == net::ERR_IO_PENDING) {
      break;
    } else {
      HandleAcceptResult(result);
    }
  }
}

void P2PSocketTcpServer::HandleAcceptResult(int result) {
  if (result < 0) {
    if (result != net::ERR_IO_PENDING)
      OnError();
    return;
  }

  net::IPEndPoint address;
  if (accept_socket_->GetPeerAddress(&address) != net::OK) {
    LOG(ERROR) << "Failed to get address of an accepted socket.";
    accept_socket_.reset();
    return;
  }
  accepted_sockets_[address] = std::move(accept_socket_);
  client_->IncomingTcpConnection(address);
}

void P2PSocketTcpServer::OnAccepted(int result) {
  HandleAcceptResult(result);
  if (result == net::OK)
    DoAccept();
}

void P2PSocketTcpServer::AcceptIncomingTcpConnection(
    const net::IPEndPoint& remote_address,
    mojom::P2PSocketClientPtr client,
    mojom::P2PSocketRequest request) {
  std::unique_ptr<P2PSocketTcpBase> socket =
      AcceptIncomingTcpConnectionInternal(remote_address, std::move(client),
                                          std::move(request));
  if (socket)
    socket_manager_->AddAcceptedConnection(std::move(socket));
}

void P2PSocketTcpServer::Send(
    const std::vector<int8_t>& data,
    const P2PPacketInfo& packet_info,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  NOTREACHED();
  delete this;
}

void P2PSocketTcpServer::SetOption(P2PSocketOption option, int32_t value) {
  // Currently we don't have use case tcp server sockets are used for p2p.
}

}  // namespace network
