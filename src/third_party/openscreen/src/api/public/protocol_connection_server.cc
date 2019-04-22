// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/protocol_connection_server.h"

namespace openscreen {

ProtocolConnectionServer::ProtocolConnectionServer(MessageDemuxer* demuxer,
                                                   Observer* observer)
    : demuxer_(demuxer), observer_(observer) {}

ProtocolConnectionServer::~ProtocolConnectionServer() = default;

std::ostream& operator<<(std::ostream& os,
                         ProtocolConnectionServer::State state) {
  switch (state) {
    case ProtocolConnectionServer::State::kStopped:
      return os << "STOPPED";
    case ProtocolConnectionServer::State::kStarting:
      return os << "STARTING";
    case ProtocolConnectionServer::State::kRunning:
      return os << "RUNNING";
    case ProtocolConnectionServer::State::kStopping:
      return os << "STOPPING";
    case ProtocolConnectionServer::State::kSuspended:
      return os << "SUSPENDED";
    default:
      return os << "UNKNOWN";
  }
}

}  // namespace openscreen
