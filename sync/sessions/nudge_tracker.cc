// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/nudge_tracker.h"

#include "sync/internal_api/public/base/invalidation.h"
#include "sync/internal_api/public/sessions/sync_source_info.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {
namespace sessions {

size_t NudgeTracker::kDefaultMaxPayloadsPerType = 10;

NudgeTracker::NudgeTracker()
    : updates_source_(sync_pb::GetUpdatesCallerInfo::UNKNOWN),
      invalidations_enabled_(false),
      invalidations_out_of_sync_(true),
      num_payloads_per_type_(kDefaultMaxPayloadsPerType) { }

NudgeTracker::~NudgeTracker() { }

bool NudgeTracker::IsGetUpdatesRequired() {
  if (!refresh_requested_counts_.empty()) {
    return true;
  } else if (!payload_list_map_.empty()) {
    return true;
  } else {
    return false;
  }
}

bool NudgeTracker::IsSyncRequired() {
  if (!local_nudge_counts_.empty()) {
    return true;
  } else if (IsGetUpdatesRequired()) {
    return true;
  } else {
    return false;
  }
}

void NudgeTracker::RecordSuccessfulSyncCycle() {
  updates_source_ = sync_pb::GetUpdatesCallerInfo::UNKNOWN;
  local_nudge_counts_.clear();
  refresh_requested_counts_.clear();
  payload_list_map_.clear();
  locally_dropped_payload_types_.Clear();
  server_dropped_payload_types_.Clear();

  // A successful cycle while invalidations are enabled puts us back into sync.
  invalidations_out_of_sync_ = !invalidations_enabled_;
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

  for (ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    local_nudge_counts_[it.Get()]++;
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
    refresh_requested_counts_[it.Get()]++;
  }
}

void NudgeTracker::RecordRemoteInvalidation(
    const ModelTypeInvalidationMap& invalidation_map) {
  updates_source_ = sync_pb::GetUpdatesCallerInfo::NOTIFICATION;

  for (ModelTypeInvalidationMap::const_iterator i = invalidation_map.begin();
       i != invalidation_map.end(); ++i) {
    const ModelType type = i->first;
    const std::string& payload = i->second.payload;

    payload_list_map_[type].push_back(payload);

    // Respect the size limits on locally queued invalidation hints.
    if (payload_list_map_[type].size() > num_payloads_per_type_) {
      payload_list_map_[type].pop_front();
      locally_dropped_payload_types_.Put(type);
    }
  }
}

void NudgeTracker::OnInvalidationsEnabled() {
  invalidations_enabled_ = true;
}

void NudgeTracker::OnInvalidationsDisabled() {
  invalidations_enabled_ = false;
  invalidations_out_of_sync_ = true;
}

SyncSourceInfo NudgeTracker::GetSourceInfo() const {
  // The old-style source added an empty hint for all locally nudge types.
  // This will be overwritten with the proper hint for any types that were both
  // locally nudged and invalidated.
  ModelTypeSet nudged_types;
  for (NudgeMap::const_iterator it = local_nudge_counts_.begin();
       it != local_nudge_counts_.end(); ++it) {
    nudged_types.Put(it->first);
  }
  ModelTypeInvalidationMap invalidation_map =
      ModelTypeSetToInvalidationMap(nudged_types, std::string());

  // The old-style source info can contain only one hint per type.  We grab the
  // most recent, to mimic the old coalescing behaviour.
  for (PayloadListMap::const_iterator it = payload_list_map_.begin();
       it != payload_list_map_.end(); ++it) {
    ModelType type = it->first;
    const PayloadList& list = it->second;

    if (!list.empty()) {
      Invalidation invalidation;
      invalidation.payload = list.back();
      if (invalidation_map.find(type) != invalidation_map.end()) {
        invalidation_map[type] = invalidation;
      } else {
        invalidation_map.insert(std::make_pair(type, invalidation));
      }
    }
  }

  return SyncSourceInfo(updates_source_, invalidation_map);
}

ModelTypeSet NudgeTracker::GetLocallyModifiedTypes() const {
  ModelTypeSet nudged_types;
  for (NudgeMap::const_iterator it = local_nudge_counts_.begin();
       it != local_nudge_counts_.end(); ++it) {
    nudged_types.Put(it->first);
  }
  return nudged_types;
}

sync_pb::GetUpdatesCallerInfo::GetUpdatesSource NudgeTracker::updates_source()
    const {
  return updates_source_;
}

void NudgeTracker::FillProtoMessage(
    ModelType type,
    sync_pb::GetUpdateTriggers* msg) const {
  // Fill the list of payloads, if applicable.  The payloads must be ordered
  // oldest to newest, so we insert them in the same order as we've been storing
  // them internally.
  PayloadListMap::const_iterator type_it = payload_list_map_.find(type);
  if (type_it != payload_list_map_.end()) {
    const PayloadList& payload_list = type_it->second;
    for (PayloadList::const_iterator payload_it = payload_list.begin();
         payload_it != payload_list.end(); ++payload_it) {
      msg->add_notification_hint(*payload_it);
    }
  }

  // TODO(rlarocque): Support Tango trickles.  See crbug.com/223437.
  // msg->set_server_dropped_hints(server_dropped_payload_types_.Has(type));

  msg->set_client_dropped_hints(locally_dropped_payload_types_.Has(type));
  msg->set_invalidations_out_of_sync(invalidations_out_of_sync_);

  {
    NudgeMap::const_iterator it = local_nudge_counts_.find(type);
    if (it == local_nudge_counts_.end()) {
      msg->set_local_modification_nudges(0);
    } else {
      msg->set_local_modification_nudges(it->second);
    }
  }

  {
    NudgeMap::const_iterator it = refresh_requested_counts_.find(type);
    if (it == refresh_requested_counts_.end()) {
      msg->set_datatype_refresh_nudges(0);
    } else {
      msg->set_datatype_refresh_nudges(it->second);
    }
  }
}

void NudgeTracker::SetHintBufferSize(size_t size) {
  // We could iterate through the list and trim it down to size here, but that
  // seems unnecessary.  This limit will take effect on all future invalidations
  // received.
  num_payloads_per_type_ = size;
}

}  // namespace sessions
}  // namespace syncer
