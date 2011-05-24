// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_TEST_HELPER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_TEST_HELPER_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_types.h"

namespace quota {
class QuotaManagerProxy;
}

class FilePath;

namespace fileapi {

class FileSystemCallbackDispatcher;
class FileSystemContext;
class FileSystemFileUtil;
class FileSystemOperation;
class FileSystemOperationContext;
class FileSystemPathManager;

// Filesystem test helper class that encapsulates test environment for
// a given {origin, type} pair.
class FileSystemTestOriginHelper {
 public:
  FileSystemTestOriginHelper(const GURL& origin, FileSystemType type);
  FileSystemTestOriginHelper();
  ~FileSystemTestOriginHelper();

  void SetUp(const FilePath& base_dir, FileSystemFileUtil* file_util);
  void SetUp(const FilePath& base_dir,
             bool incognito_mode,
             bool unlimited_quota,
             quota::QuotaManagerProxy* quota_manager_proxy,
             FileSystemFileUtil* file_util);
  void TearDown();

  FilePath GetOriginRootPath() const;
  FilePath GetLocalPath(const FilePath& path);
  FilePath GetLocalPathFromASCII(const std::string& path);
  GURL GetURLForPath(const FilePath& path) const;
  FilePath GetUsageCachePath() const;

  FileSystemOperation* NewOperation(
      FileSystemCallbackDispatcher* callback_dispatcher);
  FileSystemOperationContext* NewOperationContext();

  FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

  const GURL& origin() const { return origin_; }
  FileSystemType type() const { return type_; }
  FileSystemFileUtil* file_util() { return file_util_; }

 private:
  void InitializeOperationContext(FileSystemOperationContext* context);

  scoped_refptr<FileSystemContext> file_system_context_;
  const GURL origin_;
  const FileSystemType type_;
  FileSystemFileUtil* file_util_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_TEST_HELPER_H_
