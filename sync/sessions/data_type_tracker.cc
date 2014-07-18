// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/data_type_tracker.h"

#include "base/logging.h"
#include "sync/internal_api/public/base/invalidation_interface.h"
#include "sync/sessions/nudge_tracker.h"

namespace syncer {
namespace sessions {

DataTypeTracker::DataTypeTracker()
    : local_nudge_count_(0),
      local_refresh_request_count_(0),
      payload_buffer_size_(NudgeTracker::kDefaultMaxPayloadsPerType),
      initial_sync_required_(false) {
}

DataTypeTracker::~DataTypeTracker() { }

void DataTypeTracker::RecordLocalChange() {
  local_nudge_count_++;
}

void DataTypeTracker::RecordLocalRefreshRequest() {
  local_refresh_request_count_++;
}

void DataTypeTracker::RecordRemoteInvalidation(
    scoped_ptr<InvalidationInterface> incoming) {
  DCHECK(incoming);

  // Merge the incoming invalidation into our list of pending invalidations.
  //
  // We won't use STL algorithms here because our concept of equality doesn't
  // quite fit the expectations of set_intersection.  In particular, two
  // invalidations can be equal according to the SingleObjectInvalidationSet's
  // rules (ie. have equal versions), but still have different AckHandle values
  // and need to be acknowledged separately.
  //
  // The invalidations service can only track one outsanding invalidation per
  // type and version, so the acknowledgement here should be redundant.  We'll
  // acknowledge them anyway since it should do no harm, and makes this code a
  // bit easier to test.
  //
  // Overlaps should be extremely rare for most invalidations.  They can happen
  // for unknown version invalidations, though.

  ScopedVector<InvalidationInterface>::iterator it =
      pending_invalidations_.begin();

  // Find the lower bound.
  while (it != pending_invalidations_.end() &&
         InvalidationInterface::LessThanByVersion(**it, *incoming)) {
    it++;
  }

  if (it != pending_invalidations_.end() &&
      !InvalidationInterface::LessThanByVersion(*incoming, **it) &&
      !InvalidationInterface::LessThanByVersion(**it, *incoming)) {
    // Incoming overlaps with existing.  Either both are unknown versions
    // (likely) or these two have the same version number (very unlikely).
    // Acknowledge and overwrite existing.

    // Insert before the existing and get iterator to inserted.
    ScopedVector<InvalidationInterface>::iterator it2 =
        pending_invalidations_.insert(it, incoming.release());

    // Increment that iterator to the old one, then acknowledge and remove it.
    ++it2;
    (*it2)->Acknowledge();
    pending_invalidations_.erase(it2);
  } else {
    // The incoming has a version not in the pending_invalidations_ list.
    // Add it to the list at the proper position.
    pending_invalidations_.insert(it, incoming.release());
  }

  // The incoming invalidation may have caused us to exceed our buffer size.
  // Trim some items from our list, if necessary.
  while (pending_invalidations_.size() > payload_buffer_size_) {
    last_dropped_invalidation_.reset(pending_invalidations_.front());
    last_dropped_invalidation_->Drop();
    pending_invalidations_.weak_erase(pending_invalidations_.begin());
  }
}

void DataTypeTracker::RecordInitialSyncRequired() {
  initial_sync_required_ = true;
}

void DataTypeTracker::RecordSuccessfulSyncCycle() {
  // If we were throttled, then we would have been excluded from this cycle's
  // GetUpdates and Commit actions.  Our state remains unchanged.
  if (IsThrottled())
    return;

  local_nudge_count_ = 0;
  local_refresh_request_count_ = 0;

  // TODO(rlarocque): If we want this to be correct even if we should happen to
  // crash before writing all our state, we should wait until the results of
  // this sync cycle have been written to disk before updating the invalidations
  // state.  See crbug.com/324996.
  for (ScopedVector<InvalidationInterface>::const_iterator it =
           pending_invalidations_.begin();
       it != pending_invalidations_.end();
       ++it) {
    (*it)->Acknowledge();
  }
  pending_invalidations_.clear();

  if (last_dropped_invalidation_) {
    last_dropped_invalidation_->Acknowledge();
    last_dropped_invalidation_.reset();
  }

  initial_sync_required_ = false;
}

// This limit will take effect on all future invalidations received.
void DataTypeTracker::UpdatePayloadBufferSize(size_t new_size) {
  payload_buffer_size_ = new_size;
}

bool DataTypeTracker::IsSyncRequired() const {
  return !IsThrottled() && (HasLocalChangePending() || IsGetUpdatesRequired());
}

bool DataTypeTracker::IsGetUpdatesRequired() const {
  return !IsThrottled() &&
         (HasRefreshRequestPending() || HasPendingInvalidation() ||
          IsInitialSyncRequired());
}

bool DataTypeTracker::HasLocalChangePending() const {
  return local_nudge_count_ > 0;
}

bool DataTypeTracker::HasRefreshRequestPending() const {
  return local_refresh_request_count_ > 0;
}

bool DataTypeTracker::HasPendingInvalidation() const {
  return !pending_invalidations_.empty() || last_dropped_invalidation_;
}

bool DataTypeTracker::IsInitialSyncRequired() const {
  return initial_sync_required_;
}

void DataTypeTracker::SetLegacyNotificationHint(
    sync_pb::DataTypeProgressMarker* progress) const {
  DCHECK(!IsThrottled())
      << "We should not make requests if the type is throttled.";

  if (!pending_invalidations_.empty() &&
      !pending_invalidations_.back()->IsUnknownVersion()) {
    // The old-style source info can contain only one hint per type.  We grab
    // the most recent, to mimic the old coalescing behaviour.
    progress->set_notification_hint(
        pending_invalidations_.back()->GetPayload());
  } else if (HasLocalChangePending()) {
    // The old-style source info sent up an empty string (as opposed to
    // nothing at all) when the type was locally nudged, but had not received
    // any invalidations.
    progress->set_notification_hint(std::string());
  }
}

void DataTypeTracker::FillGetUpdatesTriggersMessage(
    sync_pb::GetUpdateTriggers* msg) const {
  // Fill the list of payloads, if applicable.  The payloads must be ordered
  // oldest to newest, so we insert them in the same order as we've been storing
  // them internally.
  for (ScopedVector<InvalidationInterface>::const_iterator it =
           pending_invalidations_.begin();
       it != pending_invalidations_.end();
       ++it) {
    if (!(*it)->IsUnknownVersion()) {
      msg->add_notification_hint((*it)->GetPayload());
    }
  }

  msg->set_server_dropped_hints(
      !pending_invalidations_.empty() &&
      (*pending_invalidations_.begin())->IsUnknownVersion());
  msg->set_client_dropped_hints(last_dropped_invalidation_);
  msg->set_local_modification_nudges(local_nudge_count_);
  msg->set_datatype_refresh_nudges(local_refresh_request_count_);
  msg->set_initial_sync_in_progress(initial_sync_required_);
}

bool DataTypeTracker::IsThrottled() const {
  return !unthrottle_time_.is_null();
}

base::TimeDelta DataTypeTracker::GetTimeUntilUnthrottle(
    base::TimeTicks now) const {
  if (!IsThrottled()) {
    NOTREACHED();
    return base::TimeDelta::FromSeconds(0);
  }
  return std::max(base::TimeDelta::FromSeconds(0),
                  unthrottle_time_ - now);
}

void DataTypeTracker::ThrottleType(base::TimeDelta duration,
                                      base::TimeTicks now) {
  unthrottle_time_ = std::max(unthrottle_time_, now + duration);
}

void DataTypeTracker::UpdateThrottleState(base::TimeTicks now) {
  if (now >= unthrottle_time_) {
    unthrottle_time_ = base::TimeTicks();
  }
}

}  // namespace sessions
}  // namespace syncer
