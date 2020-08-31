// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_CLIENT_TAG_BASED_REMOTE_UPDATE_HANDLER_H_
#define COMPONENTS_SYNC_MODEL_IMPL_CLIENT_TAG_BASED_REMOTE_UPDATE_HANDLER_H_

#include <memory>
#include <string>
#include <unordered_set>

#include "base/macros.h"
#include "base/optional.h"

#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/model_error.h"

namespace sync_pb {
class ModelTypeState;
}  // namespace sync_pb

namespace syncer {

class ModelTypeSyncBridge;
class ProcessorEntityTracker;
class ProcessorEntity;

// A sync component that performs updates from sync server.
class ClientTagBasedRemoteUpdateHandler {
 public:
  // All parameters must not be nullptr and they must outlive this object.
  ClientTagBasedRemoteUpdateHandler(
      ModelType type,
      ModelTypeSyncBridge* bridge,
      ProcessorEntityTracker* entities);

  // Processes incremental updates from the sync server.
  base::Optional<ModelError> ProcessIncrementalUpdate(
      const sync_pb::ModelTypeState& model_type_state,
      UpdateResponseDataList updates);

  ClientTagBasedRemoteUpdateHandler(const ClientTagBasedRemoteUpdateHandler&) =
      delete;
  ClientTagBasedRemoteUpdateHandler& operator=(
      const ClientTagBasedRemoteUpdateHandler&) = delete;

 private:
  // Helper function to process the update for a single entity. If a local data
  // change is required, it will be added to |entity_changes|. The return value
  // is the tracked entity, or nullptr if the update should be ignored.
  // |storage_key_to_clear| must not be null and allows the implementation to
  // indicate that a certain storage key is now obsolete and should be cleared,
  // which is leveraged in certain conflict resolution scenarios.
  ProcessorEntity* ProcessUpdate(UpdateResponseData update,
                                 EntityChangeList* entity_changes,
                                 std::string* storage_key_to_clear);

  // Resolve a conflict between |update| and the pending commit in |entity|.
  void ResolveConflict(UpdateResponseData update,
                       ProcessorEntity* entity,
                       EntityChangeList* changes,
                       std::string* storage_key_to_clear);

  // Gets the entity for the given tag hash, or null if there isn't one.
  ProcessorEntity* GetEntityForTagHash(const ClientTagHash& tag_hash);

  // Create an entity in the entity tracker for |storage_key|.
  // |storage_key| must not exist in the entity tracker.
  ProcessorEntity* CreateEntity(const std::string& storage_key,
                                const EntityData& data);

  // Version of the above that generates a tag for |data|.
  ProcessorEntity* CreateEntity(const EntityData& data);

  // The model type this object syncs.
  const ModelType type_;

  // ModelTypeSyncBridge linked to associated processor.
  ModelTypeSyncBridge* const bridge_;

  // A map of client tag hash to sync entities known to the processor.
  // Should be replaced with new interface.
  ProcessorEntityTracker* const entity_tracker_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_CLIENT_TAG_BASED_REMOTE_UPDATE_HANDLER_H_
