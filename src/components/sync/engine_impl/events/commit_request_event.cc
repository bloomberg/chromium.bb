// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/events/commit_request_event.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

CommitRequestEvent::CommitRequestEvent(
    base::Time timestamp,
    size_t num_items,
    ModelTypeSet contributing_types,
    const sync_pb::ClientToServerMessage& request)
    : timestamp_(timestamp),
      num_items_(num_items),
      contributing_types_(contributing_types),
      request_(request) {}

CommitRequestEvent::~CommitRequestEvent() {}

std::unique_ptr<ProtocolEvent> CommitRequestEvent::Clone() const {
  return std::make_unique<CommitRequestEvent>(timestamp_, num_items_,
                                              contributing_types_, request_);
}

base::Time CommitRequestEvent::GetTimestamp() const {
  return timestamp_;
}

std::string CommitRequestEvent::GetType() const {
  return "Commit Request";
}

std::string CommitRequestEvent::GetDetails() const {
  return base::StringPrintf("Item count: %" PRIuS
                            "\n"
                            "Contributing types: %s",
                            num_items_,
                            ModelTypeSetToString(contributing_types_).c_str());
}

std::unique_ptr<base::DictionaryValue> CommitRequestEvent::GetProtoMessage(
    bool include_specifics) const {
  return ClientToServerMessageToValue(request_, include_specifics);
}

}  // namespace syncer
