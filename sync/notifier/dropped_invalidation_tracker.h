// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_DROPPED_INVALIDATION_TRACKER_H_
#define SYNC_NOTIFIER_DROPPED_INVALIDATION_TRACKER_H_

#include "google/cacheinvalidation/include/types.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/ack_handle.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/ack_handler.h"

namespace syncer {

class Invalidation;

// Helps InvalidationHandlers keep track of dropped invalidations for a given
// ObjectId.
//
// The intent of this class is to hide some of the implementation details around
// how the invalidations system manages dropping and drop recovery.  Any
// invalidation handler that intends to buffer and occasionally drop
// invalidations should keep one instance of it per registered ObjectId.
//
// When an invalidation handler wishes to drop an invalidation, it must provide
// an instance of this class to that Invalidation's Drop() method.  In order to
// indicate recovery from a drop, the handler can call this class'
// RecordRecoveryFromDropEvent().
//
// Copy and assign are allowed for this class so we can use it in STL
// containers.
class SYNC_EXPORT DroppedInvalidationTracker {
 public:
  explicit DroppedInvalidationTracker(const invalidation::ObjectId& id);
  ~DroppedInvalidationTracker();

  const invalidation::ObjectId& object_id() const;

  // Called by Invalidation::Drop() to keep track of a drop event.
  //
  // Takes ownership of the internals belonging to a soon to be discarded
  // dropped invalidation.  See also the comment for this class'
  // |drop_ack_handler_| member.
  void RecordDropEvent(WeakHandle<AckHandler> handler, AckHandle handle);

  // Returns true if we're still recovering from a drop event.
  bool IsRecoveringFromDropEvent() const;

  // Called by the InvalidationHandler when it recovers from the drop event.
  void RecordRecoveryFromDropEvent();

 private:
  invalidation::ObjectId id_;
  AckHandle drop_ack_handle_;

  // This flag is set to true when we have dropped an invalidation and have not
  // yet recovered from this drop event.  Note that this may not always coincide
  // with drop_ack_handler_ being initialized because a null AckHandler could be
  // passed in to RecordDropEvent().
  bool recovering_from_drop_;

  // A WeakHandle to the enitity responsible for persisting invalidation
  // acknowledgement state on disk.  We can get away with using a WeakHandle
  // because we don't care if our drop recovery message doesn't gets delivered
  // in some shutdown cases.  If that happens, we'll have to process the
  // invalidation state again on the next restart.  It would be a waste of time
  // and resources, but otherwise not particularly harmful.
  WeakHandle<AckHandler> drop_ack_handler_;
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_DROPPED_INVALIDATION_TRACKER_H_
