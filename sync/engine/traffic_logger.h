// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has the functions to log all the sync related HTTP communication.
// To get the log run a debug build of chrome with the flag
// --vmodule=traffic_logger=1.

#ifndef CHROME_BROWSER_SYNC_ENGINE_TRAFFIC_LOGGER_H_
#define CHROME_BROWSER_SYNC_ENGINE_TRAFFIC_LOGGER_H_

namespace sync_pb {
class ClientToServerResponse;
class ClientToServerMessage;
}  // namespace sync_pb

namespace syncer {

void LogClientToServerMessage(const sync_pb::ClientToServerMessage& msg);
void LogClientToServerResponse(
    const sync_pb::ClientToServerResponse& response);

}  // namespace syncer

#endif  // CHROME_BROWSER_SYNC_ENGINE_TRAFFIC_LOGGER_H_
