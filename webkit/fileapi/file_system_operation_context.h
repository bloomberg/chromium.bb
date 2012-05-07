// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_types.h"

namespace base {
class SequencedTaskRunner;
}

namespace fileapi {

class FileSystemContext;

class FileSystemOperationContext {
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

  base::SequencedTaskRunner* file_task_runner() const;

 private:
  scoped_refptr<FileSystemContext> file_system_context_;

  int64 allowed_bytes_growth_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_
