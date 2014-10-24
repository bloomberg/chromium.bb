// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/engine/model_safe_worker.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace syncer {

base::DictionaryValue* ModelSafeRoutingInfoToValue(
    const ModelSafeRoutingInfo& routing_info) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
       it != routing_info.end(); ++it) {
    dict->SetString(ModelTypeToString(it->first),
                    ModelSafeGroupToString(it->second));
  }
  return dict;
}

std::string ModelSafeRoutingInfoToString(
    const ModelSafeRoutingInfo& routing_info) {
  scoped_ptr<base::DictionaryValue> dict(
      ModelSafeRoutingInfoToValue(routing_info));
  std::string json;
  base::JSONWriter::Write(dict.get(), &json);
  return json;
}

ModelTypeSet GetRoutingInfoTypes(const ModelSafeRoutingInfo& routing_info) {
  ModelTypeSet types;
  for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
       it != routing_info.end(); ++it) {
    types.Put(it->first);
  }
  return types;
}

ModelSafeGroup GetGroupForModelType(const ModelType type,
                                    const ModelSafeRoutingInfo& routes) {
  ModelSafeRoutingInfo::const_iterator it = routes.find(type);
  if (it == routes.end()) {
    if (type != UNSPECIFIED && type != TOP_LEVEL_FOLDER)
      DVLOG(1) << "Entry does not belong to active ModelSafeGroup!";
    return GROUP_PASSIVE;
  }
  return it->second;
}

std::string ModelSafeGroupToString(ModelSafeGroup group) {
  switch (group) {
    case GROUP_UI:
      return "GROUP_UI";
    case GROUP_DB:
      return "GROUP_DB";
    case GROUP_FILE:
      return "GROUP_FILE";
    case GROUP_HISTORY:
      return "GROUP_HISTORY";
    case GROUP_PASSIVE:
      return "GROUP_PASSIVE";
    case GROUP_PASSWORD:
      return "GROUP_PASSWORD";
    default:
      NOTREACHED();
      return "INVALID";
  }
}

ModelSafeWorker::ModelSafeWorker(WorkerLoopDestructionObserver* observer)
    : stopped_(false),
      work_done_or_stopped_(false, false),
      observer_(observer),
      working_loop_(NULL) {
}

ModelSafeWorker::~ModelSafeWorker() {}

void ModelSafeWorker::RequestStop() {
  base::AutoLock al(stopped_lock_);

  // Set stop flag but don't signal work_done_or_stopped_ to unblock sync loop
  // because the worker may be working and depending on sync command object
  // living on sync thread. his prevents any *further* tasks from being posted
  // to worker threads (see DoWorkAndWaitUntilDone below), but note that one
  // may already be posted.
  stopped_ = true;
}

SyncerError ModelSafeWorker::DoWorkAndWaitUntilDone(const WorkCallback& work) {
  {
    base::AutoLock al(stopped_lock_);
    if (stopped_)
      return CANNOT_DO_WORK;

    CHECK(!work_done_or_stopped_.IsSignaled());
  }

  return DoWorkAndWaitUntilDoneImpl(work);
}

bool ModelSafeWorker::IsStopped() {
  base::AutoLock al(stopped_lock_);
  return stopped_;
}

void ModelSafeWorker::WillDestroyCurrentMessageLoop() {
  {
    base::AutoLock al(stopped_lock_);
    stopped_ = true;

    // Must signal to unblock syncer if it's waiting for a posted task to
    // finish. At this point, all pending tasks posted to the loop have been
    // destroyed (see MessageLoop::~MessageLoop). So syncer will be blocked
    // indefinitely without signaling here.
    work_done_or_stopped_.Signal();

    DVLOG(1) << ModelSafeGroupToString(GetModelSafeGroup())
        << " worker stops on destruction of its working thread.";
  }

  {
    base::AutoLock l(working_loop_lock_);
    working_loop_ = NULL;
  }

  if (observer_)
    observer_->OnWorkerLoopDestroyed(GetModelSafeGroup());
}

void ModelSafeWorker::SetWorkingLoopToCurrent() {
  base::Callback<void(ModelSafeGroup)> unregister_done_callback;

  {
    base::AutoLock l(working_loop_lock_);
    DCHECK(!working_loop_);

    if (unregister_done_callback_.is_null()) {
      // Expected case - UnregisterForLoopDestruction hasn't been called yet.
      base::MessageLoop::current()->AddDestructionObserver(this);
      working_loop_ = base::MessageLoop::current();
    } else {
      // Rare case which is possible when the model type thread remains
      // blocked for the entire session and UnregisterForLoopDestruction ends
      // up being called before this method. This method is posted unlike
      // UnregisterForLoopDestruction - that's why they can end up being called
      // out of order.
      // In this case we skip the destruction observer registration
      // and just invoke the callback stored at UnregisterForLoopDestruction.
      DCHECK(stopped_);
      unregister_done_callback = unregister_done_callback_;
      unregister_done_callback_.Reset();
    }
  }

  if (!unregister_done_callback.is_null()) {
    unregister_done_callback.Run(GetModelSafeGroup());
  }
}

void ModelSafeWorker::UnregisterForLoopDestruction(
    base::Callback<void(ModelSafeGroup)> unregister_done_callback) {
  base::AutoLock l(working_loop_lock_);
  if (working_loop_ != NULL) {
    // Normal case - observer registration has been already done.
    // Delegate to the sync thread to do the actual unregistration in
    // UnregisterForLoopDestructionAsync.
    DCHECK_NE(base::MessageLoop::current(), working_loop_);
    working_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ModelSafeWorker::UnregisterForLoopDestructionAsync,
                   this,
                   unregister_done_callback));
  } else {
    // The working loop is still unknown, probably because the model type
    // thread is blocked. Store the callback to be called from
    // SetWorkingLoopToCurrent.
    unregister_done_callback_ = unregister_done_callback;
  }
}

void ModelSafeWorker::UnregisterForLoopDestructionAsync(
    base::Callback<void(ModelSafeGroup)> unregister_done_callback) {
  {
    base::AutoLock l(working_loop_lock_);
    if (!working_loop_)
      return;
    DCHECK_EQ(base::MessageLoop::current(), working_loop_);
  }

  DCHECK(stopped_);
  base::MessageLoop::current()->RemoveDestructionObserver(this);
  unregister_done_callback.Run(GetModelSafeGroup());
}

}  // namespace syncer
