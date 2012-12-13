// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_TASK_RUNNERS_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_TASK_RUNNERS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace

namespace fileapi {

WEBKIT_STORAGE_EXPORT extern const char kMediaTaskRunnerName[];

// This class holds task runners used for filesystem related stuff.
class WEBKIT_STORAGE_EXPORT FileSystemTaskRunners {
 public:
  FileSystemTaskRunners(
      base::SingleThreadTaskRunner* io_task_runner,
      base::SingleThreadTaskRunner* file_task_runner,
      base::SequencedTaskRunner* media_task_runner);

  ~FileSystemTaskRunners();

  static scoped_ptr<FileSystemTaskRunners> CreateMockTaskRunners();

  base::SingleThreadTaskRunner* io_task_runner() {
    return io_task_runner_.get();
  }

  base::SingleThreadTaskRunner* file_task_runner() {
    return file_task_runner_.get();
  }

  base::SequencedTaskRunner* media_task_runner() {
    return media_task_runner_.get();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemTaskRunners);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_TASK_RUNNERS_H_
