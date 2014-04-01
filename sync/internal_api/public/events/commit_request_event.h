// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_EVENTS_COMMIT_REQUEST_EVENT_H
#define SYNC_INTERNAL_API_EVENTS_COMMIT_REQUEST_EVENT_H

#include <cstddef>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/events/protocol_event.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

// An event representing a commit request message sent to the server.
class SYNC_EXPORT_PRIVATE CommitRequestEvent : public ProtocolEvent {
 public:
  CommitRequestEvent(
      base::Time timestamp,
      size_t num_items,
      ModelTypeSet contributing_types,
      const sync_pb::ClientToServerMessage& request);
  virtual ~CommitRequestEvent();

  virtual base::Time GetTimestamp() const OVERRIDE;
  virtual std::string GetType() const OVERRIDE;
  virtual std::string GetDetails() const OVERRIDE;
  virtual scoped_ptr<base::DictionaryValue> GetProtoMessage() const OVERRIDE;
  virtual scoped_ptr<ProtocolEvent> Clone() const OVERRIDE;

  static scoped_ptr<base::DictionaryValue> ToValue(
      const ProtocolEvent& event);

 private:
  const base::Time timestamp_;
  const size_t num_items_;
  const ModelTypeSet contributing_types_;
  const sync_pb::ClientToServerMessage request_;

  DISALLOW_COPY_AND_ASSIGN(CommitRequestEvent);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_EVENTS_COMMIT_REQUEST_EVENT_H
