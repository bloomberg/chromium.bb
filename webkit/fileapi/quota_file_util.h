// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

  virtual base::PlatformFileError Truncate(
      FileSystemOperationContext* fs_context,
      const FilePath& path,
      int64 length) OVERRIDE;
  virtual base::PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* fs_context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      bool copy) OVERRIDE;
  virtual base::PlatformFileError DeleteFile(
      FileSystemOperationContext* fs_context,
      const FilePath& file_path) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuotaFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_QUOTA_FILE_UTIL_H_
