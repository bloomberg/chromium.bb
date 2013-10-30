// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_PROCESS_UPDATES_UTIL_H_
#define SYNC_ENGINE_PROCESS_UPDATES_UTIL_H_

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/engine/syncer_types.h"
#include "sync/internal_api/public/base/model_type.h"

namespace sync_pb {
class GetUpdatesResponse;
class SyncEntity;
}

namespace syncer {

namespace sessions {
class StatusController;
}

namespace syncable {
class ModelNeutralWriteTransaction;
class Directory;
}

class Cryptographer;

// TODO(rlarocque): Move these definitions somewhere else?
typedef std::vector<const sync_pb::SyncEntity*> SyncEntityList;
typedef std::map<ModelType, SyncEntityList> TypeSyncEntityMap;

// Given a GetUpdates response, iterates over all the returned items and
// divides them according to their type.  Outputs a map from model types to
// received SyncEntities.  The output map will have entries (possibly empty)
// for all types in |requested_types|.
void PartitionUpdatesByType(
    const sync_pb::GetUpdatesResponse& updates,
    ModelTypeSet requested_types,
    TypeSyncEntityMap* updates_by_type);

// Processes all the updates associated with a single ModelType.
void ProcessDownloadedUpdates(
    syncable::Directory* dir,
    syncable::ModelNeutralWriteTransaction* trans,
    ModelType type,
    const SyncEntityList& applicable_updates,
    sessions::StatusController* status);

// Checks whether or not an update is fit for processing.
//
// The answer may be "no" if the update appears invalid, or it's not releveant
// (ie. a delete for an item we've never heard of), or other reasons.
VerifyResult VerifyUpdate(
    syncable::ModelNeutralWriteTransaction* trans,
    const sync_pb::SyncEntity& entry,
    ModelType requested_type);

// If the update passes a series of checks, this function will copy
// the SyncEntity's data into the SERVER side of the syncable::Directory.
void ProcessUpdate(
    const sync_pb::SyncEntity& proto_update,
    const Cryptographer* cryptographer,
    syncable::ModelNeutralWriteTransaction* const trans);

}  // namespace syncer

#endif  // SYNC_ENGINE_PROCESS_UPDATES_UTIL_H_
