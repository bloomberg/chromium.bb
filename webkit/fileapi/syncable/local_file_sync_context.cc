// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_sync_context.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/platform_file.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/fileapi/syncable/syncable_file_operation_runner.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

namespace fileapi {

namespace {
const int kMaxConcurrentSyncableOperation = 3;
}  // namespace

LocalFileSyncContext::LocalFileSyncContext(
    base::SingleThreadTaskRunner* ui_task_runner,
    base::SingleThreadTaskRunner* io_task_runner)
    : ui_task_runner_(ui_task_runner),
      io_task_runner_(io_task_runner),
      shutdown_on_ui_(false) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
}

void LocalFileSyncContext::MaybeInitializeFileSystemContext(
    const GURL& source_url,
    const std::string& service_name,
    FileSystemContext* file_system_context,
    const SyncStatusCallback& callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  if (ContainsKey(file_system_contexts_, file_system_context)) {
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
                 this, source_url, service_name,
                 make_scoped_refptr(file_system_context)));
}

void LocalFileSyncContext::ShutdownOnUIThread() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  shutdown_on_ui_ = true;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalFileSyncContext::ShutdownOnIOThread,
                 this));
}

void LocalFileSyncContext::PrepareForSync(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    const ChangeListCallback& callback) {
  // This is initially called on UI thread and to be relayed to IO thread.
  if (!io_task_runner_->RunsTasksOnCurrentThread()) {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::PrepareForSync, this,
                   make_scoped_refptr(file_system_context), url, callback));
    return;
  }
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  const bool writing = sync_status()->IsWriting(url);
  // Disable writing if it's ready to be synced.
  if (!writing)
    sync_status()->StartSyncing(url);
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalFileSyncContext::DidGetWritingStatusForPrepareForSync,
                 this, make_scoped_refptr(file_system_context),
                 writing ? SYNC_STATUS_FILE_BUSY : SYNC_STATUS_OK,
                 url, callback));
}

void LocalFileSyncContext::RegisterURLForWaitingSync(
    const FileSystemURL& url,
    const base::Closure& on_syncable_callback) {
  // This is initially called on UI thread and to be relayed to IO thread.
  if (!io_task_runner_->RunsTasksOnCurrentThread()) {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::RegisterURLForWaitingSync,
                   this, url, on_syncable_callback));
    return;
  }
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  if (sync_status()->IsWritable(url)) {
    // No need to register; fire the callback now.
    ui_task_runner_->PostTask(FROM_HERE, on_syncable_callback);
    return;
  }
  url_waiting_sync_on_io_ = url;
  url_syncable_callback_ = on_syncable_callback;
}

void LocalFileSyncContext::ApplyRemoteChange(
    FileSystemContext* file_system_context,
    const FileChange& change,
    const FilePath& local_path,
    const FileSystemURL& url,
    const SyncStatusCallback& callback) {
  if (!io_task_runner_->RunsTasksOnCurrentThread()) {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::ApplyRemoteChange, this,
                   make_scoped_refptr(file_system_context),
                   change, local_path, url, callback));
    return;
  }
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!sync_status()->IsWritable(url));
  DCHECK(!sync_status()->IsWriting(url));
  LocalFileSystemOperation* operation = CreateFileSystemOperationForSync(
      file_system_context);
  DCHECK(operation);
  FileSystemOperation::StatusCallback operation_callback =
      base::Bind(&LocalFileSyncContext::DidApplyRemoteChange,
                 this, callback);
  switch (change.change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      switch (change.file_type()) {
        case SYNC_FILE_TYPE_FILE:
          DCHECK(!local_path.empty());
          operation->CopyInForeignFile(local_path, url, operation_callback);
          break;
        case SYNC_FILE_TYPE_DIRECTORY:
          operation->CreateDirectory(
              url, false /* exclusive */, true /* recursive */,
              operation_callback);
          break;
        case SYNC_FILE_TYPE_UNKNOWN:
          NOTREACHED() << "File type unknown for ADD_OR_UPDATE change";
      }
      break;
    case FileChange::FILE_CHANGE_DELETE:
      operation->Remove(url, true /* recursive */, operation_callback);
      break;
  }
}

base::WeakPtr<SyncableFileOperationRunner>
LocalFileSyncContext::operation_runner() const {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  if (operation_runner_.get())
    return operation_runner_->AsWeakPtr();
  return base::WeakPtr<SyncableFileOperationRunner>();
}

LocalFileSyncStatus* LocalFileSyncContext::sync_status() const {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  return sync_status_.get();
}

void LocalFileSyncContext::OnSyncEnabled(const FileSystemURL& url) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  if (url_syncable_callback_.is_null() ||
      sync_status()->IsWriting(url_waiting_sync_on_io_))
    return;
  // TODO(kinuko): may want to check how many pending tasks we have.
  sync_status()->StartSyncing(url_waiting_sync_on_io_);
  ui_task_runner_->PostTask(FROM_HERE, url_syncable_callback_);
  url_syncable_callback_.Reset();
}

void LocalFileSyncContext::OnWriteEnabled(const FileSystemURL& url) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  // Nothing to do for now.
}

LocalFileSyncContext::~LocalFileSyncContext() {
}

void LocalFileSyncContext::ShutdownOnIOThread() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  operation_runner_.reset();
  sync_status_.reset();
}

void LocalFileSyncContext::InitializeFileSystemContextOnIOThread(
    const GURL& source_url,
    const std::string& service_name,
    FileSystemContext* file_system_context) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(file_system_context);
  if (!file_system_context->change_tracker()) {
    // First registers the service name.
    RegisterSyncableFileSystem(service_name);
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
                   source_url, service_name,
                   make_scoped_refptr(file_system_context)));
    return;
  }
  if (!operation_runner_.get()) {
    DCHECK(!sync_status_.get());
    sync_status_.reset(new LocalFileSyncStatus);
    operation_runner_.reset(new SyncableFileOperationRunner(
            kMaxConcurrentSyncableOperation,
            sync_status_.get()));
    sync_status_->AddObserver(this);
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
    const std::string& service_name,
    FileSystemContext* file_system_context,
    SyncStatusCode status) {
  DCHECK(file_system_context);
  if (status != SYNC_STATUS_OK) {
    DidInitialize(source_url, file_system_context, status);
    return;
  }
  file_system_context->SetLocalFileChangeTracker(tracker_ptr->Pass());
  InitializeFileSystemContextOnIOThread(source_url, service_name,
                                        file_system_context);
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

  StatusCallbackQueue& callback_queue =
      pending_initialize_callbacks_[file_system_context];
  for (StatusCallbackQueue::iterator iter = callback_queue.begin();
       iter != callback_queue.end(); ++iter) {
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(*iter, status));
  }
  pending_initialize_callbacks_.erase(file_system_context);
}

void LocalFileSyncContext::DidGetWritingStatusForPrepareForSync(
    FileSystemContext* file_system_context,
    SyncStatusCode status,
    const FileSystemURL& url,
    const ChangeListCallback& callback) {
  // This gets called on UI thread and relays the task on FILE thread.
  DCHECK(file_system_context);
  if (!file_system_context->task_runners()->file_task_runner()->
          RunsTasksOnCurrentThread()) {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    if (shutdown_on_ui_) {
      callback.Run(SYNC_STATUS_ABORT, SYNC_FILE_TYPE_UNKNOWN, FileChangeList());
      return;
    }
    file_system_context->task_runners()->file_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::DidGetWritingStatusForPrepareForSync,
                   this, make_scoped_refptr(file_system_context),
                   status, url, callback));
    return;
  }

  DCHECK(file_system_context->change_tracker());
  FileChangeList changes;
  file_system_context->change_tracker()->GetChangesForURL(url, &changes);

  FilePath platform_path;
  base::PlatformFileInfo file_info;
  FileSystemFileUtil* file_util = file_system_context->GetFileUtil(url.type());
  DCHECK(file_util);
  base::PlatformFileError file_error = file_util->GetFileInfo(
      make_scoped_ptr(
          new FileSystemOperationContext(file_system_context)).get(),
      url,
      &file_info,
      &platform_path);
  if (status == SYNC_STATUS_OK &&
      file_error != base::PLATFORM_FILE_OK &&
      file_error != base::PLATFORM_FILE_ERROR_NOT_FOUND)
    status = PlatformFileErrorToSyncStatusCode(file_error);

  DCHECK(!file_info.is_symbolic_link);

  SyncFileType file_type = SYNC_FILE_TYPE_FILE;
  if (file_error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
    file_type = SYNC_FILE_TYPE_UNKNOWN;
  else if (file_info.is_directory)
    file_type = SYNC_FILE_TYPE_DIRECTORY;

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(callback, status, file_type, changes));
}

void LocalFileSyncContext::DidApplyRemoteChange(
    const SyncStatusCallback& callback_on_ui,
    base::PlatformFileError file_error) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(callback_on_ui,
                 PlatformFileErrorToSyncStatusCode(file_error)));
}

}  // namespace fileapi
