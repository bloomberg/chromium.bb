// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/dropped_invalidation_tracker.h"

#include "sync/internal_api/public/base/invalidation.h"

namespace syncer {

DroppedInvalidationTracker::DroppedInvalidationTracker(
    const invalidation::ObjectId& id)
    : id_(id),
      drop_ack_handle_(AckHandle::InvalidAckHandle()),
      recovering_from_drop_(false) {}

DroppedInvalidationTracker::~DroppedInvalidationTracker() {}

const invalidation::ObjectId& DroppedInvalidationTracker::object_id() const {
  return id_;
}

void DroppedInvalidationTracker::RecordDropEvent(
    WeakHandle<AckHandler> handler, AckHandle handle) {
  drop_ack_handler_ = handler;
  drop_ack_handle_ = handle;
  recovering_from_drop_ = true;
}

void DroppedInvalidationTracker::RecordRecoveryFromDropEvent() {
  if (drop_ack_handler_.IsInitialized()) {
    drop_ack_handler_.Call(FROM_HERE,
                           &AckHandler::Acknowledge,
                           id_,
                           drop_ack_handle_);
  }
  drop_ack_handler_ = syncer::WeakHandle<AckHandler>();
  recovering_from_drop_ = false;
}

bool DroppedInvalidationTracker::IsRecoveringFromDropEvent() const {
  return recovering_from_drop_;
}

}  // namespace syncer
