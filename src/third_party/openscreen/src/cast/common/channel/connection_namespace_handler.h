// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_CONNECTION_NAMESPACE_HANDLER_H_
#define CAST_COMMON_CHANNEL_CONNECTION_NAMESPACE_HANDLER_H_

#include "cast/common/channel/cast_message_handler.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "util/json/json_serialization.h"

namespace openscreen {
namespace cast {

struct VirtualConnection;
class VirtualConnectionManager;
class VirtualConnectionRouter;

// Handles CastMessages in the connection namespace by opening and closing
// VirtualConnections on the socket on which the messages were received.
class ConnectionNamespaceHandler final : public CastMessageHandler {
 public:
  class VirtualConnectionPolicy {
   public:
    virtual ~VirtualConnectionPolicy() = default;

    virtual bool IsConnectionAllowed(
        const VirtualConnection& virtual_conn) const = 0;
  };

  // Both |vc_manager| and |vc_policy| should outlive this object.
  ConnectionNamespaceHandler(VirtualConnectionManager* vc_manager,
                             VirtualConnectionPolicy* vc_policy);
  ~ConnectionNamespaceHandler() override;

  // CastMessageHandler overrides.
  void OnMessage(VirtualConnectionRouter* router,
                 CastSocket* socket,
                 ::cast::channel::CastMessage message) override;

 private:
  void HandleConnect(VirtualConnectionRouter* router,
                     CastSocket* socket,
                     ::cast::channel::CastMessage message,
                     Json::Value parsed_message);
  void HandleClose(VirtualConnectionRouter* router,
                   CastSocket* socket,
                   ::cast::channel::CastMessage message,
                   Json::Value parsed_message);

  void SendClose(VirtualConnectionRouter* router,
                 VirtualConnection virtual_conn);
  void SendConnectedResponse(VirtualConnectionRouter* router,
                             const VirtualConnection& virtual_conn,
                             int max_protocol_version);

  VirtualConnectionManager* const vc_manager_;
  VirtualConnectionPolicy* const vc_policy_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_CHANNEL_CONNECTION_NAMESPACE_HANDLER_H_
