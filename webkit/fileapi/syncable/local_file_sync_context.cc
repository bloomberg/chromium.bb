// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_sync_context.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"

namespace fileapi {

LocalFileSyncContext::LocalFileSyncContext(
    base::SingleThreadTaskRunner* ui_task_runner,
    base::SingleThreadTaskRunner* io_task_runner)
    : ui_task_runner_(ui_task_runner),
      io_task_runner_(io_task_runner),
      shutdown_(false) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
}

void LocalFileSyncContext::MaybeInitializeFileSystemContext(
    const GURL& source_url,
    FileSystemContext* file_system_context,
    const StatusCallback& callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  if (ContainsKey(file_system_contexts_, file_system_context)) {
    DCHECK(!ContainsKey(origin_to_contexts_, source_url) ||
           origin_to_contexts_[source_url] == file_system_context);
    origin_to_contexts_[source_url] = file_system_context;
    // The context has been already initialized. Just dispatch the callback
    // with SYNC_STATUS_OK.
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  StatusCallbackQueue& callback_queue =
      pending_initialize_callbacks_[file_system_context];
  callback_queue.push_back(callback);
  if (callback_queue.size() > 1)
    return;

  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalFileSyncContext::InitializeFileSystemContextOnIOThread,
                 this, source_url, make_scoped_refptr(file_system_context)));
}

void LocalFileSyncContext::ShutdownOnUIThread() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  shutdown_ = true;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalFileSyncContext::ShutdownOnIOThread,
                 this));
}

LocalFileSyncContext::~LocalFileSyncContext() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(shutdown_);
}

void LocalFileSyncContext::ShutdownOnIOThread() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
}

void LocalFileSyncContext::InitializeFileSystemContextOnIOThread(
    const GURL& source_url,
    FileSystemContext* file_system_context) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(file_system_context);
  if (!file_system_context->change_tracker()) {
    // Create and initialize LocalFileChangeTracker and call back this method
    // later again.
    scoped_ptr<LocalFileChangeTracker>* tracker_ptr(
        new scoped_ptr<LocalFileChangeTracker>);
    base::PostTaskAndReplyWithResult(
        file_system_context->task_runners()->file_task_runner(),
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::InitializeChangeTrackerOnFileThread,
                   this, tracker_ptr,
                   make_scoped_refptr(file_system_context)),
        base::Bind(&LocalFileSyncContext::DidInitializeChangeTracker, this,
                   base::Owned(tracker_ptr),
                   source_url,
                   make_scoped_refptr(file_system_context)));
    return;
  }
  file_system_context->set_sync_context(this);
  DidInitialize(source_url, file_system_context, SYNC_STATUS_OK);
}

SyncStatusCode LocalFileSyncContext::InitializeChangeTrackerOnFileThread(
    scoped_ptr<LocalFileChangeTracker>* tracker_ptr,
    FileSystemContext* file_system_context) {
  DCHECK(file_system_context);
  DCHECK(tracker_ptr);
  tracker_ptr->reset(new LocalFileChangeTracker(
          file_system_context->partition_path(),
          file_system_context->task_runners()->file_task_runner()));
  return (*tracker_ptr)->Initialize(file_system_context);
}

void LocalFileSyncContext::DidInitializeChangeTracker(
    scoped_ptr<LocalFileChangeTracker>* tracker_ptr,
    const GURL& source_url,
    FileSystemContext* file_system_context,
    SyncStatusCode status) {
  DCHECK(file_system_context);
  if (status != SYNC_STATUS_OK) {
    DidInitialize(source_url, file_system_context, status);
    return;
  }
  file_system_context->SetLocalFileChangeTracker(tracker_ptr->Pass());
  InitializeFileSystemContextOnIOThread(source_url, file_system_context);
}

void LocalFileSyncContext::DidInitialize(
    const GURL& source_url,
    FileSystemContext* file_system_context,
    SyncStatusCode status) {
  if (!ui_task_runner_->RunsTasksOnCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::DidInitialize,
                   this, source_url,
                   make_scoped_refptr(file_system_context), status));
    return;
  }
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!ContainsKey(file_system_contexts_, file_system_context));
  DCHECK(ContainsKey(pending_initialize_callbacks_, file_system_context));
  DCHECK(file_system_context->change_tracker());

  file_system_contexts_.insert(file_system_context);

  DCHECK(!ContainsKey(origin_to_contexts_, source_url));
  origin_to_contexts_[source_url] = file_system_context;

  StatusCallbackQueue& callback_queue =
      pending_initialize_callbacks_[file_system_context];
  for (StatusCallbackQueue::iterator iter = callback_queue.begin();
       iter != callback_queue.end(); ++iter) {
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(*iter, status));
  }
  pending_initialize_callbacks_.erase(file_system_context);
}

}  // namespace fileapi
