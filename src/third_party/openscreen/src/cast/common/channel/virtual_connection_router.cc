// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/virtual_connection_router.h"

#include <utility>

#include "cast/common/channel/cast_message_handler.h"
#include "cast/common/channel/message_util.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

using ::cast::channel::CastMessage;

VirtualConnectionRouter::VirtualConnectionRouter(
    VirtualConnectionManager* vc_manager)
    : vc_manager_(vc_manager) {
  OSP_DCHECK(vc_manager);
}

VirtualConnectionRouter::~VirtualConnectionRouter() = default;

bool VirtualConnectionRouter::AddHandlerForLocalId(
    std::string local_id,
    CastMessageHandler* endpoint) {
  return endpoints_.emplace(std::move(local_id), endpoint).second;
}

bool VirtualConnectionRouter::RemoveHandlerForLocalId(
    const std::string& local_id) {
  return endpoints_.erase(local_id) == 1u;
}

void VirtualConnectionRouter::TakeSocket(SocketErrorHandler* error_handler,
                                         std::unique_ptr<CastSocket> socket) {
  int id = socket->socket_id();
  socket->SetClient(this);
  sockets_.emplace(id, SocketWithHandler{std::move(socket), error_handler});
}

void VirtualConnectionRouter::CloseSocket(int id) {
  auto it = sockets_.find(id);
  if (it != sockets_.end()) {
    vc_manager_->RemoveConnectionsBySocketId(
        id, VirtualConnection::kTransportClosed);
    std::unique_ptr<CastSocket> socket = std::move(it->second.socket);
    SocketErrorHandler* error_handler = it->second.error_handler;
    sockets_.erase(it);
    error_handler->OnClose(socket.get());
  }
}

Error VirtualConnectionRouter::Send(VirtualConnection virtual_conn,
                                    CastMessage message) {
  if (virtual_conn.peer_id == kBroadcastId) {
    return BroadcastFromLocalPeer(std::move(virtual_conn.local_id),
                                  std::move(message));
  }

  if (!IsTransportNamespace(message.namespace_()) &&
      !vc_manager_->GetConnectionData(virtual_conn)) {
    return Error::Code::kNoActiveConnection;
  }
  auto it = sockets_.find(virtual_conn.socket_id);
  if (it == sockets_.end()) {
    return Error::Code::kItemNotFound;
  }
  message.set_source_id(std::move(virtual_conn.local_id));
  message.set_destination_id(std::move(virtual_conn.peer_id));
  return it->second.socket->Send(message);
}

Error VirtualConnectionRouter::BroadcastFromLocalPeer(
    std::string local_id,
    ::cast::channel::CastMessage message) {
  message.set_source_id(std::move(local_id));
  message.set_destination_id(kBroadcastId);

  // Broadcast to local endpoints.
  for (const auto& entry : endpoints_) {
    if (entry.first != message.source_id()) {
      entry.second->OnMessage(this, nullptr, message);
    }
  }

  // Broadcast to remote endpoints. If an Error occurs, continue broadcasting,
  // and later return the first Error that occurred.
  Error error;
  for (const auto& entry : sockets_) {
    auto result = entry.second.socket->Send(message);
    if (!result.ok() && error.ok()) {
      error = std::move(result);
    }
  }
  return error;
}

void VirtualConnectionRouter::OnError(CastSocket* socket, Error error) {
  const int id = socket->socket_id();
  auto it = sockets_.find(id);
  if (it != sockets_.end()) {
    vc_manager_->RemoveConnectionsBySocketId(id, VirtualConnection::kUnknown);
    std::unique_ptr<CastSocket> socket_owned = std::move(it->second.socket);
    SocketErrorHandler* error_handler = it->second.error_handler;
    sockets_.erase(it);
    error_handler->OnError(socket, error);
  }
}

void VirtualConnectionRouter::OnMessage(CastSocket* socket,
                                        CastMessage message) {
  OSP_DCHECK(socket);

  const std::string& local_id = message.destination_id();
  if (local_id == kBroadcastId) {
    for (const auto& entry : endpoints_) {
      entry.second->OnMessage(this, socket, message);
    }
  } else {
    if (!IsTransportNamespace(message.namespace_()) &&
        !vc_manager_->GetConnectionData(VirtualConnection{
            local_id, message.source_id(), socket->socket_id()})) {
      return;
    }
    auto it = endpoints_.find(local_id);
    if (it != endpoints_.end()) {
      it->second->OnMessage(this, socket, std::move(message));
    }
  }
}

}  // namespace cast
}  // namespace openscreen
