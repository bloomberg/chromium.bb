// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_CHANGE_TRACKER_H_
#define WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_CHANGE_TRACKER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "webkit/fileapi/file_observers.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/fileapi_export.h"
#include "webkit/fileapi/syncable/file_change.h"

namespace base {
class SequencedTaskRunner;
}

namespace fileapi {

class LocalFileSyncStatus;

// Tracks local file changes for cloud-backed file systems.
// All methods must be called on the file_task_runner given to the constructor.
// Owned by FileSystemContext.
class FILEAPI_EXPORT LocalFileChangeTracker
    : public FileUpdateObserver,
      public FileChangeObserver {
 public:
  typedef std::map<FileSystemURL, FileChangeList, FileSystemURL::Comparator>
      FileChangeMap;

  // |file_task_runner| must be the one where the observee file operations run.
  // (So that we can make sure DB operations are done before actual update
  // happens)
  LocalFileChangeTracker(LocalFileSyncStatus* sync_status,
                         base::SequencedTaskRunner* file_task_runner);
  virtual ~LocalFileChangeTracker();

  // FileUpdateObserver overrides.
  virtual void OnStartUpdate(const FileSystemURL& url) OVERRIDE;
  virtual void OnUpdate(const FileSystemURL& url, int64 delta) OVERRIDE {}
  virtual void OnEndUpdate(const FileSystemURL& url) OVERRIDE;

  // FileChangeObserver overrides.
  virtual void OnCreateFile(const FileSystemURL& url) OVERRIDE;
  virtual void OnCreateFileFrom(const FileSystemURL& url,
                                const FileSystemURL& src) OVERRIDE;
  virtual void OnRemoveFile(const FileSystemURL& url) OVERRIDE;
  virtual void OnModifyFile(const FileSystemURL& url) OVERRIDE;
  virtual void OnCreateDirectory(const FileSystemURL& url) OVERRIDE;
  virtual void OnRemoveDirectory(const FileSystemURL& url) OVERRIDE;

  // Called by FileSyncService to collect changed files in FileSystem URL.
  void GetChangedURLs(std::vector<FileSystemURL>* urls);

  // Called by FileSyncService to get changes for the given |url|.
  // This should be called after writing is disabled.
  void GetChangesForURL(const FileSystemURL& url, FileChangeList* changes);

  // Called by FileSyncService to notify that the changes are synced for |url|.
  // This removes |url| from the internal change map and re-enables writing
  // for |url|.
  void FinalizeSyncForURL(const FileSystemURL& url);

  // Called by FileSyncService at the startup time to collect last
  // dirty changes left after the last shutdown (if any).
  void CollectLastDirtyChanges(FileChangeMap* changes);

 private:
  void RecordChange(const FileSystemURL& url, const FileChange& change);

  // Database related methods.
  void MarkDirtyOnDatabase(const FileSystemURL& url);
  void ClearDirtyOnDatabase(const FileSystemURL& url);

  // Not owned; this must have the same lifetime with us (both owned
  // by FileSystemContext).
  LocalFileSyncStatus *sync_status_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  FileChangeMap changes_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileChangeTracker);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_CHANGE_TRACKER_H_
