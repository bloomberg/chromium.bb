// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_MODEL_TYPE_REGISTRY_H_
#define SYNC_ENGINE_MODEL_TYPE_REGISTRY_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"

namespace syncer {

namespace syncable {
class Directory;
}  // namespace syncable

class SyncDirectoryUpdateHandler;
class SyncDirectoryCommitContributor;

typedef std::map<ModelType, SyncDirectoryUpdateHandler*> UpdateHandlerMap;
typedef std::map<ModelType, SyncDirectoryCommitContributor*>
    CommitContributorMap;

// Keeps track of the sets of active update handlers and commit contributors.
class SYNC_EXPORT_PRIVATE ModelTypeRegistry {
 public:
  ModelTypeRegistry(
      const std::vector<scoped_refptr<ModelSafeWorker> >& workers,
      syncable::Directory* directory);
  ~ModelTypeRegistry();

  // Sets the set of enabled types.
  void SetEnabledDirectoryTypes(const ModelSafeRoutingInfo& routing_info);

  // Simple getters.
  UpdateHandlerMap* update_handler_map();
  CommitContributorMap* commit_contributor_map();

 private:
  // Classes to manage the types hooked up to receive and commit sync data.
  UpdateHandlerMap update_handler_map_;
  CommitContributorMap commit_contributor_map_;

  // Deleter for the |update_handler_map_|.
  STLValueDeleter<UpdateHandlerMap> update_handler_deleter_;

  // Deleter for the |commit_contributor_map_|.
  STLValueDeleter<CommitContributorMap> commit_contributor_deleter_;

  // The known ModelSafeWorkers.
  std::map<ModelSafeGroup, scoped_refptr<ModelSafeWorker> > workers_map_;

  // The directory.  Not owned.
  syncable::Directory* directory_;

  DISALLOW_COPY_AND_ASSIGN(ModelTypeRegistry);
};

}  // namespace syncer

#endif // SYNC_ENGINE_MODEL_TYPE_REGISTRY_H_

