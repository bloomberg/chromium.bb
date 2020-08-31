// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/virtual_connection_manager.h"

#include <type_traits>

namespace openscreen {
namespace cast {

VirtualConnectionManager::VirtualConnectionManager() = default;

VirtualConnectionManager::~VirtualConnectionManager() = default;

void VirtualConnectionManager::AddConnection(
    VirtualConnection virtual_connection,
    VirtualConnection::AssociatedData associated_data) {
  auto& socket_map = connections_[virtual_connection.socket_id];
  auto local_entries = socket_map.equal_range(virtual_connection.local_id);
  auto it = std::find_if(
      local_entries.first, local_entries.second,
      [&virtual_connection](const std::pair<std::string, VCTail>& entry) {
        return entry.second.peer_id == virtual_connection.peer_id;
      });
  if (it == socket_map.end()) {
    socket_map.emplace(std::move(virtual_connection.local_id),
                       VCTail{std::move(virtual_connection.peer_id),
                              std::move(associated_data)});
  }
}

bool VirtualConnectionManager::RemoveConnection(
    const VirtualConnection& virtual_connection,
    VirtualConnection::CloseReason reason) {
  auto socket_entry = connections_.find(virtual_connection.socket_id);
  if (socket_entry == connections_.end()) {
    return false;
  }

  auto& socket_map = socket_entry->second;
  auto local_entries = socket_map.equal_range(virtual_connection.local_id);
  if (local_entries.first == socket_map.end()) {
    return false;
  }
  for (auto it = local_entries.first; it != local_entries.second; ++it) {
    if (it->second.peer_id == virtual_connection.peer_id) {
      socket_map.erase(it);
      if (socket_map.empty()) {
        connections_.erase(socket_entry);
      }
      return true;
    }
  }
  return false;
}

size_t VirtualConnectionManager::RemoveConnectionsByLocalId(
    const std::string& local_id,
    VirtualConnection::CloseReason reason) {
  size_t removed_count = 0;
  for (auto socket_entry = connections_.begin();
       socket_entry != connections_.end();) {
    auto& socket_map = socket_entry->second;
    auto local_entries = socket_map.equal_range(local_id);
    if (local_entries.first != socket_map.end()) {
      size_t current_count =
          std::distance(local_entries.first, local_entries.second);
      removed_count += current_count;
      socket_map.erase(local_entries.first, local_entries.second);
      if (socket_map.empty()) {
        socket_entry = connections_.erase(socket_entry);
      } else {
        ++socket_entry;
      }
    } else {
      ++socket_entry;
    }
  }
  return removed_count;
}

size_t VirtualConnectionManager::RemoveConnectionsBySocketId(
    int socket_id,
    VirtualConnection::CloseReason reason) {
  auto entry = connections_.find(socket_id);
  if (entry == connections_.end()) {
    return 0;
  }

  size_t removed_count = entry->second.size();
  connections_.erase(entry);

  return removed_count;
}

absl::optional<const VirtualConnection::AssociatedData*>
VirtualConnectionManager::GetConnectionData(
    const VirtualConnection& virtual_connection) const {
  auto socket_entry = connections_.find(virtual_connection.socket_id);
  if (socket_entry == connections_.end()) {
    return absl::nullopt;
  }

  auto& socket_map = socket_entry->second;
  auto local_entries = socket_map.equal_range(virtual_connection.local_id);
  if (local_entries.first == socket_map.end()) {
    return absl::nullopt;
  }
  for (auto it = local_entries.first; it != local_entries.second; ++it) {
    if (it->second.peer_id == virtual_connection.peer_id) {
      return &it->second.data;
    }
  }
  return absl::nullopt;
}

}  // namespace cast
}  // namespace openscreen
