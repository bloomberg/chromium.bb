// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SERVER_LOG_ENTRY_HOST_H_
#define REMOTING_HOST_SERVER_LOG_ENTRY_HOST_H_

#include "remoting/host/host_exit_codes.h"
#include "remoting/host/host_status_sender.h"
#include "remoting/protocol/transport.h"

namespace remoting {

class ServerLogEntry;

// Constructs a log entry for a session state change.
// Currently this is either connection or disconnection.
scoped_ptr<ServerLogEntry> MakeLogEntryForSessionStateChange(
    bool connected);

// Constructs a log entry for a heartbeat.
scoped_ptr<ServerLogEntry> MakeLogEntryForHeartbeat();

// Constructs a log entry for a host status message.
scoped_ptr<ServerLogEntry> MakeLogEntryForHostStatus(
    HostStatusSender::HostStatus host_status, HostExitCodes exit_code);

// Adds fields describing the host to this log entry.
void AddHostFieldsToLogEntry(ServerLogEntry* entry);

// Adds a field describing connection type (direct/stun/relay).
void AddConnectionTypeToLogEntry(ServerLogEntry* entry,
                                 protocol::TransportRoute::RouteType type);

}  // namespace remoting

#endif  // REMOTING_HOST_SERVER_LOG_ENTRY_HOST_H_
