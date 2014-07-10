// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/model_type_registry.h"

#include "base/bind.h"
#include "base/observer_list.h"
#include "base/thread_task_runner_handle.h"
#include "sync/engine/directory_commit_contributor.h"
#include "sync/engine/directory_update_handler.h"
#include "sync/engine/model_type_sync_proxy.h"
#include "sync/engine/model_type_sync_proxy_impl.h"
#include "sync/engine/model_type_sync_worker.h"
#include "sync/engine/model_type_sync_worker_impl.h"
#include "sync/engine/non_blocking_sync_common.h"
#include "sync/sessions/directory_type_debug_info_emitter.h"

namespace syncer {

namespace {

class ModelTypeSyncProxyWrapper : public ModelTypeSyncProxy {
 public:
  ModelTypeSyncProxyWrapper(
      const base::WeakPtr<ModelTypeSyncProxyImpl>& proxy,
      const scoped_refptr<base::SequencedTaskRunner>& processor_task_runner);
  virtual ~ModelTypeSyncProxyWrapper();

  virtual void OnCommitCompleted(
      const DataTypeState& type_state,
      const CommitResponseDataList& response_list) OVERRIDE;
  virtual void OnUpdateReceived(
      const DataTypeState& type_state,
      const UpdateResponseDataList& response_list) OVERRIDE;

 private:
  base::WeakPtr<ModelTypeSyncProxyImpl> processor_;
  scoped_refptr<base::SequencedTaskRunner> processor_task_runner_;
};

ModelTypeSyncProxyWrapper::ModelTypeSyncProxyWrapper(
    const base::WeakPtr<ModelTypeSyncProxyImpl>& proxy,
    const scoped_refptr<base::SequencedTaskRunner>& processor_task_runner)
    : processor_(proxy), processor_task_runner_(processor_task_runner) {
}

ModelTypeSyncProxyWrapper::~ModelTypeSyncProxyWrapper() {
}

void ModelTypeSyncProxyWrapper::OnCommitCompleted(
    const DataTypeState& type_state,
    const CommitResponseDataList& response_list) {
  processor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ModelTypeSyncProxyImpl::OnCommitCompleted,
                 processor_,
                 type_state,
                 response_list));
}

void ModelTypeSyncProxyWrapper::OnUpdateReceived(
    const DataTypeState& type_state,
    const UpdateResponseDataList& response_list) {
  processor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ModelTypeSyncProxyImpl::OnUpdateReceived,
                 processor_,
                 type_state,
                 response_list));
}

class ModelTypeSyncWorkerWrapper : public ModelTypeSyncWorker {
 public:
  ModelTypeSyncWorkerWrapper(
      const base::WeakPtr<ModelTypeSyncWorkerImpl>& worker,
      const scoped_refptr<base::SequencedTaskRunner>& sync_thread);
  virtual ~ModelTypeSyncWorkerWrapper();

  virtual void EnqueueForCommit(const CommitRequestDataList& list) OVERRIDE;

 private:
  base::WeakPtr<ModelTypeSyncWorkerImpl> worker_;
  scoped_refptr<base::SequencedTaskRunner> sync_thread_;
};

ModelTypeSyncWorkerWrapper::ModelTypeSyncWorkerWrapper(
    const base::WeakPtr<ModelTypeSyncWorkerImpl>& worker,
    const scoped_refptr<base::SequencedTaskRunner>& sync_thread)
    : worker_(worker), sync_thread_(sync_thread) {
}

ModelTypeSyncWorkerWrapper::~ModelTypeSyncWorkerWrapper() {
}

void ModelTypeSyncWorkerWrapper::EnqueueForCommit(
    const CommitRequestDataList& list) {
  sync_thread_->PostTask(
      FROM_HERE,
      base::Bind(&ModelTypeSyncWorkerImpl::EnqueueForCommit, worker_, list));
}

}  // namespace

ModelTypeRegistry::ModelTypeRegistry(
    const std::vector<scoped_refptr<ModelSafeWorker> >& workers,
    syncable::Directory* directory,
    NudgeHandler* nudge_handler)
    : directory_(directory),
      nudge_handler_(nudge_handler),
      weak_ptr_factory_(this) {
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

void ModelTypeRegistry::ConnectSyncTypeToWorker(
    ModelType type,
    const DataTypeState& data_type_state,
    const scoped_refptr<base::SequencedTaskRunner>& type_task_runner,
    const base::WeakPtr<ModelTypeSyncProxyImpl>& proxy_impl) {
  DVLOG(1) << "Enabling an off-thread sync type: " << ModelTypeToString(type);

  // Initialize Worker -> Proxy communication channel.
  scoped_ptr<ModelTypeSyncProxy> proxy(
      new ModelTypeSyncProxyWrapper(proxy_impl, type_task_runner));
  scoped_ptr<ModelTypeSyncWorkerImpl> worker(new ModelTypeSyncWorkerImpl(
      type, data_type_state, nudge_handler_, proxy.Pass()));

  // Initialize Proxy -> Worker communication channel.
  scoped_ptr<ModelTypeSyncWorker> wrapped_worker(
      new ModelTypeSyncWorkerWrapper(worker->AsWeakPtr(),
                                     scoped_refptr<base::SequencedTaskRunner>(
                                         base::ThreadTaskRunnerHandle::Get())));
  type_task_runner->PostTask(FROM_HERE,
                             base::Bind(&ModelTypeSyncProxyImpl::OnConnect,
                                        proxy_impl,
                                        base::Passed(&wrapped_worker)));

  DCHECK(update_handler_map_.find(type) == update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) == commit_contributor_map_.end());

  update_handler_map_.insert(std::make_pair(type, worker.get()));
  commit_contributor_map_.insert(std::make_pair(type, worker.get()));

  // The container takes ownership.
  model_type_sync_workers_.push_back(worker.release());

  DCHECK(Intersection(GetEnabledDirectoryTypes(),
                      GetEnabledNonBlockingTypes()).Empty());
}

void ModelTypeRegistry::DisconnectSyncWorker(ModelType type) {
  DVLOG(1) << "Disabling an off-thread sync type: " << ModelTypeToString(type);
  DCHECK(update_handler_map_.find(type) != update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) != commit_contributor_map_.end());

  size_t updaters_erased = update_handler_map_.erase(type);
  size_t committers_erased = commit_contributor_map_.erase(type);

  DCHECK_EQ(1U, updaters_erased);
  DCHECK_EQ(1U, committers_erased);

  // Remove from the ScopedVector, deleting the worker in the process.
  for (ScopedVector<ModelTypeSyncWorkerImpl>::iterator it =
           model_type_sync_workers_.begin();
       it != model_type_sync_workers_.end();
       ++it) {
    if ((*it)->GetModelType() == type) {
      model_type_sync_workers_.erase(it);
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

base::WeakPtr<SyncContext> ModelTypeRegistry::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ModelTypeSet ModelTypeRegistry::GetEnabledDirectoryTypes() const {
  return enabled_directory_types_;
}

ModelTypeSet ModelTypeRegistry::GetEnabledNonBlockingTypes() const {
  ModelTypeSet enabled_off_thread_types;
  for (ScopedVector<ModelTypeSyncWorkerImpl>::const_iterator it =
           model_type_sync_workers_.begin();
       it != model_type_sync_workers_.end();
       ++it) {
    enabled_off_thread_types.Put((*it)->GetModelType());
  }
  return enabled_off_thread_types;
}

}  // namespace syncer
