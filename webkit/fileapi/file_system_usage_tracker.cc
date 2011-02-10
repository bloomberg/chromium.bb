// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_usage_tracker.h"

#include <algorithm>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_path_manager.h"

namespace fileapi {

class FileSystemUsageTracker::GetUsageTask
    : public base::RefCountedThreadSafe<GetUsageTask> {
 public:
  GetUsageTask(
      FileSystemUsageTracker* tracker,
      scoped_refptr<base::MessageLoopProxy> file_message_loop,
      std::string fs_name,
      const FilePath& origin_base_path)
      : tracker_(tracker),
        file_message_loop_(file_message_loop),
        original_message_loop_(
            base::MessageLoopProxy::CreateForCurrentThread()),
        fs_name_(fs_name),
        fs_usage_(0),
        origin_base_path_(origin_base_path) {
  }

  virtual ~GetUsageTask() {}

  void Start() {
    DCHECK(tracker_);
    tracker_->RegisterUsageTask(this);
    file_message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &GetUsageTask::RunOnFileThread));
  }

  void Cancel() {
    DCHECK(original_message_loop_->BelongsToCurrentThread());
    tracker_ = NULL;
  }

 private:
  void RunOnFileThread() {
    DCHECK(file_message_loop_->BelongsToCurrentThread());

    // TODO(dmikurube): add the code that retrieves the origin usage here.

    original_message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &GetUsageTask::Completed));
  }

  void Completed() {
    DCHECK(original_message_loop_->BelongsToCurrentThread());
    if (tracker_) {
      tracker_->UnregisterUsageTask(this);
      tracker_->DidGetOriginUsage(fs_name_, fs_usage_);
    }
  }

  FileSystemUsageTracker* tracker_;
  scoped_refptr<base::MessageLoopProxy> file_message_loop_;
  scoped_refptr<base::MessageLoopProxy> original_message_loop_;
  std::string fs_name_;
  int64 fs_usage_;
  FilePath origin_base_path_;
};

FileSystemUsageTracker::FileSystemUsageTracker(
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    const FilePath& profile_path,
    bool is_incognito)
    : file_message_loop_(file_message_loop),
      base_path_(profile_path.Append(
          FileSystemPathManager::kFileSystemDirectory)),
      is_incognito_(is_incognito) {
  DCHECK(file_message_loop);
}

FileSystemUsageTracker::~FileSystemUsageTracker() {
  std::for_each(running_usage_tasks_.begin(),
                running_usage_tasks_.end(),
                std::mem_fun(&GetUsageTask::Cancel));
}

void FileSystemUsageTracker::GetOriginUsage(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    GetUsageCallback* callback_ptr) {
  DCHECK(callback_ptr);
  scoped_ptr<GetUsageCallback> callback(callback_ptr);

  if (is_incognito_) {
    // We don't support FileSystem in incognito mode yet.
    callback->Run(0);
    return;
  }

  std::string fs_name = FileSystemPathManager::GetFileSystemName(
      origin_url, type);
  if (pending_usage_callbacks_.find(fs_name) !=
      pending_usage_callbacks_.end()) {
    // Another get usage task is running.  Add the callback to
    // the pending queue and return.
    pending_usage_callbacks_[fs_name].push_back(callback.release());
    return;
  }

  // Get the filesystem base path (i.e. "FileSystem/<origin>/<type>",
  // without unique part).
  FilePath origin_base_path =
      FileSystemPathManager::GetFileSystemBaseDirectoryForOriginAndType(
          base_path_, origin_url, type);
  if (origin_base_path.empty()) {
    // The directory does not exist.
    callback->Run(0);
    return;
  }

  pending_usage_callbacks_[fs_name].push_back(callback.release());
  scoped_refptr<GetUsageTask> task(
      new GetUsageTask(this, file_message_loop_, fs_name, origin_base_path));
  task->Start();
}

void FileSystemUsageTracker::RegisterUsageTask(GetUsageTask* task) {
  running_usage_tasks_.push_back(task);
}

void FileSystemUsageTracker::UnregisterUsageTask(GetUsageTask* task) {
  DCHECK(running_usage_tasks_.front() == task);
  running_usage_tasks_.pop_front();
}

void FileSystemUsageTracker::DidGetOriginUsage(
    const std::string& fs_name, int64 usage) {
  PendingUsageCallbackMap::iterator cb_list_iter =
      pending_usage_callbacks_.find(fs_name);
  DCHECK(cb_list_iter != pending_usage_callbacks_.end());
  PendingCallbackList cb_list = cb_list_iter->second;
  for (PendingCallbackList::iterator cb_iter = cb_list.begin();
        cb_iter != cb_list.end();
        ++cb_iter) {
    scoped_ptr<GetUsageCallback> callback(*cb_iter);
    callback->Run(usage);
  }
  pending_usage_callbacks_.erase(cb_list_iter);
}

}  // namespace fileapi
