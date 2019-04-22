// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_EVENTS_NORMAL_GET_UPDATES_REQUEST_EVENT_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_EVENTS_NORMAL_GET_UPDATES_REQUEST_EVENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

class NudgeTracker;

// An event representing a 'normal mode' GetUpdate request to the server.
class NormalGetUpdatesRequestEvent : public ProtocolEvent {
 public:
  NormalGetUpdatesRequestEvent(base::Time timestamp,
                               const NudgeTracker& nudge_tracker,
                               const sync_pb::ClientToServerMessage& request);
  NormalGetUpdatesRequestEvent(base::Time timestamp,
                               ModelTypeSet nudged_types,
                               ModelTypeSet notified_types,
                               ModelTypeSet refresh_requested_types,
                               bool is_retry,
                               sync_pb::ClientToServerMessage request);
  ~NormalGetUpdatesRequestEvent() override;
  std::unique_ptr<ProtocolEvent> Clone() const override;

 private:
  base::Time GetTimestamp() const override;
  std::string GetType() const override;
  std::string GetDetails() const override;
  std::unique_ptr<base::DictionaryValue> GetProtoMessage(
      bool include_specifics) const override;

  const base::Time timestamp_;

  const ModelTypeSet nudged_types_;
  const ModelTypeSet notified_types_;
  const ModelTypeSet refresh_requested_types_;
  const bool is_retry_;

  const sync_pb::ClientToServerMessage request_;

  DISALLOW_COPY_AND_ASSIGN(NormalGetUpdatesRequestEvent);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_EVENTS_NORMAL_GET_UPDATES_REQUEST_EVENT_H_
