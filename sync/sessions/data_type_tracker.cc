// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/data_type_tracker.h"

#include "base/logging.h"
#include "sync/sessions/nudge_tracker.h"

namespace syncer {
namespace sessions {

DataTypeTracker::DataTypeTracker()
  : local_nudge_count_(0),
    local_refresh_request_count_(0),
    local_payload_overflow_(false),
    server_payload_overflow_(false),
    payload_buffer_size_(NudgeTracker::kDefaultMaxPayloadsPerType) { }

DataTypeTracker::~DataTypeTracker() { }

void DataTypeTracker::RecordLocalChange() {
  local_nudge_count_++;
}

void DataTypeTracker::RecordLocalRefreshRequest() {
  local_refresh_request_count_++;
}

void DataTypeTracker::RecordRemoteInvalidation(
    const std::string& payload) {
  pending_payloads_.push_back(payload);
  if (pending_payloads_.size() > payload_buffer_size_) {
    // Drop the oldest payload if we've overflowed.
    pending_payloads_.pop_front();
    local_payload_overflow_ = true;
  }
}

void DataTypeTracker::RecordSuccessfulSyncCycle() {
  // If we were throttled, then we would have been excluded from this cycle's
  // GetUpdates and Commit actions.  Our state remains unchanged.
  if (IsThrottled())
    return;

  local_nudge_count_ = 0;
  local_refresh_request_count_ = 0;
  pending_payloads_.clear();
  local_payload_overflow_ = false;
  server_payload_overflow_ = false;
}

// This limit will take effect on all future invalidations received.
void DataTypeTracker::UpdatePayloadBufferSize(size_t new_size) {
  payload_buffer_size_ = new_size;
}

bool DataTypeTracker::IsSyncRequired() const {
  return !IsThrottled() &&
      (local_nudge_count_ > 0 ||
       local_refresh_request_count_ > 0 ||
       HasPendingInvalidation() ||
       local_payload_overflow_ ||
       server_payload_overflow_);
}

bool DataTypeTracker::IsGetUpdatesRequired() const {
  return !IsThrottled() &&
      (local_refresh_request_count_ > 0 ||
       HasPendingInvalidation() ||
       local_payload_overflow_ ||
       server_payload_overflow_);
}

bool DataTypeTracker::HasLocalChangePending() const {
  return local_nudge_count_ > 0;
}

bool DataTypeTracker::HasPendingInvalidation() const {
  return !pending_payloads_.empty();
}

std::string DataTypeTracker::GetMostRecentInvalidationPayload() const {
  return pending_payloads_.back();
}

void DataTypeTracker::SetLegacyNotificationHint(
    sync_pb::DataTypeProgressMarker* progress) const {
  DCHECK(!IsThrottled())
      << "We should not make requests if the type is throttled.";

  if (HasPendingInvalidation()) {
    // The old-style source info can contain only one hint per type.  We grab
    // the most recent, to mimic the old coalescing behaviour.
    progress->set_notification_hint(GetMostRecentInvalidationPayload());
  } else if (HasLocalChangePending()) {
    // The old-style source info sent up an empty string (as opposed to
    // nothing at all) when the type was locally nudged, but had not received
    // any invalidations.
    progress->set_notification_hint("");
  }
}

void DataTypeTracker::FillGetUpdatesTriggersMessage(
    sync_pb::GetUpdateTriggers* msg) const {
  // Fill the list of payloads, if applicable.  The payloads must be ordered
  // oldest to newest, so we insert them in the same order as we've been storing
  // them internally.
  for (PayloadList::const_iterator payload_it = pending_payloads_.begin();
       payload_it != pending_payloads_.end(); ++payload_it) {
    msg->add_notification_hint(*payload_it);
  }

  msg->set_client_dropped_hints(local_payload_overflow_);
  msg->set_local_modification_nudges(local_nudge_count_);
  msg->set_datatype_refresh_nudges(local_refresh_request_count_);

  // TODO(rlarocque): Support Tango trickles.  See crbug.com/223437.
  // msg->set_server_dropped_hints(server_payload_oveflow_);
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
