// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/fileapi_export.h"
#include "webkit/fileapi/media/media_file_system_config.h"
#include "webkit/fileapi/task_runner_bound_observer_list.h"

#if defined(SUPPORT_MEDIA_FILESYSTEM)
#include "webkit/fileapi/media/media_device_delegate.h"
#endif

namespace fileapi {

class MediaPathFilter;

class FILEAPI_EXPORT_PRIVATE FileSystemOperationContext {
 public:
  explicit FileSystemOperationContext(FileSystemContext* context);
  ~FileSystemOperationContext();

  FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

  void set_allowed_bytes_growth(const int64& allowed_bytes_growth) {
    allowed_bytes_growth_ = allowed_bytes_growth;
  }
  int64 allowed_bytes_growth() const { return allowed_bytes_growth_; }

#if defined(SUPPORT_MEDIA_FILESYSTEM)
  void set_media_device_delegate(MediaDeviceDelegate* delegate) {
    media_device_delegate_ = delegate;
  }

  MediaDeviceDelegate* media_device_delegate() const {
    return media_device_delegate_.get();
  }
#endif

  // Returns TaskRunner which the operation is performed on.
  base::SequencedTaskRunner* task_runner() const {
    return task_runner_.get();
  }

  // Overrides TaskRunner which the operation is performed on.
  // file_system_context_->task_runners()->file_task_runner() is used otherwise.
  void set_task_runner(base::SequencedTaskRunner* task_runner);

  void set_media_path_filter(MediaPathFilter* media_path_filter) {
    media_path_filter_ = media_path_filter;
  }

  MediaPathFilter* media_path_filter() {
    return media_path_filter_;
  }

  void set_change_observers(const ChangeObserverList& list) {
    change_observers_ = list;
  }
  ChangeObserverList* change_observers() { return &change_observers_; }

  void set_access_observers(const AccessObserverList& list) {
    access_observers_ = list;
  }
  AccessObserverList* access_observers() { return &access_observers_; }

  void set_update_observers(const UpdateObserverList& list) {
    update_observers_ = list;
  }
  UpdateObserverList* update_observers() { return &update_observers_; }

 private:
  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  int64 allowed_bytes_growth_;
  MediaPathFilter* media_path_filter_;

  AccessObserverList access_observers_;
  ChangeObserverList change_observers_;
  UpdateObserverList update_observers_;

#if defined(SUPPORT_MEDIA_FILESYSTEM)
  // Store the current media device.
  scoped_refptr<MediaDeviceDelegate> media_device_delegate_;
#endif
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_
