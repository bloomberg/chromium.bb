// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains various utility functions used by ModelTypeWorker to
// preprocess remote bookmark updates, to deal with backward-compatibility of
// data and migrate updates such that they resemble updates from a modern
// client.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_BOOKMARK_UPDATE_PREPROCESSING_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_BOOKMARK_UPDATE_PREPROCESSING_H_

#include <string>

namespace sync_pb {
class SyncEntity;
class EntitySpecifics;
}  // namespace sync_pb

namespace syncer {

struct EntityData;

// Populates |data->unique_position| from the various supported proto fields in
// |update_entity|. |data| must not be null.
void AdaptUniquePositionForBookmark(const sync_pb::SyncEntity& update_entity,
                                    EntityData* data);

// Populates |specifics->bookmark().legacy_canonicalized_title()| from the
// various supported sources, or no-op if specifics already have the field set.
// |specifics| must not be null.
void AdaptTitleForBookmark(const sync_pb::SyncEntity& update_entity,
                           sync_pb::EntitySpecifics* specifics,
                           bool specifics_were_encrypted);

// Populates |specifics->bookmark().guid()| from the various supported sources,
// or no-op if specifics already have the field set. |specifics| must not be
// null.
void AdaptGuidForBookmark(const sync_pb::SyncEntity& update_entity,
                          sync_pb::EntitySpecifics* specifics);

// GUID-inferring function exposed for testing.
std::string InferGuidForLegacyBookmarkForTesting(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_BOOKMARK_UPDATE_PREPROCESSING_H_
