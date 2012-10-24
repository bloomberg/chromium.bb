// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_SYNC_CONTEXT_H_
#define WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_SYNC_CONTEXT_H_

#include <deque>
#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/local_file_sync_status.h"
#include "webkit/fileapi/syncable/sync_status_code.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace fileapi {

class FileChange;
class FileSystemContext;
class LocalFileChangeTracker;
class SyncableFileOperationRunner;

// This class works as a bridge between LocalFileSyncService (which is a
// per-profile object) and FileSystemContext's (which is a per-storage-partition
// object and may exist multiple in a profile).
// An instance of this class is shared by FileSystemContexts and outlives
// LocalFileSyncService.
class WEBKIT_STORAGE_EXPORT LocalFileSyncContext
    : public base::RefCountedThreadSafe<LocalFileSyncContext> {
 public:
  typedef base::Callback<void(SyncStatusCode status)> StatusCallback;

  LocalFileSyncContext(base::SingleThreadTaskRunner* ui_task_runner,
                       base::SingleThreadTaskRunner* io_task_runner);

  // Initializes |file_system_context| for syncable file operations and
  // registers the it into the internal map.
  // Calling this multiple times for the same file_system_context is valid.
  // This method must be called on UI thread.
  void MaybeInitializeFileSystemContext(const GURL& source_url,
                                        FileSystemContext* file_system_context,
                                        const StatusCallback& callback);

  // Called when the corresponding LocalFileSyncService exits.
  // This method must be called on UI thread.
  void ShutdownOnUIThread();

  // OperationRunner is accessible only on IO thread.
  base::WeakPtr<SyncableFileOperationRunner> operation_runner() const;

  // SyncContext is accessible only on IO thread.
  LocalFileSyncStatus* sync_status() const;

 private:
  typedef std::deque<StatusCallback> StatusCallbackQueue;
  friend class base::RefCountedThreadSafe<LocalFileSyncContext>;
  friend class CannedSyncableFileSystem;

  virtual ~LocalFileSyncContext();

  void ShutdownOnIOThread();

  // Helper routines for MaybeInitializeFileSystemContext.
  void InitializeFileSystemContextOnIOThread(
      const GURL& source_url,
      FileSystemContext* file_system_context);
  SyncStatusCode InitializeChangeTrackerOnFileThread(
      scoped_ptr<LocalFileChangeTracker>* tracker_ptr,
      FileSystemContext* file_system_context);
  void DidInitializeChangeTracker(
      scoped_ptr<LocalFileChangeTracker>* tracker_ptr,
      const GURL& source_url,
      FileSystemContext* file_system_context,
      SyncStatusCode status);
  void DidInitialize(
      const GURL& source_url,
      FileSystemContext* file_system_context,
      SyncStatusCode status);

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

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

  // Origin to context map. (Assuming that as far as we're in the same
  // profile single origin wouldn't belong to multiple FileSystemContexts.)
  // Accessed only on UI thread.
  std::map<GURL, FileSystemContext*> origin_to_contexts_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileSyncContext);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_SYNC_CONTEXT_H_
