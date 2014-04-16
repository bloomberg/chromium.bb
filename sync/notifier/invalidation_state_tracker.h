// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An InvalidationStateTracker is an interface that handles persisting state
// needed for invalidations. Currently, it is responsible for managing the
// following information:
// - Max version seen from the invalidation server to help dedupe invalidations.
// - Bootstrap data for the invalidation client.
// - Payloads and locally generated ack handles, to support local acking.

#ifndef SYNC_NOTIFIER_INVALIDATION_STATE_TRACKER_H_
#define SYNC_NOTIFIER_INVALIDATION_STATE_TRACKER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "google/cacheinvalidation/include/types.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/invalidation.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/unacked_invalidation_set.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace syncer {

class SYNC_EXPORT InvalidationStateTracker {
 public:
  InvalidationStateTracker() {}
  virtual ~InvalidationStateTracker() {}

  // The per-client unique ID used to register the invalidation client with the
  // server.  This is used to squelch invalidation notifications that originate
  // from changes made by this client.  Setting the client ID clears all other
  // state.
  virtual void ClearAndSetNewClientId(const std::string& data) = 0;
  virtual std::string GetInvalidatorClientId() const = 0;

  // Used by invalidation::InvalidationClient for persistence. |data| is an
  // opaque blob that an invalidation client can use after a restart to
  // bootstrap itself. |data| is binary data (not valid UTF8, embedded nulls,
  // etc).
  virtual void SetBootstrapData(const std::string& data) = 0;
  virtual std::string GetBootstrapData() const = 0;

  // Used to store invalidations that have been acked to the server, but not yet
  // handled by our clients.  We store these invalidations on disk so we won't
  // lose them if we need to restart.
  virtual void SetSavedInvalidations(const UnackedInvalidationsMap& states) = 0;
  virtual UnackedInvalidationsMap GetSavedInvalidations() const = 0;

  // Erases invalidation versions, client ID, and state stored on disk.
  virtual void Clear() = 0;
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATION_STATE_TRACKER_H_
