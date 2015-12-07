// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_EVENTS_CLEAR_SERVER_DATA_RESPONSE_EVENT_H_
#define SYNC_INTERNAL_API_EVENTS_CLEAR_SERVER_DATA_RESPONSE_EVENT_H_

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/events/protocol_event.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

// An event representing a ClearServerData response event from the server.
class SYNC_EXPORT_PRIVATE ClearServerDataResponseEvent : public ProtocolEvent {
 public:
  ClearServerDataResponseEvent(base::Time timestamp,
                               SyncerError result,
                               const sync_pb::ClientToServerResponse& response);
  ~ClearServerDataResponseEvent() override;

  base::Time GetTimestamp() const override;
  std::string GetType() const override;
  std::string GetDetails() const override;
  scoped_ptr<base::DictionaryValue> GetProtoMessage() const override;
  scoped_ptr<ProtocolEvent> Clone() const override;

  static scoped_ptr<base::DictionaryValue> ToValue(const ProtocolEvent& event);

 private:
  const base::Time timestamp_;
  const SyncerError result_;
  const sync_pb::ClientToServerResponse response_;

  DISALLOW_COPY_AND_ASSIGN(ClearServerDataResponseEvent);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_EVENTS_CLEAR_SERVER_DATA_RESPONSE_EVENT_H_
