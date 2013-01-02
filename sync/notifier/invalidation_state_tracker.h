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

namespace base {
class TaskRunner;
}  // namespace base

namespace syncer {

struct SYNC_EXPORT InvalidationState {
  InvalidationState();
  ~InvalidationState();

  int64 version;
  std::string payload;
  AckHandle expected;
  AckHandle current;
};

// TODO(dcheng): Remove this in favor of adding an Equals() method.
SYNC_EXPORT_PRIVATE bool operator==(const InvalidationState& lhs,
                                    const InvalidationState& rhs);

typedef std::map<invalidation::ObjectId, InvalidationState, ObjectIdLessThan>
    InvalidationStateMap;
typedef std::map<invalidation::ObjectId, AckHandle, ObjectIdLessThan>
    AckHandleMap;

class InvalidationStateTracker {
 public:
  InvalidationStateTracker() {}

  virtual InvalidationStateMap GetAllInvalidationStates() const = 0;

  // |max_version| should be strictly greater than any existing max
  // version for |model_type|.
  virtual void SetMaxVersionAndPayload(const invalidation::ObjectId& id,
                                       int64 max_version,
                                       const std::string& payload) = 0;
  // Removes all state tracked for |ids|.
  virtual void Forget(const ObjectIdSet& ids) = 0;

  // Used by invalidation::InvalidationClient for persistence. |data| is an
  // opaque blob that an invalidation client can use after a restart to
  // bootstrap itself. |data| is binary data (not valid UTF8, embedded nulls,
  // etc).
  virtual void SetBootstrapData(const std::string& data) = 0;
  virtual std::string GetBootstrapData() const = 0;

  // Used for generating our own local ack handles. Generates a new ack handle
  // for each object id in |ids|. The result is returned via |callback| posted
  // to |task_runner|.
  virtual void GenerateAckHandles(
      const ObjectIdSet& ids,
      const scoped_refptr<base::TaskRunner>& task_runner,
      base::Callback<void(const AckHandleMap&)> callback) = 0;

  // Records an acknowledgement for |id|. Note that no attempt at ordering is
  // made. Acknowledge() only records the last ack_handle it received, even if
  // the last ack_handle it received was generated before the value currently
  // recorded.
  virtual void Acknowledge(const invalidation::ObjectId& id,
                           const AckHandle& ack_handle) = 0;

 protected:
  virtual ~InvalidationStateTracker() {}
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATION_STATE_TRACKER_H_
