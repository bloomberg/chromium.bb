// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/events/poll_get_updates_request_event.h"

#include "sync/protocol/proto_value_conversions.h"

namespace syncer {

PollGetUpdatesRequestEvent::PollGetUpdatesRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request)
  : timestamp_(timestamp),
    request_(request) { }

PollGetUpdatesRequestEvent::~PollGetUpdatesRequestEvent() {}

base::Time PollGetUpdatesRequestEvent::GetTimestamp() const {
  return timestamp_;
}

std::string PollGetUpdatesRequestEvent::GetType() const {
  return "Poll GetUpdate request";
}

std::string PollGetUpdatesRequestEvent::GetDetails() const {
  return std::string();
}

scoped_ptr<base::DictionaryValue>
PollGetUpdatesRequestEvent::GetProtoMessage() const {
  return scoped_ptr<base::DictionaryValue>(
    ClientToServerMessageToValue(request_, false));
}

scoped_ptr<ProtocolEvent> PollGetUpdatesRequestEvent::Clone() const {
  return scoped_ptr<ProtocolEvent>(
      new PollGetUpdatesRequestEvent(
          timestamp_,
          request_));
}

}  // namespace
