// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An InvalidationVersionTracker is an interface that handles getting
// and setting (persisting) max invalidation versions.

#ifndef SYNC_NOTIFIER_INVALIDATION_STATE_TRACKER_H_
#define SYNC_NOTIFIER_INVALIDATION_STATE_TRACKER_H_

#include <map>

#include "base/basictypes.h"
#include "google/cacheinvalidation/include/types.h"
#include "sync/notifier/invalidation_util.h"

namespace syncer {

struct InvalidationState {
  int64 version;
};

// TODO(dcheng): Remove this in favor of adding an Equals() method.
bool operator==(const InvalidationState& lhs, const InvalidationState& rhs);

typedef std::map<invalidation::ObjectId, InvalidationState, ObjectIdLessThan>
    InvalidationStateMap;

class InvalidationStateTracker {
 public:
  InvalidationStateTracker() {}

  virtual InvalidationStateMap GetAllInvalidationStates() const = 0;

  // |max_version| should be strictly greater than any existing max
  // version for |model_type|.
  virtual void SetMaxVersion(const invalidation::ObjectId& id,
                             int64 max_version) = 0;
  // Removes all state tracked for |ids|.
  virtual void Forget(const ObjectIdSet& ids) = 0;

  // Used by invalidation::InvalidationClient for persistence. |data| is an
  // opaque blob that an invalidation client can use after a restart to
  // bootstrap itself. |data| is binary data (not valid UTF8, embedded nulls,
  // etc).
  virtual void SetBootstrapData(const std::string& data) = 0;
  virtual std::string GetBootstrapData() const = 0;

 protected:
  virtual ~InvalidationStateTracker() {}
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATION_STATE_TRACKER_H_
