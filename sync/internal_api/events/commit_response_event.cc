// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/events/commit_response_event.h"

#include "base/strings/stringprintf.h"
#include "sync/protocol/proto_value_conversions.h"

namespace syncer {

CommitResponseEvent::CommitResponseEvent(
    base::Time timestamp,
    SyncerError result,
    const sync_pb::ClientToServerResponse& response)
  : timestamp_(timestamp),
    result_(result),
    response_(response) {}

CommitResponseEvent::~CommitResponseEvent() {}

base::Time CommitResponseEvent::GetTimestamp() const {
  return timestamp_;
}

std::string CommitResponseEvent::GetType() const {
  return "Commit Response";
}

std::string CommitResponseEvent::GetDetails() const {
  return base::StringPrintf("Result: %s", GetSyncerErrorString(result_));
}

scoped_ptr<base::DictionaryValue> CommitResponseEvent::GetProtoMessage() const {
  return scoped_ptr<base::DictionaryValue>(
      ClientToServerResponseToValue(response_, false));
}

scoped_ptr<ProtocolEvent> CommitResponseEvent::Clone() const {
  return scoped_ptr<ProtocolEvent>(
      new CommitResponseEvent(
          timestamp_,
          result_,
          response_));
}

}  // namespace syncer
