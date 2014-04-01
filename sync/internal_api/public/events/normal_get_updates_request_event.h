// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_EVENTS_NORMAL_GET_UPDATES_REQUEST_H
#define SYNC_INTERNAL_API_EVENTS_NORMAL_GET_UPDATES_REQUEST_H

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/events/protocol_event.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

namespace sessions {
class NudgeTracker;
}  // namespace sessions

// An event representing a 'normal mode' GetUpdate request to the server.
class SYNC_EXPORT_PRIVATE NormalGetUpdatesRequestEvent : public ProtocolEvent {
 public:
  NormalGetUpdatesRequestEvent(
      base::Time timestamp,
      const sessions::NudgeTracker& nudge_tracker,
      const sync_pb::ClientToServerMessage& request);

  virtual ~NormalGetUpdatesRequestEvent();

  virtual base::Time GetTimestamp() const OVERRIDE;
  virtual std::string GetType() const OVERRIDE;
  virtual std::string GetDetails() const OVERRIDE;
  virtual scoped_ptr<base::DictionaryValue> GetProtoMessage() const OVERRIDE;
  virtual scoped_ptr<ProtocolEvent> Clone() const OVERRIDE;

 private:
  NormalGetUpdatesRequestEvent(
      base::Time timestamp,
      ModelTypeSet nudged_types,
      ModelTypeSet notified_types,
      ModelTypeSet refresh_requested_types,
      bool is_retry,
      sync_pb::ClientToServerMessage request);

  const base::Time timestamp_;

  const ModelTypeSet nudged_types_;
  const ModelTypeSet notified_types_;
  const ModelTypeSet refresh_requested_types_;
  const bool is_retry_;

  const sync_pb::ClientToServerMessage request_;

  DISALLOW_COPY_AND_ASSIGN(NormalGetUpdatesRequestEvent);
};

}

#endif  // SYNC_INTERNAL_API_EVENTS_NORMAL_GET_UPDATES_REQUEST_H
