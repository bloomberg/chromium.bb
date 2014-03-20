// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/model_type_registry.h"

#include "sync/engine/directory_commit_contributor.h"
#include "sync/engine/directory_update_handler.h"

namespace syncer {

ModelTypeRegistry::ModelTypeRegistry(
    const std::vector<scoped_refptr<ModelSafeWorker> >& workers,
    syncable::Directory* directory)
    : update_handler_deleter_(&update_handler_map_),
      commit_contributor_deleter_(&commit_contributor_map_),
      directory_(directory) {
  for (size_t i = 0u; i < workers.size(); ++i) {
    workers_map_.insert(
        std::make_pair(workers[i]->GetModelSafeGroup(), workers[i]));
  }
}

ModelTypeRegistry::~ModelTypeRegistry() {}

void ModelTypeRegistry::SetEnabledDirectoryTypes(
    const ModelSafeRoutingInfo& routing_info) {
  STLDeleteValues(&update_handler_map_);
  STLDeleteValues(&commit_contributor_map_);
  update_handler_map_.clear();
  commit_contributor_map_.clear();

  for (ModelSafeRoutingInfo::const_iterator routing_iter = routing_info.begin();
       routing_iter != routing_info.end(); ++routing_iter) {
    ModelType type = routing_iter->first;
    ModelSafeGroup group = routing_iter->second;
    std::map<ModelSafeGroup, scoped_refptr<ModelSafeWorker> >::iterator
        worker_it = workers_map_.find(group);
    DCHECK(worker_it != workers_map_.end());
    scoped_refptr<ModelSafeWorker> worker = worker_it->second;

    DirectoryCommitContributor* committer =
        new DirectoryCommitContributor(directory_, type);
    DirectoryUpdateHandler* updater =
        new DirectoryUpdateHandler(directory_, type, worker);

    bool inserted1 =
        update_handler_map_.insert(std::make_pair(type, updater)).second;
    DCHECK(inserted1) << "Attempt to override existing type handler in map";

    bool inserted2 =
        commit_contributor_map_.insert(std::make_pair(type, committer)).second;
    DCHECK(inserted2) << "Attempt to override existing type handler in map";

  }
}

UpdateHandlerMap* ModelTypeRegistry::update_handler_map() {
  return &update_handler_map_;
}

CommitContributorMap* ModelTypeRegistry::commit_contributor_map() {
  return &commit_contributor_map_;
}

}  // namespace syncer
