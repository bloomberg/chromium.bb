// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_SYNCABLE_FILE_OPERATION_RUNNER_H_
#define WEBKIT_FILEAPI_SYNCABLE_SYNCABLE_FILE_OPERATION_RUNNER_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/local_file_sync_status.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {
class FileSystemURL;
}

namespace sync_file_system {

// This class must run only on IO thread.
// Owned by LocalFileSyncContext.
class WEBKIT_STORAGE_EXPORT SyncableFileOperationRunner
    : public base::NonThreadSafe,
      public base::SupportsWeakPtr<SyncableFileOperationRunner>,
      public LocalFileSyncStatus::Observer {
 public:
  // Represents an operation task (which usually wraps one FileSystemOperation).
  class Task {
   public:
    Task() {}
    virtual ~Task() {}

    // Only one of Run() or Cancel() is called.
    virtual void Run() = 0;
    virtual void Cancel() = 0;

   protected:
    // This is never called after Run() or Cancel() is called.
    virtual const std::vector<fileapi::FileSystemURL>& target_paths() const = 0;

   private:
    friend class SyncableFileOperationRunner;
    bool IsRunnable(LocalFileSyncStatus* status) const;
    void Start(LocalFileSyncStatus* status);
    static void CancelAndDelete(Task* task);

    DISALLOW_COPY_AND_ASSIGN(Task);
  };

  SyncableFileOperationRunner(int64 max_inflight_tasks,
                              LocalFileSyncStatus* sync_status);
  virtual ~SyncableFileOperationRunner();

  // LocalFileSyncStatus::Observer overrides.
  virtual void OnSyncEnabled(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void OnWriteEnabled(const fileapi::FileSystemURL& url) OVERRIDE;

  // Runs the given |task| if no sync operation is running on any of
  // its target_paths(). This also runs pending tasks that have become
  // runnable (before running the given operation).
  // If there're ongoing sync tasks on the target_paths this method
  // just queues up the |task|.
  // Pending tasks are cancelled when this class is destructed.
  void PostOperationTask(scoped_ptr<Task> task);

  // Runs a next runnable task (if there's any).
  void RunNextRunnableTask();

  // Called when an operation is completed. This will make |target_paths|
  // writable and may start a next runnable task.
  void OnOperationCompleted(
      const std::vector<fileapi::FileSystemURL>& target_paths);

  LocalFileSyncStatus* sync_status() const { return sync_status_; }

  int64 num_pending_tasks() const {
    return static_cast<int64>(pending_tasks_.size());
  }

  int64 num_inflight_tasks() const { return num_inflight_tasks_; }

 private:
  // Returns true if we should start more tasks.
  bool ShouldStartMoreTasks() const;

  // Keeps track of the writing/syncing status. Not owned.
  LocalFileSyncStatus* sync_status_;

  std::list<Task*> pending_tasks_;

  const int64 max_inflight_tasks_;
  int64 num_inflight_tasks_;

  DISALLOW_COPY_AND_ASSIGN(SyncableFileOperationRunner);
};

}  // namespace sync_file_system

#endif  // WEBKIT_FILEAPI_SYNCABLE_SYNCABLE_FILE_OPERATION_RUNNER_H_
