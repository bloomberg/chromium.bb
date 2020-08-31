// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_MANAGER_H_
#define CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_MANAGER_H_

#include <cstdint>
#include <map>
#include <string>

#include "absl/types/optional.h"
#include "cast/common/channel/virtual_connection.h"

namespace openscreen {
namespace cast {

// Maintains a collection of open VirtualConnections and associated data.
class VirtualConnectionManager {
 public:
  VirtualConnectionManager();
  ~VirtualConnectionManager();

  void AddConnection(VirtualConnection virtual_connection,
                     VirtualConnection::AssociatedData associated_data);

  // Returns true if a connection matching |virtual_connection| was found and
  // removed.
  bool RemoveConnection(const VirtualConnection& virtual_connection,
                        VirtualConnection::CloseReason reason);

  // Returns the number of connections removed.
  size_t RemoveConnectionsByLocalId(const std::string& local_id,
                                    VirtualConnection::CloseReason reason);
  size_t RemoveConnectionsBySocketId(int socket_id,
                                     VirtualConnection::CloseReason reason);

  // Returns the AssociatedData for |virtual_connection| if a connection exists,
  // nullopt otherwise.  The pointer isn't stable in the long term, so if it
  // actually needs to be stored for later, the caller should make a copy.
  absl::optional<const VirtualConnection::AssociatedData*> GetConnectionData(
      const VirtualConnection& virtual_connection) const;

 private:
  // This struct simply stores the remainder of the data {VirtualConnection,
  // VirtVirtualConnection::AssociatedData} that is not broken up into map keys
  // for |connections_|.
  struct VCTail {
    std::string peer_id;
    VirtualConnection::AssociatedData data;
  };

  std::map<int /* socket_id */,
           std::multimap<std::string /* local_id */, VCTail>>
      connections_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_MANAGER_H_
