// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/model_type_registry.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "sync/engine/directory_commit_contributor.h"
#include "sync/engine/directory_type_debug_info_emitter.h"
#include "sync/engine/directory_update_handler.h"
#include "sync/engine/non_blocking_type_processor_core.h"
#include "sync/internal_api/public/non_blocking_type_processor.h"

namespace syncer {

ModelTypeRegistry::ModelTypeRegistry() : directory_(NULL) {}

ModelTypeRegistry::ModelTypeRegistry(
    const std::vector<scoped_refptr<ModelSafeWorker> >& workers,
    syncable::Directory* directory)
    : directory_(directory) {
  for (size_t i = 0u; i < workers.size(); ++i) {
    workers_map_.insert(
        std::make_pair(workers[i]->GetModelSafeGroup(), workers[i]));
  }
}

ModelTypeRegistry::~ModelTypeRegistry() {}

void ModelTypeRegistry::SetEnabledDirectoryTypes(
    const ModelSafeRoutingInfo& routing_info) {
  // Remove all existing directory processors and delete them.
  for (ModelTypeSet::Iterator it = enabled_directory_types_.First();
       it.Good(); it.Inc()) {
    size_t result1 = update_handler_map_.erase(it.Get());
    size_t result2 = commit_contributor_map_.erase(it.Get());
    size_t result3 = directory_type_debug_info_emitter_map_.erase(it.Get());
    DCHECK_EQ(1U, result1);
    DCHECK_EQ(1U, result2);
    DCHECK_EQ(1U, result3);
  }

  // Clear the old instances of directory update handlers and commit
  // contributors, deleting their contents in the processs.
  directory_update_handlers_.clear();
  directory_commit_contributors_.clear();
  directory_type_debug_info_emitters_.clear();

  // Create new ones and add them to the appropriate containers.
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
    DirectoryTypeDebugInfoEmitter* emitter =
        new DirectoryTypeDebugInfoEmitter(directory_, type);

    // These containers take ownership of their contents.
    directory_commit_contributors_.push_back(committer);
    directory_update_handlers_.push_back(updater);
    directory_type_debug_info_emitters_.push_back(emitter);

    bool inserted1 =
        update_handler_map_.insert(std::make_pair(type, updater)).second;
    DCHECK(inserted1) << "Attempt to override existing type handler in map";

    bool inserted2 =
        commit_contributor_map_.insert(std::make_pair(type, committer)).second;
    DCHECK(inserted2) << "Attempt to override existing type handler in map";

    bool inserted3 =
        directory_type_debug_info_emitter_map_.insert(
            std::make_pair(type, emitter)).second;
    DCHECK(inserted3) << "Attempt to override existing type handler in map";
  }

  enabled_directory_types_ = GetRoutingInfoTypes(routing_info);
  DCHECK(Intersection(GetEnabledDirectoryTypes(),
                      GetEnabledNonBlockingTypes()).Empty());
}

void ModelTypeRegistry::InitializeNonBlockingType(
    ModelType type,
    scoped_refptr<base::SequencedTaskRunner> type_task_runner,
    base::WeakPtr<NonBlockingTypeProcessor> processor) {
  DVLOG(1) << "Enabling an off-thread sync type: " << ModelTypeToString(type);

  // Initialize CoreProcessor -> Processor communication channel.
  scoped_ptr<NonBlockingTypeProcessorCore> core(
      new NonBlockingTypeProcessorCore(type, type_task_runner, processor));

  // Initialize Processor -> CoreProcessor communication channel.
  type_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingTypeProcessor::OnConnect,
                 processor,
                 core->AsWeakPtr(),
                 scoped_refptr<base::SequencedTaskRunner>(
                     base::MessageLoopProxy::current())));

  DCHECK(update_handler_map_.find(type) == update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) == commit_contributor_map_.end());

  update_handler_map_.insert(std::make_pair(type, core.get()));
  commit_contributor_map_.insert(std::make_pair(type, core.get()));

  // The container takes ownership.
  non_blocking_type_processor_cores_.push_back(core.release());

  DCHECK(Intersection(GetEnabledDirectoryTypes(),
                      GetEnabledNonBlockingTypes()).Empty());
}

void ModelTypeRegistry::RemoveNonBlockingType(ModelType type) {
  DVLOG(1) << "Disabling an off-thread sync type: " << ModelTypeToString(type);
  DCHECK(update_handler_map_.find(type) != update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) != commit_contributor_map_.end());

  size_t updaters_erased = update_handler_map_.erase(type);
  size_t committers_erased = commit_contributor_map_.erase(type);

  DCHECK_EQ(1U, updaters_erased);
  DCHECK_EQ(1U, committers_erased);

  // Remove from the ScopedVector, deleting the core in the process.
  for (ScopedVector<NonBlockingTypeProcessorCore>::iterator it =
       non_blocking_type_processor_cores_.begin();
       it != non_blocking_type_processor_cores_.end(); ++it) {
    if ((*it)->GetModelType() == type) {
      non_blocking_type_processor_cores_.erase(it);
      break;
    }
  }
}

ModelTypeSet ModelTypeRegistry::GetEnabledTypes() const {
  return Union(GetEnabledDirectoryTypes(), GetEnabledNonBlockingTypes());
}

UpdateHandlerMap* ModelTypeRegistry::update_handler_map() {
  return &update_handler_map_;
}

CommitContributorMap* ModelTypeRegistry::commit_contributor_map() {
  return &commit_contributor_map_;
}

DirectoryTypeDebugInfoEmitterMap*
ModelTypeRegistry::directory_type_debug_info_emitter_map() {
  return &directory_type_debug_info_emitter_map_;
}

ModelTypeSet ModelTypeRegistry::GetEnabledDirectoryTypes() const {
  return enabled_directory_types_;
}

ModelTypeSet ModelTypeRegistry::GetEnabledNonBlockingTypes() const {
  ModelTypeSet enabled_off_thread_types;
  for (ScopedVector<NonBlockingTypeProcessorCore>::const_iterator it =
           non_blocking_type_processor_cores_.begin();
       it != non_blocking_type_processor_cores_.end(); ++it) {
    enabled_off_thread_types.Put((*it)->GetModelType());
  }
  return enabled_off_thread_types;
}

}  // namespace syncer
