// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/model_type_registry.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/observer_list.h"
#include "sync/engine/directory_commit_contributor.h"
#include "sync/engine/directory_update_handler.h"
#include "sync/engine/non_blocking_sync_common.h"
#include "sync/engine/non_blocking_type_processor.h"
#include "sync/engine/non_blocking_type_processor_core.h"
#include "sync/engine/non_blocking_type_processor_core_interface.h"
#include "sync/sessions/directory_type_debug_info_emitter.h"

namespace syncer {

namespace {

class NonBlockingTypeProcessorCoreWrapper
    : public NonBlockingTypeProcessorCoreInterface {
 public:
  NonBlockingTypeProcessorCoreWrapper(
      base::WeakPtr<NonBlockingTypeProcessorCore> core,
      scoped_refptr<base::SequencedTaskRunner> sync_thread);
  virtual ~NonBlockingTypeProcessorCoreWrapper();

  virtual void RequestCommits(const CommitRequestDataList& list) OVERRIDE;

 private:
  base::WeakPtr<NonBlockingTypeProcessorCore> core_;
  scoped_refptr<base::SequencedTaskRunner> sync_thread_;
};

NonBlockingTypeProcessorCoreWrapper::NonBlockingTypeProcessorCoreWrapper(
    base::WeakPtr<NonBlockingTypeProcessorCore> core,
    scoped_refptr<base::SequencedTaskRunner> sync_thread)
    : core_(core), sync_thread_(sync_thread) {
}

NonBlockingTypeProcessorCoreWrapper::~NonBlockingTypeProcessorCoreWrapper() {
}

void NonBlockingTypeProcessorCoreWrapper::RequestCommits(
    const CommitRequestDataList& list) {
  sync_thread_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingTypeProcessorCore::RequestCommits, core_, list));
}

}  // namespace

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
  // Remove all existing directory processors and delete them.  The
  // DebugInfoEmitters are not deleted here, since we want to preserve their
  // counters.
  for (ModelTypeSet::Iterator it = enabled_directory_types_.First();
       it.Good(); it.Inc()) {
    size_t result1 = update_handler_map_.erase(it.Get());
    size_t result2 = commit_contributor_map_.erase(it.Get());
    DCHECK_EQ(1U, result1);
    DCHECK_EQ(1U, result2);
  }

  // Clear the old instances of directory update handlers and commit
  // contributors, deleting their contents in the processs.
  directory_update_handlers_.clear();
  directory_commit_contributors_.clear();

  // Create new ones and add them to the appropriate containers.
  for (ModelSafeRoutingInfo::const_iterator routing_iter = routing_info.begin();
       routing_iter != routing_info.end(); ++routing_iter) {
    ModelType type = routing_iter->first;
    ModelSafeGroup group = routing_iter->second;
    std::map<ModelSafeGroup, scoped_refptr<ModelSafeWorker> >::iterator
        worker_it = workers_map_.find(group);
    DCHECK(worker_it != workers_map_.end());
    scoped_refptr<ModelSafeWorker> worker = worker_it->second;

    // DebugInfoEmitters are never deleted.  Use existing one if we have it.
    DirectoryTypeDebugInfoEmitter* emitter = NULL;
    DirectoryTypeDebugInfoEmitterMap::iterator it =
        directory_type_debug_info_emitter_map_.find(type);
    if (it != directory_type_debug_info_emitter_map_.end()) {
      emitter = it->second;
    } else {
      emitter = new DirectoryTypeDebugInfoEmitter(directory_, type,
                                                  &type_debug_info_observers_);
      directory_type_debug_info_emitter_map_.insert(
          std::make_pair(type, emitter));
      directory_type_debug_info_emitters_.push_back(emitter);
    }

    DirectoryCommitContributor* committer =
        new DirectoryCommitContributor(directory_, type, emitter);
    DirectoryUpdateHandler* updater =
        new DirectoryUpdateHandler(directory_, type, worker, emitter);

    // These containers take ownership of their contents.
    directory_commit_contributors_.push_back(committer);
    directory_update_handlers_.push_back(updater);

    bool inserted1 =
        update_handler_map_.insert(std::make_pair(type, updater)).second;
    DCHECK(inserted1) << "Attempt to override existing type handler in map";

    bool inserted2 =
        commit_contributor_map_.insert(std::make_pair(type, committer)).second;
    DCHECK(inserted2) << "Attempt to override existing type handler in map";
  }

  enabled_directory_types_ = GetRoutingInfoTypes(routing_info);
  DCHECK(Intersection(GetEnabledDirectoryTypes(),
                      GetEnabledNonBlockingTypes()).Empty());
}

void ModelTypeRegistry::InitializeNonBlockingType(
    ModelType type,
    const DataTypeState& data_type_state,
    scoped_refptr<base::SequencedTaskRunner> type_task_runner,
    base::WeakPtr<NonBlockingTypeProcessor> processor) {
  DVLOG(1) << "Enabling an off-thread sync type: " << ModelTypeToString(type);

  // Initialize CoreProcessor -> Processor communication channel.
  scoped_ptr<NonBlockingTypeProcessorCore> core(
      new NonBlockingTypeProcessorCore(type, type_task_runner, processor));

  // TODO(rlarocque): DataTypeState should be forwarded to core here.

  // Initialize Processor -> CoreProcessor communication channel.
  scoped_ptr<NonBlockingTypeProcessorCoreInterface> core_interface(
      new NonBlockingTypeProcessorCoreWrapper(
          core->AsWeakPtr(),
          scoped_refptr<base::SequencedTaskRunner>(
              base::MessageLoopProxy::current())));
  type_task_runner->PostTask(FROM_HERE,
                             base::Bind(&NonBlockingTypeProcessor::OnConnect,
                                        processor,
                                        base::Passed(&core_interface)));

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

void ModelTypeRegistry::RegisterDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) {
  if (!type_debug_info_observers_.HasObserver(observer))
    type_debug_info_observers_.AddObserver(observer);
}

void ModelTypeRegistry::UnregisterDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) {
  type_debug_info_observers_.RemoveObserver(observer);
}

bool ModelTypeRegistry::HasDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) {
  return type_debug_info_observers_.HasObserver(observer);
}

void ModelTypeRegistry::RequestEmitDebugInfo() {
  for (DirectoryTypeDebugInfoEmitterMap::iterator it =
       directory_type_debug_info_emitter_map_.begin();
       it != directory_type_debug_info_emitter_map_.end(); ++it) {
    it->second->EmitCommitCountersUpdate();
    it->second->EmitUpdateCountersUpdate();
    it->second->EmitStatusCountersUpdate();
  }
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
