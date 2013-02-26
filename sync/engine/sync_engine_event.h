// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_SYNC_ENGINE_EVENT_H_
#define SYNC_ENGINE_SYNC_ENGINE_EVENT_H_

#include <string>

#include "base/observer_list.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"

namespace syncable {
class Id;
}

namespace syncer {

struct SYNC_EXPORT_PRIVATE SyncEngineEvent {
  enum EventCause {
    ////////////////////////////////////////////////////////////////
    // Sent on entry of Syncer state machine
    SYNC_CYCLE_BEGIN,

    // SyncerCommand generated events.
    STATUS_CHANGED,

    // We have reached the SYNCER_END state in the main sync loop.
    SYNC_CYCLE_ENDED,

    ////////////////////////////////////////////////////////////////
    // Generated in response to specific protocol actions or events.

    // New token in updated_token.
    UPDATED_TOKEN,

    // This is sent after the Syncer (and SyncerThread) have initiated self
    // halt due to no longer being permitted to communicate with the server.
    // The listener should sever the sync / browser connections and delete sync
    // data (i.e. as if the user clicked 'Stop Syncing' in the browser.
    STOP_SYNCING_PERMANENTLY,

    // This event is sent when we receive an actionable error. It is upto
    // the listeners to figure out the action to take using the snapshot sent.
    ACTIONABLE_ERROR,

    // This event is sent when scheduler decides to wait before next request
    // either because it gets throttled by server or because it backs off after
    // request failure. Retry time is passed in retry_time field of event.
    RETRY_TIME_CHANGED,
  };

  explicit SyncEngineEvent(EventCause cause);
  ~SyncEngineEvent();

  EventCause what_happened;

  // The last session used for syncing.
  sessions::SyncSessionSnapshot snapshot;

  // Update-Client-Auth returns a new token for sync use.
  std::string updated_token;

  // Time when scheduler will try to send request after backoff
  base::Time retry_time;
};

class SYNC_EXPORT_PRIVATE SyncEngineEventListener {
 public:
  // TODO(tim): Consider splitting this up to multiple callbacks, rather than
  // have to do Event e(type); OnSyncEngineEvent(e); at all callsites,
  virtual void OnSyncEngineEvent(const SyncEngineEvent& event) = 0;
 protected:
  virtual ~SyncEngineEventListener() {}
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNC_ENGINE_EVENT_H_
