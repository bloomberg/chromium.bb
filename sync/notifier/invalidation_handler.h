// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_INVALIDATION_HANDLER_H_
#define SYNC_NOTIFIER_INVALIDATION_HANDLER_H_

#include "sync/base/sync_export.h"
#include "sync/notifier/invalidator_state.h"
#include "sync/notifier/object_id_invalidation_map.h"

namespace syncer {

enum IncomingInvalidationSource {
  // The server is notifying us that one or more objects have stale data.
  REMOTE_INVALIDATION,
  // Something locally is requesting an optimistic refresh of its data.
  LOCAL_INVALIDATION,
};

class SYNC_EXPORT InvalidationHandler {
 public:
  // Called when the invalidator state changes.
  virtual void OnInvalidatorStateChange(InvalidatorState state) = 0;

  // Called when a invalidation is received.  The per-id states are in
  // |id_state_map| and the source is in |source|.  Note that this may be
  // called regardless of the current invalidator state.
  virtual void OnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map,
      IncomingInvalidationSource source) = 0;

 protected:
  virtual ~InvalidationHandler() {}
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATION_HANDLER_H_
