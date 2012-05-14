// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "webkit/fileapi/file_system_path.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/quota/quota_types.h"

namespace quota {
class QuotaManagerProxy;
}

class FilePath;

namespace fileapi {

class FileSystemContext;
class FileSystemFileUtil;
class FileSystemOperation;
class FileSystemOperationContext;

// Filesystem test helper class that encapsulates test environment for
// a given {origin, type} pair.
class FileSystemTestOriginHelper {
 public:
  FileSystemTestOriginHelper(const GURL& origin, FileSystemType type);
  FileSystemTestOriginHelper();
  ~FileSystemTestOriginHelper();

  void SetUp(const FilePath& base_dir, FileSystemFileUtil* file_util);
  // If you want to use more than one FileSystemTestOriginHelper in a single
  // base directory, they have to share a context, so that they don't have
  // multiple databases fighting over the lock to the origin directory [deep
  // down inside ObfuscatedFileUtil].
  void SetUp(FileSystemContext* file_system_context,
             FileSystemFileUtil* file_util);
  void SetUp(const FilePath& base_dir,
             bool unlimited_quota,
             quota::QuotaManagerProxy* quota_manager_proxy,
             FileSystemFileUtil* file_util);
  void TearDown();

  FilePath GetOriginRootPath() const;
  FilePath GetLocalPath(const FilePath& path);
  FilePath GetLocalPathFromASCII(const std::string& path);
  GURL GetURLForPath(const FilePath& path) const;

  // Returns empty path if filesystem type is neither temporary nor persistent.
  FilePath GetUsageCachePath() const;

  // Creates a new FileSystemPath for the given |path|.
  FileSystemPath CreatePath(const FilePath& path) const;
  FileSystemPath CreatePathFromUTF8(const std::string& utf8) const {
    return CreatePath(FilePath::FromUTF8Unsafe(utf8));
  }

  // Helper methods for same-FileUtil copy/move.
  base::PlatformFileError SameFileUtilCopy(
      FileSystemOperationContext* context,
      const FileSystemPath& src,
      const FileSystemPath& dest) const;
  base::PlatformFileError SameFileUtilMove(
      FileSystemOperationContext* context,
      const FileSystemPath& src,
      const FileSystemPath& dest) const;

  // This returns cached usage size returned by QuotaUtil.
  int64 GetCachedOriginUsage() const;

  // This doesn't work with OFSFU.
  int64 ComputeCurrentOriginUsage() const;

  int64 ComputeCurrentDirectoryDatabaseUsage() const;

  FileSystemOperation* NewOperation();
  FileSystemOperationContext* NewOperationContext();

  FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

  const GURL& origin() const { return origin_; }
  FileSystemType type() const { return type_; }
  quota::StorageType storage_type() const {
    return FileSystemTypeToQuotaStorageType(type_);
  }
  FileSystemFileUtil* file_util() const { return file_util_; }

 private:
  scoped_refptr<FileSystemContext> file_system_context_;
  const GURL origin_;
  const FileSystemType type_;
  FileSystemFileUtil* file_util_;
  int64 initial_usage_size_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_TEST_HELPER_H_
