// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/events/get_updates_response_event.h"

#include "base/strings/stringprintf.h"
#include "sync/protocol/proto_value_conversions.h"

namespace syncer {

GetUpdatesResponseEvent::GetUpdatesResponseEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerResponse& response,
      SyncerError error)
  : timestamp_(timestamp),
    response_(response),
    error_(error) {
}

GetUpdatesResponseEvent::~GetUpdatesResponseEvent() {}

base::Time GetUpdatesResponseEvent::GetTimestamp() const {
  return timestamp_;
}

std::string GetUpdatesResponseEvent::GetType() const {
  return "GetUpdates Response";
}

std::string GetUpdatesResponseEvent::GetDetails() const {
  switch (error_) {
    case SYNCER_OK:
      return base::StringPrintf("Received %d update(s).",
                                response_.get_updates().entries_size());
    case SERVER_MORE_TO_DOWNLOAD:
      return base::StringPrintf("Received %d update(s).  Some updates remain.",
                                response_.get_updates().entries_size());
    default:
      return base::StringPrintf("Received error: %s",
                                GetSyncerErrorString(error_));
  }
}

scoped_ptr<base::DictionaryValue>
GetUpdatesResponseEvent::GetProtoMessage() const {
  return scoped_ptr<base::DictionaryValue>(
      ClientToServerResponseToValue(response_, false));
}

scoped_ptr<ProtocolEvent> GetUpdatesResponseEvent::Clone() const {
  return scoped_ptr<ProtocolEvent>(
      new GetUpdatesResponseEvent(
          timestamp_,
          response_,
          error_));
}

}  // namespace syncer
