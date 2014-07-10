// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_INVALIDATION_HANDLER_H_
#define SYNC_NOTIFIER_INVALIDATION_HANDLER_H_

#include <string>

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/invalidator_state.h"

namespace syncer {

class ObjectIdInvalidationMap;

class SYNC_EXPORT InvalidationHandler {
 public:
  InvalidationHandler();

  // Called when the invalidator state changes.
  virtual void OnInvalidatorStateChange(InvalidatorState state) = 0;

  // Called when a invalidation is received.  The per-id states are in
  // |id_state_map| and the source is in |source|.  Note that this may be
  // called regardless of the current invalidator state.
  virtual void OnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) = 0;

  virtual std::string GetOwnerName() const = 0;

 protected:
  virtual ~InvalidationHandler();
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATION_HANDLER_H_
