// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_EVENTS_CLEAR_SERVER_DATA_REQUEST_EVENT_H
#define SYNC_INTERNAL_API_EVENTS_CLEAR_SERVER_DATA_REQUEST_EVENT_H

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/events/protocol_event.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

// An event representing a ClearServerData request message sent to the server.
class SYNC_EXPORT_PRIVATE ClearServerDataRequestEvent : public ProtocolEvent {
 public:
  ClearServerDataRequestEvent(base::Time timestamp,
                              const sync_pb::ClientToServerMessage& request);
  ~ClearServerDataRequestEvent() override;

  base::Time GetTimestamp() const override;
  std::string GetType() const override;
  std::string GetDetails() const override;
  scoped_ptr<base::DictionaryValue> GetProtoMessage() const override;
  scoped_ptr<ProtocolEvent> Clone() const override;

  static scoped_ptr<base::DictionaryValue> ToValue(const ProtocolEvent& event);

 private:
  const base::Time timestamp_;
  const sync_pb::ClientToServerMessage request_;

  DISALLOW_COPY_AND_ASSIGN(ClearServerDataRequestEvent);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_EVENTS_CLEAR_SERVER_DATA_REQUEST_EVENT_H
