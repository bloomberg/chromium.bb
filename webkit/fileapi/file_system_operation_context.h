// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_

#include "webkit/fileapi/media/mtp_device_file_system_config.h"
#include "webkit/fileapi/task_runner_bound_observer_list.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace fileapi {

class FileSystemContext;
class MediaPathFilter;

// A context class which is carried around by FileSystemOperation and
// its delegated tasks. It is valid to reuse one context instance across
// multiple operations as far as those operations are supposed to share
// the same context (e.g. use the same task runner, share the quota etc).
// Note that the remaining quota bytes (allowed_bytes_growth) may be
// updated during the execution of write operations.
class WEBKIT_STORAGE_EXPORT_PRIVATE FileSystemOperationContext {
 public:
  explicit FileSystemOperationContext(FileSystemContext* context);
  ~FileSystemOperationContext();

  FileSystemContext* file_system_context() const {
    return file_system_context_;
  }

  // Updates the current remaining quota.
  void set_allowed_bytes_growth(const int64& allowed_bytes_growth) {
    allowed_bytes_growth_ = allowed_bytes_growth;
  }

  // Returns the current remaining quota.
  int64 allowed_bytes_growth() const { return allowed_bytes_growth_; }

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  // Reads |mtp_device_delegate_url_| on |task_runner_|.
  const std::string& mtp_device_delegate_url() const {
    return mtp_device_delegate_url_;
  }
#endif

  // Returns TaskRunner which the operation is performed on.
  base::SequencedTaskRunner* task_runner() const {
    return task_runner_.get();
  }

  MediaPathFilter* media_path_filter() { return media_path_filter_; }
  ChangeObserverList* change_observers() { return &change_observers_; }
  AccessObserverList* access_observers() { return &access_observers_; }
  UpdateObserverList* update_observers() { return &update_observers_; }

 private:
  // Only MountPointProviders can access these setters.
  friend class CrosMountPointProvider;
  friend class IsolatedMountPointProvider;
  friend class SandboxMountPointProvider;
  friend class TestMountPointProvider;

  // Tests also need access to some setters.
  friend class FileSystemQuotaClientTest;
  friend class LocalFileSystemOperationTest;
  friend class LocalFileSystemOperationWriteTest;
  friend class LocalFileSystemTestOriginHelper;
  friend class ObfuscatedFileUtilTest;

  // Overrides TaskRunner which the operation is performed on.
  // file_system_context_->task_runners()->file_task_runner() is used otherwise.
  void set_task_runner(base::SequencedTaskRunner* task_runner);

  void set_media_path_filter(MediaPathFilter* media_path_filter) {
    media_path_filter_ = media_path_filter;
  }
  void set_change_observers(const ChangeObserverList& list) {
    change_observers_ = list;
  }
  void set_access_observers(const AccessObserverList& list) {
    access_observers_ = list;
  }
  void set_update_observers(const UpdateObserverList& list) {
    update_observers_ = list;
  }
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  // Initializes |mtp_device_delegate_url_| on the IO thread.
  void set_mtp_device_delegate_url(const std::string& delegate_url) {
    mtp_device_delegate_url_ = delegate_url;
  }
#endif

  FileSystemContext* file_system_context_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  int64 allowed_bytes_growth_;
  MediaPathFilter* media_path_filter_;

  AccessObserverList access_observers_;
  ChangeObserverList change_observers_;
  UpdateObserverList update_observers_;

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  // URL for the media transfer protocol (MTP) device delegate.
  // Initialized on IO thread and used on |task_runner_|.
  std::string mtp_device_delegate_url_;
#endif

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationContext);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_
