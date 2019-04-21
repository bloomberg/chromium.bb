// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_UTIL_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_UTIL_H_

#include <stdint.h>

#include <set>

#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync.pb.h"

namespace sync_pb {
class CommitMessage;
class SyncEntity;
}

namespace syncer {

namespace syncable {
class Entry;
class Id;
class BaseWriteTransaction;
}

namespace commit_util {

// Adds bookmark extensions activity report to |message|.
void AddExtensionsActivityToMessage(
    ExtensionsActivity* activity,
    ExtensionsActivity::Records* extensions_activity_buffer,
    sync_pb::CommitMessage* message);

// Fills the config_params field of |message|.
void AddClientConfigParamsToMessage(ModelTypeSet enabled_types,
                                    bool cookie_jar_mismatch,
                                    sync_pb::CommitMessage* message);

// Takes a snapshot of |meta_entry| and puts it into a protobuf suitable for use
// in a commit request message.
void BuildCommitItem(const syncable::Entry& meta_entry,
                     sync_pb::SyncEntity* sync_entry);

// Process a single commit response.  Updates the entry's SERVER fields using
// |pb_commit_response| and |pb_committed_entry|.
//
// The |deleted_folders| parameter is a set of IDs that represent deleted
// folders.  This function will add its entry's ID to this set if it finds
// itself processing a folder deletion.
sync_pb::CommitResponse::ResponseType ProcessSingleCommitResponse(
    syncable::BaseWriteTransaction* trans,
    const sync_pb::CommitResponse_EntryResponse& server_entry,
    const sync_pb::SyncEntity& commit_request_entry,
    int64_t metahandle,
    std::set<syncable::Id>* deleted_folders);

}  // namespace commit_util

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_UTIL_H_
