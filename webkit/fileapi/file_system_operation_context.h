// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_

#include "webkit/fileapi/file_system_file_util.h"

namespace fileapi {

class FileSystemOperationContext {
 public:
  FileSystemOperationContext(FileSystemFileUtil* file_system_file_util)
      : file_system_file_util_(file_system_file_util) {
  }

  FileSystemFileUtil* file_system_file_util() const {
    return file_system_file_util_;
  }

 private:
  // This file_system_file_util_ is not "owned" by FileSystemOperationContext.
  // It is supposed to be a pointer to a singleton.
  FileSystemFileUtil* file_system_file_util_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_
