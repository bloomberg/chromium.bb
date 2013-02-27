// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_CHANGE_TRACKER_H_
#define WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_CHANGE_TRACKER_H_

#include <deque>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "webkit/fileapi/file_observers.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/sync_status_code.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace fileapi {
class FileSystemContext;
class FileSystemURL;
}

namespace sync_file_system {

// Tracks local file changes for cloud-backed file systems.
// All methods must be called on the file_task_runner given to the constructor.
// Owned by FileSystemContext.
class WEBKIT_STORAGE_EXPORT LocalFileChangeTracker
    : public fileapi::FileUpdateObserver,
      public fileapi::FileChangeObserver {
 public:
  // |file_task_runner| must be the one where the observee file operations run.
  // (So that we can make sure DB operations are done before actual update
  // happens)
  LocalFileChangeTracker(const base::FilePath& base_path,
                         base::SequencedTaskRunner* file_task_runner);
  virtual ~LocalFileChangeTracker();

  // FileUpdateObserver overrides.
  virtual void OnStartUpdate(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void OnUpdate(
      const fileapi::FileSystemURL& url, int64 delta) OVERRIDE {}
  virtual void OnEndUpdate(const fileapi::FileSystemURL& url) OVERRIDE;

  // FileChangeObserver overrides.
  virtual void OnCreateFile(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void OnCreateFileFrom(const fileapi::FileSystemURL& url,
                                const fileapi::FileSystemURL& src) OVERRIDE;
  virtual void OnRemoveFile(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void OnModifyFile(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void OnCreateDirectory(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void OnRemoveDirectory(const fileapi::FileSystemURL& url) OVERRIDE;

  // Retrieves an array of |url| which have more than one pending changes.
  // If |max_urls| is non-zero (recommended in production code) this
  // returns URLs up to the number from the ones that have smallest
  // change_seq numbers (i.e. older changes).
  void GetNextChangedURLs(std::deque<fileapi::FileSystemURL>* urls,
                          int max_urls);

  // Returns all changes recorded for the given |url|.
  // This should be called after writing is disabled.
  void GetChangesForURL(const fileapi::FileSystemURL& url,
                        FileChangeList* changes);

  // Clears the pending changes recorded in this tracker for |url|.
  void ClearChangesForURL(const fileapi::FileSystemURL& url);

  // Called by FileSyncService at the startup time to restore last dirty changes
  // left after the last shutdown (if any).
  SyncStatusCode Initialize(fileapi::FileSystemContext* file_system_context);

  // This method is (exceptionally) thread-safe.
  int64 num_changes() const {
    base::AutoLock lock(num_changes_lock_);
    return num_changes_;
  }

  void UpdateNumChanges();

 private:
  class TrackerDB;
  friend class CannedSyncableFileSystem;
  friend class LocalFileChangeTrackerTest;
  friend class LocalFileSyncContext;
  friend class SyncableFileSystemTest;

  struct ChangeInfo {
    ChangeInfo();
    ~ChangeInfo();
    FileChangeList change_list;
    int64 change_seq;
  };

  typedef std::map<fileapi::FileSystemURL, ChangeInfo,
      fileapi::FileSystemURL::Comparator>
          FileChangeMap;
  typedef std::map<int64, fileapi::FileSystemURL> ChangeSeqMap;

  // This does mostly same as calling GetNextChangedURLs with max_url=0
  // except that it returns urls in set rather than in deque.
  // Used only in testings.
  void GetAllChangedURLs(fileapi::FileSystemURLSet* urls);

  // Used only in testings.
  void DropAllChanges();

  // Database related methods.
  SyncStatusCode MarkDirtyOnDatabase(const fileapi::FileSystemURL& url);
  SyncStatusCode ClearDirtyOnDatabase(const fileapi::FileSystemURL& url);

  SyncStatusCode CollectLastDirtyChanges(
      fileapi::FileSystemContext* file_system_context);
  void RecordChange(const fileapi::FileSystemURL& url,
                    const FileChange& change);

  bool initialized_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  FileChangeMap changes_;
  ChangeSeqMap change_seqs_;

  scoped_ptr<TrackerDB> tracker_db_;

  // Change sequence number. Briefly gives a hint about the order of changes,
  // but they are updated when a new change comes on the same file (as
  // well as Drive's changestamps).
  int64 current_change_seq_;

  // This can be accessed on any threads (with num_changes_lock_).
  int64 num_changes_;
  mutable base::Lock num_changes_lock_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileChangeTracker);
};

}  // namespace sync_file_system

#endif  // WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_CHANGE_TRACKER_H_
