// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/events/commit_request_event.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "sync/protocol/proto_value_conversions.h"

namespace syncer {

CommitRequestEvent::CommitRequestEvent(
    base::Time timestamp,
    size_t num_items,
    ModelTypeSet contributing_types_,
    const sync_pb::ClientToServerMessage& request)
  : timestamp_(timestamp),
    num_items_(num_items),
    contributing_types_(contributing_types_),
    request_(request) {}

CommitRequestEvent::~CommitRequestEvent() {}

base::Time CommitRequestEvent::GetTimestamp() const {
  return timestamp_;
}

std::string CommitRequestEvent::GetType() const {
  return "Commit Request";
}

std::string CommitRequestEvent::GetDetails() const {
  return base::StringPrintf(
      "Item count: %" PRIuS "\n"
      "Contributing types: %s",
      num_items_,
      ModelTypeSetToString(contributing_types_).c_str());
}

scoped_ptr<base::DictionaryValue> CommitRequestEvent::GetProtoMessage() const {
  return scoped_ptr<base::DictionaryValue>(
      ClientToServerMessageToValue(request_, false));
}

scoped_ptr<ProtocolEvent> CommitRequestEvent::Clone() const {
  return scoped_ptr<ProtocolEvent>(
      new CommitRequestEvent(timestamp_,
                             num_items_,
                             contributing_types_,
                             request_));
}

}  // namespace syncer
