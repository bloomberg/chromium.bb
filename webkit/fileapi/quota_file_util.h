// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_QUOTA_FILE_UTIL_H_
#define WEBKIT_FILEAPI_QUOTA_FILE_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#pragma once

namespace fileapi {

class QuotaFileUtil : public FileSystemFileUtil {
 public:
  static const int64 kNoLimit;

  // |underlying_file_util| is owned by the instance.  It will be deleted by
  // the owner instance.  For example, it can be instanciated as follows:
  // FileSystemFileUtil* file_util = new QuotaFileUtil(new SomeFileUtil());
  //
  // To instanciate an object whose underlying file_util is FileSystemFileUtil,
  // QuotaFileUtil::CreateDefault() can be used.
  explicit QuotaFileUtil(FileSystemFileUtil* underlying_file_util);
  virtual ~QuotaFileUtil();

  // Creates a QuotaFileUtil instance with an underlying NativeFileUtil
  // instance.
  static QuotaFileUtil* CreateDefault();

  virtual base::PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* fs_context,
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path,
      bool copy) OVERRIDE;
  virtual base::PlatformFileError DeleteFile(
      FileSystemOperationContext* fs_context,
      const FileSystemPath& path) OVERRIDE;

  // TODO(tzik): Remove this after we complete merging QuotaFileUtil
  // into ObfuscatedFileUtil.  TruncateInternal should be used in test and
  // other methods of QuotaFileUtil.
  base::PlatformFileError TruncateInternal(
      FileSystemOperationContext* fs_context,
      const FileSystemPath& path,
      int64 length);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuotaFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_QUOTA_FILE_UTIL_H_
