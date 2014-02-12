// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/get_updates_processor.h"

#include <map>

#include "sync/engine/get_updates_delegate.h"
#include "sync/engine/sync_directory_update_handler.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

typedef std::map<ModelType, size_t> TypeToIndexMap;

namespace {

// Given a GetUpdates response, iterates over all the returned items and
// divides them according to their type.  Outputs a map from model types to
// received SyncEntities.  The output map will have entries (possibly empty)
// for all types in |requested_types|.
void PartitionUpdatesByType(
    const sync_pb::GetUpdatesResponse& updates,
    ModelTypeSet requested_types,
    TypeSyncEntityMap* updates_by_type) {
  int update_count = updates.entries().size();
  for (ModelTypeSet::Iterator it = requested_types.First();
       it.Good(); it.Inc()) {
    updates_by_type->insert(std::make_pair(it.Get(), SyncEntityList()));
  }
  for (int i = 0; i < update_count; ++i) {
    const sync_pb::SyncEntity& update = updates.entries(i);
    ModelType type = GetModelType(update);
    if (!IsRealDataType(type)) {
      NOTREACHED() << "Received update with invalid type.";
      continue;
    }

    TypeSyncEntityMap::iterator it = updates_by_type->find(type);
    if (it == updates_by_type->end()) {
      NOTREACHED() << "Received update for unexpected type "
                   << ModelTypeToString(type);
      continue;
    }

    it->second.push_back(&update);
  }
}

// Builds a map of ModelTypes to indices to progress markers in the given
// |gu_response| message.  The map is returned in the |index_map| parameter.
void PartitionProgressMarkersByType(
    const sync_pb::GetUpdatesResponse& gu_response,
    ModelTypeSet request_types,
    TypeToIndexMap* index_map) {
  for (int i = 0; i < gu_response.new_progress_marker_size(); ++i) {
    int field_number = gu_response.new_progress_marker(i).data_type_id();
    ModelType model_type = GetModelTypeFromSpecificsFieldNumber(field_number);
    if (!IsRealDataType(model_type)) {
      DLOG(WARNING) << "Unknown field number " << field_number;
      continue;
    }
    if (!request_types.Has(model_type)) {
      DLOG(WARNING)
          << "Skipping unexpected progress marker for non-enabled type "
          << ModelTypeToString(model_type);
      continue;
    }
    index_map->insert(std::make_pair(model_type, i));
  }
}

}  // namespace

GetUpdatesProcessor::GetUpdatesProcessor(UpdateHandlerMap* update_handler_map,
                                         const GetUpdatesDelegate& delegate)
    : update_handler_map_(update_handler_map), delegate_(delegate) {}

GetUpdatesProcessor::~GetUpdatesProcessor() {}

void GetUpdatesProcessor::PrepareGetUpdates(
    ModelTypeSet gu_types,
    sync_pb::GetUpdatesMessage* get_updates) {
  for (ModelTypeSet::Iterator it = gu_types.First(); it.Good(); it.Inc()) {
    UpdateHandlerMap::iterator handler_it = update_handler_map_->find(it.Get());
    DCHECK(handler_it != update_handler_map_->end());
    sync_pb::DataTypeProgressMarker* progress_marker =
        get_updates->add_from_progress_marker();
    handler_it->second->GetDownloadProgress(progress_marker);
  }
  delegate_.HelpPopulateGuMessage(get_updates);
}

bool GetUpdatesProcessor::ProcessGetUpdatesResponse(
    ModelTypeSet gu_types,
    const sync_pb::GetUpdatesResponse& gu_response,
    sessions::StatusController* status_controller) {
  TypeSyncEntityMap updates_by_type;
  PartitionUpdatesByType(gu_response, gu_types, &updates_by_type);
  DCHECK_EQ(gu_types.Size(), updates_by_type.size());

  TypeToIndexMap progress_index_by_type;
  PartitionProgressMarkersByType(gu_response,
                                 gu_types,
                                 &progress_index_by_type);
  if (gu_types.Size() != progress_index_by_type.size()) {
    NOTREACHED() << "Missing progress markers in GetUpdates response.";
    return false;
  }

  // Iterate over these maps in parallel, processing updates for each type.
  TypeToIndexMap::iterator progress_marker_iter =
      progress_index_by_type.begin();
  TypeSyncEntityMap::iterator updates_iter = updates_by_type.begin();
  for (; (progress_marker_iter != progress_index_by_type.end()
           && updates_iter != updates_by_type.end());
       ++progress_marker_iter, ++updates_iter) {
    DCHECK_EQ(progress_marker_iter->first, updates_iter->first);
    ModelType type = progress_marker_iter->first;

    UpdateHandlerMap::iterator update_handler_iter =
        update_handler_map_->find(type);

    if (update_handler_iter != update_handler_map_->end()) {
      update_handler_iter->second->ProcessGetUpdatesResponse(
          gu_response.new_progress_marker(progress_marker_iter->second),
          updates_iter->second,
          status_controller);
    } else {
      DLOG(WARNING)
          << "Ignoring received updates of a type we can't handle.  "
          << "Type is: " << ModelTypeToString(type);
      continue;
    }
  }
  DCHECK(progress_marker_iter == progress_index_by_type.end() &&
         updates_iter == updates_by_type.end());

  return true;
}

void GetUpdatesProcessor::ApplyUpdates(
    sessions::StatusController* status_controller) {
  delegate_.ApplyUpdates(status_controller, update_handler_map_);
}

}  // namespace syncer
