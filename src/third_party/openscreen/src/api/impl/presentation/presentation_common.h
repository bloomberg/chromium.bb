// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_PRESENTATION_PRESENTATION_COMMON_H_
#define API_IMPL_PRESENTATION_PRESENTATION_COMMON_H_

#include <algorithm>
#include <memory>

#include "api/public/message_demuxer.h"
#include "api/public/network_service_manager.h"
#include "api/public/protocol_connection_server.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"
#include "platform/api/time.h"

namespace openscreen {
namespace presentation {

// This method asks the singleton NetworkServiceManager
// to create a new protocol connection for the given endpoint.
// Typically, the same QuicConnection and ServiceConnectionDelegate
// are reused (see QuicProtocolConnection::FromExisting) for the new
// ProtocolConnection instance.
std::unique_ptr<ProtocolConnection> GetProtocolConnection(uint64_t endpoint_id);

// These methods retrieve the server and client demuxers, respectively. These
// are retrieved from the protocol connection server and client. The lifetime of
// the demuxers themselves are not well defined: currently they are created in
// the demo component for the ListenerDemo and PublisherDemo methods.
MessageDemuxer* GetServerDemuxer();
MessageDemuxer* GetClientDemuxer();

class PresentationID {
 public:
  PresentationID(const std::string presentation_id);

  operator bool() { return id_; }
  operator std::string() { return id_.value(); }

 private:
  ErrorOr<std::string> id_;
};

}  // namespace presentation
}  // namespace openscreen

#endif  // API_IMPL_PRESENTATION_PRESENTATION_COMMON_H_