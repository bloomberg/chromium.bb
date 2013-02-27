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
#include "webkit/fileapi/syncable/local_origin_change_observer.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/syncable_file_operation_runner.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

using fileapi::FileSystemContext;
using fileapi::FileSystemFileUtil;
using fileapi::FileSystemOperation;
using fileapi::FileSystemOperationContext;
using fileapi::FileSystemURL;
using fileapi::LocalFileSystemOperation;

namespace sync_file_system {

namespace {
const int kMaxConcurrentSyncableOperation = 3;
const int kNotifyChangesDurationInSec = 1;
const int kMaxURLsToFetchForLocalSync = 5;
}  // namespace

LocalFileSyncContext::LocalFileSyncContext(
    base::SingleThreadTaskRunner* ui_task_runner,
    base::SingleThreadTaskRunner* io_task_runner)
    : ui_task_runner_(ui_task_runner),
      io_task_runner_(io_task_runner),
      shutdown_on_ui_(false),
      mock_notify_changes_duration_in_sec_(-1) {
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
    ui_task_runner_->PostTask(FROM_HERE,
                              base::Bind(callback,
                                         SYNC_STATUS_OK));
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

void LocalFileSyncContext::GetFileForLocalSync(
    FileSystemContext* file_system_context,
    const LocalFileSyncInfoCallback& callback) {
  DCHECK(file_system_context);
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  std::deque<FileSystemURL>* urls = new std::deque<FileSystemURL>;
  file_system_context->task_runners()->file_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&LocalFileSyncContext::GetNextURLsForSyncOnFileThread,
                 this, make_scoped_refptr(file_system_context),
                 base::Unretained(urls)),
      base::Bind(&LocalFileSyncContext::TryPrepareForLocalSync,
                 this, make_scoped_refptr(file_system_context),
                 base::Owned(urls), callback));
}

void LocalFileSyncContext::ClearChangesForURL(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    const base::Closure& done_callback) {
  // This is initially called on UI thread and to be relayed to FILE thread.
  DCHECK(file_system_context);
  if (!file_system_context->task_runners()->file_task_runner()->
          RunsTasksOnCurrentThread()) {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    file_system_context->task_runners()->file_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::ClearChangesForURL,
                   this, make_scoped_refptr(file_system_context),
                   url, done_callback));
    return;
  }
  DCHECK(file_system_context->change_tracker());
  file_system_context->change_tracker()->ClearChangesForURL(url);

  // Call the completion callback on UI thread.
  ui_task_runner_->PostTask(FROM_HERE, done_callback);
}

void LocalFileSyncContext::ClearSyncFlagForURL(const FileSystemURL& url) {
  // This is initially called on UI thread and to be relayed to IO thread.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalFileSyncContext::EnableWritingOnIOThread,
                 this, url));
}

void LocalFileSyncContext::PrepareForSync(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    const LocalFileSyncInfoCallback& callback) {
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
  const bool syncable = sync_status()->IsSyncable(url);
  // Disable writing if it's ready to be synced.
  if (syncable)
    sync_status()->StartSyncing(url);
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalFileSyncContext::DidGetWritingStatusForSync,
                 this, make_scoped_refptr(file_system_context),
                 syncable ? SYNC_STATUS_OK :
                            SYNC_STATUS_FILE_BUSY,
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
    const base::FilePath& local_path,
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
                 this, url, callback);
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

void LocalFileSyncContext::RecordFakeLocalChange(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    const FileChange& change,
    const SyncStatusCallback& callback) {
  // This is called on UI thread and to be relayed to FILE thread.
  DCHECK(file_system_context);
  if (!file_system_context->task_runners()->file_task_runner()->
          RunsTasksOnCurrentThread()) {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    file_system_context->task_runners()->file_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::RecordFakeLocalChange,
                   this, make_scoped_refptr(file_system_context),
                   url, change, callback));
    return;
  }

  DCHECK(file_system_context->change_tracker());
  file_system_context->change_tracker()->MarkDirtyOnDatabase(url);
  file_system_context->change_tracker()->RecordChange(url, change);

  // Fire the callback on UI thread.
  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(callback,
                                       SYNC_STATUS_OK));
}

void LocalFileSyncContext::GetFileMetadata(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    const SyncFileMetadataCallback& callback) {
  // This is initially called on UI thread and to be relayed to IO thread.
  if (!io_task_runner_->RunsTasksOnCurrentThread()) {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::GetFileMetadata, this,
                   make_scoped_refptr(file_system_context), url, callback));
    return;
  }
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  LocalFileSystemOperation* operation = CreateFileSystemOperationForSync(
      file_system_context);
  DCHECK(operation);
  operation->GetMetadata(
      url, base::Bind(&LocalFileSyncContext::DidGetFileMetadata,
                      this, callback));
}

void LocalFileSyncContext::HasPendingLocalChanges(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    const HasPendingLocalChangeCallback& callback) {
  // This gets called on UI thread and relays the task on FILE thread.
  DCHECK(file_system_context);
  if (!file_system_context->task_runners()->file_task_runner()->
          RunsTasksOnCurrentThread()) {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    file_system_context->task_runners()->file_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::HasPendingLocalChanges,
                   this, make_scoped_refptr(file_system_context),
                   url, callback));
    return;
  }

  DCHECK(file_system_context->change_tracker());
  FileChangeList changes;
  file_system_context->change_tracker()->GetChangesForURL(url, &changes);

  // Fire the callback on UI thread.
  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(callback,
                                       SYNC_STATUS_OK,
                                       !changes.empty()));
}

void LocalFileSyncContext::AddOriginChangeObserver(
    LocalOriginChangeObserver* observer) {
  origin_change_observers_.AddObserver(observer);
}

void LocalFileSyncContext::RemoveOriginChangeObserver(
    LocalOriginChangeObserver* observer) {
  origin_change_observers_.RemoveObserver(observer);
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
  origins_with_pending_changes_.insert(url.origin());
  ScheduleNotifyChangesUpdatedOnIOThread();
  if (url_syncable_callback_.is_null() ||
      sync_status()->IsWriting(url_waiting_sync_on_io_)) {
    return;
  }
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

void LocalFileSyncContext::ScheduleNotifyChangesUpdatedOnIOThread() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  if (base::Time::Now() > last_notified_changes_ + NotifyChangesDuration()) {
    NotifyAvailableChangesOnIOThread();
  } else if (!timer_on_io_->IsRunning()) {
    timer_on_io_->Start(
        FROM_HERE, NotifyChangesDuration(), this,
        &LocalFileSyncContext::NotifyAvailableChangesOnIOThread);
  }
}

void LocalFileSyncContext::NotifyAvailableChangesOnIOThread() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalFileSyncContext::NotifyAvailableChanges,
                 this, origins_with_pending_changes_));
  last_notified_changes_ = base::Time::Now();
  origins_with_pending_changes_.clear();
}

void LocalFileSyncContext::NotifyAvailableChanges(
    const std::set<GURL>& origins) {
  FOR_EACH_OBSERVER(LocalOriginChangeObserver, origin_change_observers_,
                    OnChangesAvailableInOrigins(origins));
}

void LocalFileSyncContext::ShutdownOnIOThread() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  operation_runner_.reset();
  sync_status_.reset();
  timer_on_io_.reset();
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
    std::set<GURL>* origins_with_changes = new std::set<GURL>;
    scoped_ptr<LocalFileChangeTracker>* tracker_ptr(
        new scoped_ptr<LocalFileChangeTracker>);
    base::PostTaskAndReplyWithResult(
        file_system_context->task_runners()->file_task_runner(),
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::InitializeChangeTrackerOnFileThread,
                   this, tracker_ptr,
                   make_scoped_refptr(file_system_context),
                   origins_with_changes),
        base::Bind(&LocalFileSyncContext::DidInitializeChangeTrackerOnIOThread,
                   this, base::Owned(tracker_ptr),
                   source_url, service_name,
                   make_scoped_refptr(file_system_context),
                   base::Owned(origins_with_changes)));
    return;
  }
  if (!operation_runner_.get()) {
    DCHECK(!sync_status_);
    DCHECK(!timer_on_io_);
    sync_status_.reset(new LocalFileSyncStatus);
    timer_on_io_.reset(new base::OneShotTimer<LocalFileSyncContext>);
    operation_runner_.reset(new SyncableFileOperationRunner(
            kMaxConcurrentSyncableOperation,
            sync_status_.get()));
    sync_status_->AddObserver(this);
  }
  file_system_context->set_sync_context(this);
  DidInitialize(source_url, file_system_context,
                SYNC_STATUS_OK);
}

SyncStatusCode LocalFileSyncContext::InitializeChangeTrackerOnFileThread(
    scoped_ptr<LocalFileChangeTracker>* tracker_ptr,
    FileSystemContext* file_system_context,
    std::set<GURL>* origins_with_changes) {
  DCHECK(file_system_context);
  DCHECK(tracker_ptr);
  DCHECK(origins_with_changes);
  tracker_ptr->reset(new LocalFileChangeTracker(
          file_system_context->partition_path(),
          file_system_context->task_runners()->file_task_runner()));
  const SyncStatusCode status = (*tracker_ptr)->Initialize(file_system_context);
  if (status != SYNC_STATUS_OK)
    return status;

  // Get all origins that have pending changes.
  std::deque<FileSystemURL> urls;
  (*tracker_ptr)->GetNextChangedURLs(&urls, 0);
  for (std::deque<FileSystemURL>::iterator iter = urls.begin();
       iter != urls.end(); ++iter) {
    origins_with_changes->insert(iter->origin());
  }
  return status;
}

void LocalFileSyncContext::DidInitializeChangeTrackerOnIOThread(
    scoped_ptr<LocalFileChangeTracker>* tracker_ptr,
    const GURL& source_url,
    const std::string& service_name,
    FileSystemContext* file_system_context,
    std::set<GURL>* origins_with_changes,
    SyncStatusCode status) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(file_system_context);
  DCHECK(origins_with_changes);
  if (status != SYNC_STATUS_OK) {
    DidInitialize(source_url, file_system_context, status);
    return;
  }
  file_system_context->SetLocalFileChangeTracker(tracker_ptr->Pass());

  origins_with_pending_changes_.insert(origins_with_changes->begin(),
                                       origins_with_changes->end());
  ScheduleNotifyChangesUpdatedOnIOThread();

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

void LocalFileSyncContext::GetNextURLsForSyncOnFileThread(
    FileSystemContext* file_system_context,
    std::deque<FileSystemURL>* urls) {
  DCHECK(file_system_context);
  DCHECK(file_system_context->task_runners()->file_task_runner()->
             RunsTasksOnCurrentThread());
  DCHECK(file_system_context->change_tracker());
  file_system_context->change_tracker()->GetNextChangedURLs(
      urls, kMaxURLsToFetchForLocalSync);
}

void LocalFileSyncContext::TryPrepareForLocalSync(
    FileSystemContext* file_system_context,
    std::deque<FileSystemURL>* urls,
    const LocalFileSyncInfoCallback& callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(urls);

  if (urls->empty()) {
    callback.Run(SYNC_STATUS_NO_CHANGE_TO_SYNC,
                 LocalFileSyncInfo());
    return;
  }

  const FileSystemURL url = urls->front();
  urls->pop_front();
  std::deque<FileSystemURL>* remaining = new std::deque<FileSystemURL>;
  remaining->swap(*urls);

  PrepareForSync(
      file_system_context, url,
      base::Bind(&LocalFileSyncContext::DidTryPrepareForLocalSync,
                 this, make_scoped_refptr(file_system_context),
                 base::Owned(remaining), callback));
}

void LocalFileSyncContext::DidTryPrepareForLocalSync(
    FileSystemContext* file_system_context,
    std::deque<FileSystemURL>* remaining_urls,
    const LocalFileSyncInfoCallback& callback,
    SyncStatusCode status,
    const LocalFileSyncInfo& sync_file_info) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  if (status != SYNC_STATUS_FILE_BUSY) {
    callback.Run(status, sync_file_info);
    return;
  }
  // Recursively call TryPrepareForLocalSync with remaining_urls.
  TryPrepareForLocalSync(file_system_context, remaining_urls, callback);
}

void LocalFileSyncContext::DidGetWritingStatusForSync(
    FileSystemContext* file_system_context,
    SyncStatusCode status,
    const FileSystemURL& url,
    const LocalFileSyncInfoCallback& callback) {
  // This gets called on UI thread and relays the task on FILE thread.
  DCHECK(file_system_context);
  if (!file_system_context->task_runners()->file_task_runner()->
          RunsTasksOnCurrentThread()) {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    if (shutdown_on_ui_) {
      callback.Run(SYNC_STATUS_ABORT, LocalFileSyncInfo());
      return;
    }
    file_system_context->task_runners()->file_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::DidGetWritingStatusForSync,
                   this, make_scoped_refptr(file_system_context),
                   status, url, callback));
    return;
  }

  DCHECK(file_system_context->change_tracker());
  FileChangeList changes;
  file_system_context->change_tracker()->GetChangesForURL(url, &changes);

  base::FilePath platform_path;
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

  LocalFileSyncInfo sync_file_info;
  sync_file_info.url = url;
  sync_file_info.local_file_path = platform_path;
  sync_file_info.metadata.file_type = file_type;
  sync_file_info.metadata.size = file_info.size;
  sync_file_info.metadata.last_modified = file_info.last_modified;
  sync_file_info.changes = changes;

  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(callback, status, sync_file_info));
}

void LocalFileSyncContext::EnableWritingOnIOThread(
    const FileSystemURL& url) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  sync_status()->EndSyncing(url);
  // Since a sync has finished the number of changes must have been updated.
  origins_with_pending_changes_.insert(url.origin());
  ScheduleNotifyChangesUpdatedOnIOThread();
}

void LocalFileSyncContext::DidApplyRemoteChange(
    const FileSystemURL& url,
    const SyncStatusCallback& callback_on_ui,
    base::PlatformFileError file_error) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(callback_on_ui,
                 PlatformFileErrorToSyncStatusCode(file_error)));
  EnableWritingOnIOThread(url);
}

void LocalFileSyncContext::DidGetFileMetadata(
    const SyncFileMetadataCallback& callback,
    base::PlatformFileError file_error,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  SyncFileMetadata metadata;
  if (file_error == base::PLATFORM_FILE_OK) {
    metadata.file_type = file_info.is_directory ?
        SYNC_FILE_TYPE_DIRECTORY : SYNC_FILE_TYPE_FILE;
    metadata.size = file_info.size;
    metadata.last_modified = file_info.last_modified;
  }
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 PlatformFileErrorToSyncStatusCode(file_error),
                 metadata));
}

base::TimeDelta LocalFileSyncContext::NotifyChangesDuration() {
  if (mock_notify_changes_duration_in_sec_ >= 0)
    return base::TimeDelta::FromSeconds(mock_notify_changes_duration_in_sec_);
  return base::TimeDelta::FromSeconds(kNotifyChangesDurationInSec);
}

}  // namespace sync_file_system
