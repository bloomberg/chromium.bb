// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CLIENT_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CLIENT_H_

#include <vector>

#include "base/file_util_proxy.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileError.h"

namespace fileapi {

class FileSystemOperation;

// Interface for client of FileSystemOperation.
class FileSystemOperationClient {
 public:
  virtual ~FileSystemOperationClient() {}

  virtual void DidFail(
      WebKit::WebFileError status,
      FileSystemOperation* operation) = 0;

  virtual void DidSucceed(FileSystemOperation* operation) = 0;

  // Info about the file entry such as modification date and size.
  virtual void DidReadMetadata(const base::PlatformFileInfo& info,
                               FileSystemOperation* operation) = 0;

  virtual void DidReadDirectory(
      const std::vector<base::file_util_proxy::Entry>& entries,
      bool has_more, FileSystemOperation* operation) = 0;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CLIENT_H_
