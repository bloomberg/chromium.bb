// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_USAGE_CACHE_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_USAGE_CACHE_H_

#include "base/basictypes.h"
#include "base/file_path.h"

namespace fileapi {

class FileSystemUsageCache {
 public:
  // Gets the size described in the .usage file.
  // Returns less than zero if the size is not availble.
  static int64 GetUsage(const FilePath& usage_file_path);

  // Increments or decrements the "dirty" entry in the .usage file.
  // Returns false if no .usage is available.
  static bool IncrementDirty(const FilePath& usage_file_path);
  static bool DecrementDirty(const FilePath& usage_file_path);

  // Updates the size described in the .usage file.
  static int UpdateUsage(const FilePath& usage_file_path, int64 fs_usage);

  static bool Exists(const FilePath& usage_file_path);
  static bool Delete(const FilePath& usage_file_path);

  static const char kUsageFileName[];
  static const char kUsageFileHeader[];
  static const int kUsageFileSize;
  static const int kUsageFileHeaderSize;

 private:
  // Read the size and the "dirty" entry described in the .usage file.
  // Returns less than zero if no .usage file is available.
  static int64 Read(const FilePath& usage_file_path, uint32* dirty);

  static int Write(const FilePath& usage_file_path,
                   uint32 dirty, int64 fs_usage);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_USAGE_CACHE_H_
