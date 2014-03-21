// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_SYNC_ENGINE_EVENT_LISTENER_H_
#define SYNC_ENGINE_SYNC_ENGINE_EVENT_LISTENER_H_

#include "base/time/time.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

struct SyncProtocolError;
struct SyncCycleEvent;
class ProtocolEvent;

class SYNC_EXPORT_PRIVATE SyncEngineEventListener {
 public:
  SyncEngineEventListener();

  // Generated at various points during the sync cycle.
  virtual void OnSyncCycleEvent(const SyncCycleEvent& event) = 0;

  // This event is sent when we receive an actionable error. It is up to
  // the listeners to figure out the action to take using the error sent.
  virtual void OnActionableError(const SyncProtocolError& error) = 0;

  // This event is sent when scheduler decides to wait before next request
  // either because it gets throttled by server or because it backs off after
  // request failure. Retry time is passed in retry_time field of event.
  virtual void OnRetryTimeChanged(base::Time retry_time) = 0;

  // This event is sent when types are throttled or unthrottled.
  virtual void OnThrottledTypesChanged(ModelTypeSet throttled_types) = 0;

  // This event is sent when the server requests a migration.
  virtual void OnMigrationRequested(ModelTypeSet migration_types) = 0;

  // Emits events when sync communicates with the server.
  virtual void OnProtocolEvent(const ProtocolEvent& event) = 0;

 protected:
  virtual ~SyncEngineEventListener();
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNC_ENGINE_EVENT_LISTENER_H_
