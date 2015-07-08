// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/events/clear_server_data_response_event.h"

#include "base/strings/stringprintf.h"
#include "sync/protocol/proto_value_conversions.h"

namespace syncer {

ClearServerDataResponseEvent::ClearServerDataResponseEvent(
    base::Time timestamp,
    SyncerError result,
    const sync_pb::ClientToServerResponse& response)
    : timestamp_(timestamp), result_(result), response_(response) {}

ClearServerDataResponseEvent::~ClearServerDataResponseEvent() {}

base::Time ClearServerDataResponseEvent::GetTimestamp() const {
  return timestamp_;
}

std::string ClearServerDataResponseEvent::GetType() const {
  return "ClearServerData Response";
}

std::string ClearServerDataResponseEvent::GetDetails() const {
  return base::StringPrintf("Result: %s", GetSyncerErrorString(result_));
}

scoped_ptr<base::DictionaryValue>
ClearServerDataResponseEvent::GetProtoMessage() const {
  return scoped_ptr<base::DictionaryValue>(
      ClientToServerResponseToValue(response_, false));
}

scoped_ptr<ProtocolEvent> ClearServerDataResponseEvent::Clone() const {
  return scoped_ptr<ProtocolEvent>(
      new ClearServerDataResponseEvent(timestamp_, result_, response_));
}

}  // namespace syncer
