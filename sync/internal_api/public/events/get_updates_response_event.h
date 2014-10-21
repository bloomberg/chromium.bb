// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_EVENTS_GET_UPDATES_RESPONSE_EVENT_H
#define SYNC_INTERNAL_API_EVENTS_GET_UPDATES_RESPONSE_EVENT_H

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/events/protocol_event.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

// An event representing a GetUpdates response event from the server.
//
// Unlike the events for the request message, the response events are generic
// and do not vary for each type of GetUpdate cycle.
class SYNC_EXPORT_PRIVATE GetUpdatesResponseEvent : public ProtocolEvent {
 public:
  GetUpdatesResponseEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerResponse& response,
      SyncerError error);

  ~GetUpdatesResponseEvent() override;

  base::Time GetTimestamp() const override;
  std::string GetType() const override;
  std::string GetDetails() const override;
  scoped_ptr<base::DictionaryValue> GetProtoMessage() const override;
  scoped_ptr<ProtocolEvent> Clone() const override;

 private:
  const base::Time timestamp_;
  const sync_pb::ClientToServerResponse response_;
  const SyncerError error_;

  DISALLOW_COPY_AND_ASSIGN(GetUpdatesResponseEvent);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_EVENTS_GET_UPDATES_RESPONSE_EVENT_H
