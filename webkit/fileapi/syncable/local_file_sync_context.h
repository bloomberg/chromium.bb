// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_SYNC_CONTEXT_H_
#define WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_SYNC_CONTEXT_H_

#include <deque>
#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/syncable/local_file_sync_status.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"
#include "webkit/fileapi/syncable/sync_status_code.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace sync_file_system {
class FileChange;
}

namespace fileapi {

class FileSystemContext;
class LocalFileChangeTracker;
struct LocalFileSyncInfo;
class LocalOriginChangeObserver;
class SyncableFileOperationRunner;

// This class works as a bridge between LocalFileSyncService (which is a
// per-profile object) and FileSystemContext's (which is a per-storage-partition
// object and may exist multiple in a profile).
// An instance of this class is shared by FileSystemContexts and outlives
// LocalFileSyncService.
class WEBKIT_STORAGE_EXPORT LocalFileSyncContext
    : public base::RefCountedThreadSafe<LocalFileSyncContext>,
      public LocalFileSyncStatus::Observer {
 public:
  typedef base::Callback<void(SyncStatusCode status,
                              const LocalFileSyncInfo& sync_file_info)>
      LocalFileSyncInfoCallback;

  typedef base::Callback<void(fileapi::SyncStatusCode status,
                              bool has_pending_changes)>
      HasPendingLocalChangeCallback;

  LocalFileSyncContext(base::SingleThreadTaskRunner* ui_task_runner,
                       base::SingleThreadTaskRunner* io_task_runner);

  // Initializes |file_system_context| for syncable file operations for
  // |service_name| and registers the it into the internal map.
  // Calling this multiple times for the same file_system_context is valid.
  // This method must be called on UI thread.
  void MaybeInitializeFileSystemContext(const GURL& source_url,
                                        const std::string& service_name,
                                        FileSystemContext* file_system_context,
                                        const SyncStatusCallback& callback);

  // Called when the corresponding LocalFileSyncService exits.
  // This method must be called on UI thread.
  void ShutdownOnUIThread();

  // Picks a file for next local sync and returns it after disabling writes
  // for the file.
  // This method must be called on UI thread.
  void GetFileForLocalSync(FileSystemContext* file_system_context,
                           const LocalFileSyncInfoCallback& callback);

  // Clears all pending local changes for |url|. |done_callback| is called
  // when the changes are cleared.
  // This method must be called on UI thread.
  void ClearChangesForURL(FileSystemContext* file_system_context,
                          const FileSystemURL& url,
                          const base::Closure& done_callback);

  // A local or remote sync has been finished (either successfully or
  // with an error). Clears the internal sync flag and enable writing for |url|.
  // This method must be called on UI thread.
  void ClearSyncFlagForURL(const FileSystemURL& url);

  // Prepares for sync |url| by disabling writes on |url|.
  // If the target |url| is being written and cannot start sync it
  // returns SYNC_STATUS_WRITING status code via |callback|.
  // Otherwise it disables writes, marks the |url| syncing and returns
  // the current change set made on |url|.
  // This method must be called on UI thread.
  void PrepareForSync(FileSystemContext* file_system_context,
                      const FileSystemURL& url,
                      const LocalFileSyncInfoCallback& callback);

  // Registers |url| to wait until sync is enabled for |url|.
  // |on_syncable_callback| is to be called when |url| becomes syncable
  // (i.e. when we have no pending writes and the file is successfully locked
  // for sync).
  //
  // Calling this method again while this already has another URL waiting
  // for sync will overwrite the previously registered URL.
  //
  // This method must be called on UI thread.
  void RegisterURLForWaitingSync(const FileSystemURL& url,
                                 const base::Closure& on_syncable_callback);

  // Applies a remote change.
  // This method must be called on UI thread.
  void ApplyRemoteChange(
      FileSystemContext* file_system_context,
      const sync_file_system::FileChange& change,
      const base::FilePath& local_path,
      const FileSystemURL& url,
      const SyncStatusCallback& callback);

  // Records a fake local change in the local change tracker.
  void RecordFakeLocalChange(
      FileSystemContext* file_system_context,
      const fileapi::FileSystemURL& url,
      const sync_file_system::FileChange& change,
      const fileapi::SyncStatusCallback& callback);

  // This must be called on UI thread.
  void GetFileMetadata(
      FileSystemContext* file_system_context,
      const FileSystemURL& url,
      const SyncFileMetadataCallback& callback);

  // Returns true via |callback| if the given file |url| has local pending
  // changes.
  void HasPendingLocalChanges(
      FileSystemContext* file_system_context,
      const FileSystemURL& url,
      const HasPendingLocalChangeCallback& callback);

  // They must be called on UI thread.
  void AddOriginChangeObserver(LocalOriginChangeObserver* observer);
  void RemoveOriginChangeObserver(LocalOriginChangeObserver* observer);

  // OperationRunner is accessible only on IO thread.
  base::WeakPtr<SyncableFileOperationRunner> operation_runner() const;

  // SyncContext is accessible only on IO thread.
  LocalFileSyncStatus* sync_status() const;

  // For testing; override the duration to notify changes from the
  // default value.
  void set_mock_notify_changes_duration_in_sec(int duration) {
    mock_notify_changes_duration_in_sec_ = duration;
  }

 protected:
  // LocalFileSyncStatus::Observer overrides. They are called on IO thread.
  virtual void OnSyncEnabled(const FileSystemURL& url) OVERRIDE;
  virtual void OnWriteEnabled(const FileSystemURL& url) OVERRIDE;

 private:
  typedef std::deque<SyncStatusCallback> StatusCallbackQueue;
  friend class base::RefCountedThreadSafe<LocalFileSyncContext>;
  friend class CannedSyncableFileSystem;

  virtual ~LocalFileSyncContext();

  void ShutdownOnIOThread();

  // Starts a timer to eventually call NotifyAvailableChangesOnIOThread.
  // The caller is expected to update origins_with_pending_changes_ before
  // calling this.
  void ScheduleNotifyChangesUpdatedOnIOThread();

  // Called by the internal timer on IO thread to notify changes to UI thread.
  void NotifyAvailableChangesOnIOThread();

  // Called from NotifyAvailableChangesOnIOThread.
  void NotifyAvailableChanges(const std::set<GURL>& origins);

  // Helper routines for MaybeInitializeFileSystemContext.
  void InitializeFileSystemContextOnIOThread(
      const GURL& source_url,
      const std::string& service_name,
      FileSystemContext* file_system_context);
  SyncStatusCode InitializeChangeTrackerOnFileThread(
      scoped_ptr<LocalFileChangeTracker>* tracker_ptr,
      FileSystemContext* file_system_context,
      std::set<GURL>* origins_with_changes);
  void DidInitializeChangeTrackerOnIOThread(
      scoped_ptr<LocalFileChangeTracker>* tracker_ptr,
      const GURL& source_url,
      const std::string& service_name,
      FileSystemContext* file_system_context,
      std::set<GURL>* origins_with_changes,
      SyncStatusCode status);
  void DidInitialize(
      const GURL& source_url,
      FileSystemContext* file_system_context,
      SyncStatusCode status);

  // Helper routines for GetFileForLocalSync.
  void GetNextURLsForSyncOnFileThread(
      FileSystemContext* file_system_context,
      std::deque<FileSystemURL>* urls);
  void TryPrepareForLocalSync(
      FileSystemContext* file_system_context,
      std::deque<FileSystemURL>* urls,
      const LocalFileSyncInfoCallback& callback);
  void DidTryPrepareForLocalSync(
      FileSystemContext* file_system_context,
      std::deque<FileSystemURL>* remaining_urls,
      const LocalFileSyncInfoCallback& callback,
      SyncStatusCode status,
      const LocalFileSyncInfo& sync_file_info);

  // Callback routine for PrepareForSync and GetFileForLocalSync.
  void DidGetWritingStatusForSync(
      FileSystemContext* file_system_context,
      SyncStatusCode status,
      const FileSystemURL& url,
      const LocalFileSyncInfoCallback& callback);

  // Helper routine for ClearSyncFlagForURL.
  void EnableWritingOnIOThread(const FileSystemURL& url);

  // Callback routine for ApplyRemoteChange.
  void DidApplyRemoteChange(
      const FileSystemURL& url,
      const SyncStatusCallback& callback_on_ui,
      base::PlatformFileError file_error);

  void DidGetFileMetadata(
      const SyncFileMetadataCallback& callback,
      base::PlatformFileError file_error,
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path);

  base::TimeDelta NotifyChangesDuration();

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Indicates if the sync service is shutdown on UI thread.
  bool shutdown_on_ui_;

  // OperationRunner. This must be accessed only on IO thread.
  scoped_ptr<SyncableFileOperationRunner> operation_runner_;

  // Keeps track of writing/syncing status.
  // This must be accessed only on IO thread.
  scoped_ptr<LocalFileSyncStatus> sync_status_;

  // Pointers to file system contexts that have been initialized for
  // synchronization (i.e. that own this instance).
  // This must be accessed only on UI thread.
  std::set<FileSystemContext*> file_system_contexts_;

  // Accessed only on UI thread.
  std::map<FileSystemContext*, StatusCallbackQueue>
      pending_initialize_callbacks_;

  // A URL and associated callback waiting for sync is enabled.
  // Accessed only on IO thread.
  FileSystemURL url_waiting_sync_on_io_;
  base::Closure url_syncable_callback_;

  // Used only on IO thread for available changes notifications.
  base::Time last_notified_changes_;
  scoped_ptr<base::OneShotTimer<LocalFileSyncContext> > timer_on_io_;
  std::set<GURL> origins_with_pending_changes_;

  ObserverList<LocalOriginChangeObserver> origin_change_observers_;

  int mock_notify_changes_duration_in_sec_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileSyncContext);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_SYNC_CONTEXT_H_
