// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_EVENTS_COMMIT_RESPONSE_EVENT_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_EVENTS_COMMIT_RESPONSE_EVENT_H_

#include <cstddef>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/syncer_error.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

// An event representing a commit response event from the server.
class CommitResponseEvent : public ProtocolEvent {
 public:
  CommitResponseEvent(base::Time timestamp,
                      SyncerError result,
                      const sync_pb::ClientToServerResponse& response);
  ~CommitResponseEvent() override;
  std::unique_ptr<ProtocolEvent> Clone() const override;

 private:
  base::Time GetTimestamp() const override;
  std::string GetType() const override;
  std::string GetDetails() const override;
  std::unique_ptr<base::DictionaryValue> GetProtoMessage(
      bool include_specifics) const override;

  static std::unique_ptr<base::DictionaryValue> ToValue(
      const ProtocolEvent& event);

  const base::Time timestamp_;
  const SyncerError result_;
  const sync_pb::ClientToServerResponse response_;

  DISALLOW_COPY_AND_ASSIGN(CommitResponseEvent);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_EVENTS_COMMIT_RESPONSE_EVENT_H_
