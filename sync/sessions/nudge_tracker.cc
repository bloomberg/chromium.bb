// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/nudge_tracker.h"

#include "base/basictypes.h"
#include "sync/internal_api/public/base/invalidation.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/object_id_invalidation_map.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {
namespace sessions {

size_t NudgeTracker::kDefaultMaxPayloadsPerType = 10;

NudgeTracker::NudgeTracker()
    : updates_source_(sync_pb::GetUpdatesCallerInfo::UNKNOWN),
      invalidations_enabled_(false),
      invalidations_out_of_sync_(true) {
  ModelTypeSet protocol_types = ProtocolTypes();
  // Default initialize all the type trackers.
  for (ModelTypeSet::Iterator it = protocol_types.First(); it.Good();
       it.Inc()) {
    invalidation::ObjectId id;
    if (!RealModelTypeToObjectId(it.Get(), &id)) {
      NOTREACHED();
    } else {
      type_trackers_.insert(std::make_pair(it.Get(), DataTypeTracker(id)));
    }
  }
}

NudgeTracker::~NudgeTracker() { }

bool NudgeTracker::IsSyncRequired() const {
  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second.IsSyncRequired()) {
      return true;
    }
  }
  return false;
}

bool NudgeTracker::IsGetUpdatesRequired() const {
  if (invalidations_out_of_sync_)
    return true;

  if (IsRetryRequired())
    return true;

  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second.IsGetUpdatesRequired()) {
      return true;
    }
  }
  return false;
}

bool NudgeTracker::IsRetryRequired() const {
  if (sync_cycle_start_time_.is_null())
    return false;

  if (current_retry_time_.is_null())
    return false;

  return current_retry_time_ < sync_cycle_start_time_;
}

void NudgeTracker::RecordSuccessfulSyncCycle() {
  updates_source_ = sync_pb::GetUpdatesCallerInfo::UNKNOWN;

  // If a retry was required, we've just serviced it.  Unset the flag.
  if (IsRetryRequired())
    current_retry_time_ = base::TimeTicks();

  // A successful cycle while invalidations are enabled puts us back into sync.
  invalidations_out_of_sync_ = !invalidations_enabled_;

  for (TypeTrackerMap::iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    it->second.RecordSuccessfulSyncCycle();
  }
}

void NudgeTracker::RecordLocalChange(ModelTypeSet types) {
  // Don't overwrite an NOTIFICATION or DATATYPE_REFRESH source.  The server
  // makes some assumptions about the source; overriding these sources with
  // LOCAL could lead to incorrect behaviour.  This is part of the reason why
  // we're deprecating 'source' in favor of 'origin'.
  if (updates_source_ != sync_pb::GetUpdatesCallerInfo::NOTIFICATION
      && updates_source_ != sync_pb::GetUpdatesCallerInfo::DATATYPE_REFRESH) {
    updates_source_ = sync_pb::GetUpdatesCallerInfo::LOCAL;
  }

  for (ModelTypeSet::Iterator type_it = types.First(); type_it.Good();
       type_it.Inc()) {
    TypeTrackerMap::iterator tracker_it = type_trackers_.find(type_it.Get());
    DCHECK(tracker_it != type_trackers_.end());
    tracker_it->second.RecordLocalChange();
  }
}

void NudgeTracker::RecordLocalRefreshRequest(ModelTypeSet types) {
  // Don't overwrite an NOTIFICATION source.  The server makes some assumptions
  // about the source.  Overriding this source with LOCAL could lead to
  // incorrect behaviour.  This is part of the reason why we're deprecating
  // 'source' in favor of 'origin'.
  if (updates_source_ != sync_pb::GetUpdatesCallerInfo::NOTIFICATION) {
    updates_source_ = sync_pb::GetUpdatesCallerInfo::DATATYPE_REFRESH;
  }

  for (ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    TypeTrackerMap::iterator tracker_it = type_trackers_.find(it.Get());
    DCHECK(tracker_it != type_trackers_.end());
    tracker_it->second.RecordLocalRefreshRequest();
  }
}

void NudgeTracker::RecordRemoteInvalidation(
    const ObjectIdInvalidationMap& invalidation_map) {
  updates_source_ = sync_pb::GetUpdatesCallerInfo::NOTIFICATION;

  // Be very careful here.  The invalidations acknowledgement system requires a
  // sort of manual memory management.  We'll leak a small amount of memory if
  // we fail to acknowledge or drop any of these incoming invalidations.

  ObjectIdSet id_set = invalidation_map.GetObjectIds();
  for (ObjectIdSet::iterator it = id_set.begin(); it != id_set.end(); ++it) {
    ModelType type;

    // This should never happen.  If it does, we'll start to leak memory.
    if (!ObjectIdToRealModelType(*it, &type)) {
      NOTREACHED()
          << "Object ID " << ObjectIdToString(*it)
          << " does not map to valid model type";
      continue;
    }

    // Forward the invalidations to the proper recipient.
    TypeTrackerMap::iterator tracker_it = type_trackers_.find(type);
    DCHECK(tracker_it != type_trackers_.end());
    tracker_it->second.RecordRemoteInvalidations(
        invalidation_map.ForObject(*it));
  }
}

void NudgeTracker::OnInvalidationsEnabled() {
  invalidations_enabled_ = true;
}

void NudgeTracker::OnInvalidationsDisabled() {
  invalidations_enabled_ = false;
  invalidations_out_of_sync_ = true;
}

void NudgeTracker::SetTypesThrottledUntil(
    ModelTypeSet types,
    base::TimeDelta length,
    base::TimeTicks now) {
  for (ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    TypeTrackerMap::iterator tracker_it = type_trackers_.find(it.Get());
    tracker_it->second.ThrottleType(length, now);
  }
}

void NudgeTracker::UpdateTypeThrottlingState(base::TimeTicks now) {
  for (TypeTrackerMap::iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    it->second.UpdateThrottleState(now);
  }
}

bool NudgeTracker::IsAnyTypeThrottled() const {
  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second.IsThrottled()) {
      return true;
    }
  }
  return false;
}

bool NudgeTracker::IsTypeThrottled(ModelType type) const {
  DCHECK(type_trackers_.find(type) != type_trackers_.end());
  return type_trackers_.find(type)->second.IsThrottled();
}

base::TimeDelta NudgeTracker::GetTimeUntilNextUnthrottle(
    base::TimeTicks now) const {
  DCHECK(IsAnyTypeThrottled()) << "This function requires a pending unthrottle";
  const base::TimeDelta kMaxTimeDelta =
      base::TimeDelta::FromInternalValue(kint64max);

  // Return min of GetTimeUntilUnthrottle() values for all IsThrottled() types.
  base::TimeDelta time_until_next_unthrottle = kMaxTimeDelta;
  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second.IsThrottled()) {
      time_until_next_unthrottle =
          std::min(time_until_next_unthrottle,
                   it->second.GetTimeUntilUnthrottle(now));
    }
  }
  DCHECK(kMaxTimeDelta != time_until_next_unthrottle);

  return time_until_next_unthrottle;
}

ModelTypeSet NudgeTracker::GetThrottledTypes() const {
  ModelTypeSet result;
  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second.IsThrottled()) {
      result.Put(it->first);
    }
  }
  return result;
}

void NudgeTracker::SetLegacyNotificationHint(
    ModelType type,
    sync_pb::DataTypeProgressMarker* progress) const {
  DCHECK(type_trackers_.find(type) != type_trackers_.end());
  type_trackers_.find(type)->second.SetLegacyNotificationHint(progress);
}

sync_pb::GetUpdatesCallerInfo::GetUpdatesSource NudgeTracker::updates_source()
    const {
  return updates_source_;
}

void NudgeTracker::FillProtoMessage(
    ModelType type,
    sync_pb::GetUpdateTriggers* msg) const {
  DCHECK(type_trackers_.find(type) != type_trackers_.end());

  // Fill what we can from the global data.
  msg->set_invalidations_out_of_sync(invalidations_out_of_sync_);

  // Delegate the type-specific work to the DataTypeTracker class.
  type_trackers_.find(type)->second.FillGetUpdatesTriggersMessage(msg);
}

void NudgeTracker::SetSyncCycleStartTime(base::TimeTicks now) {
  sync_cycle_start_time_ = now;

  // If current_retry_time_ is still set, that means we have an old retry time
  // left over from a previous cycle.  For example, maybe we tried to perform
  // this retry, hit a network connection error, and now we're in exponential
  // backoff.  In that case, we want this sync cycle to include the GU retry
  // flag so we leave this variable set regardless of whether or not there is an
  // overwrite pending.
  if (!current_retry_time_.is_null()) {
    return;
  }

  // If do not have a current_retry_time_, but we do have a next_retry_time_ and
  // it is ready to go, then we set it as the current_retry_time_.  It will stay
  // there until a GU retry has succeeded.
  if (!next_retry_time_.is_null() &&
      next_retry_time_ < sync_cycle_start_time_) {
    current_retry_time_ = next_retry_time_;
    next_retry_time_ = base::TimeTicks();
  }
}

void NudgeTracker::SetHintBufferSize(size_t size) {
  for (TypeTrackerMap::iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    it->second.UpdatePayloadBufferSize(size);
  }
}

void NudgeTracker::SetNextRetryTime(base::TimeTicks retry_time) {
  next_retry_time_ = retry_time;
}

}  // namespace sessions
}  // namespace syncer
