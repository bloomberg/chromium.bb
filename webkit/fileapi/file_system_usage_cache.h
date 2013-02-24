// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_USAGE_CACHE_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_USAGE_CACHE_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {

class WEBKIT_STORAGE_EXPORT_PRIVATE FileSystemUsageCache {
 public:
  // Gets the size described in the .usage file even if dirty > 0 or
  // is_valid == false.  Returns less than zero if the .usage file is not
  // available.
  static int64 GetUsage(const base::FilePath& usage_file_path);

  // Gets the dirty count in the .usage file.
  // Returns less than zero if the .usage file is not available.
  static int32 GetDirty(const base::FilePath& usage_file_path);

  // Increments or decrements the "dirty" entry in the .usage file.
  // Returns false if no .usage is available.
  static bool IncrementDirty(const base::FilePath& usage_file_path);
  static bool DecrementDirty(const base::FilePath& usage_file_path);

  // Notifies quota system that it needs to recalculate the usage cache of the
  // origin.  Returns false if no .usage is available.
  static bool Invalidate(const base::FilePath& usage_file_path);
  static bool IsValid(const base::FilePath& usage_file_path);

  // Updates the size described in the .usage file.
  static int UpdateUsage(const base::FilePath& usage_file_path, int64 fs_usage);

  // Updates the size described in the .usage file by delta with keeping dirty
  // even if dirty > 0.
  static int AtomicUpdateUsageByDelta(
      const base::FilePath& usage_file_path, int64 delta);

  static bool Exists(const base::FilePath& usage_file_path);
  static bool Delete(const base::FilePath& usage_file_path);

  static const base::FilePath::CharType kUsageFileName[];
  static const char kUsageFileHeader[];
  static const int kUsageFileSize;
  static const int kUsageFileHeaderSize;

 private:
  // Read the size, validity and the "dirty" entry described in the .usage file.
  // Returns less than zero if no .usage file is available.
  static int64 Read(const base::FilePath& usage_file_path,
                    bool* is_valid,
                    uint32* dirty);

  static int Write(const base::FilePath& usage_file_path,
                   bool is_valid,
                   uint32 dirty,
                   int64 fs_usage);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_USAGE_CACHE_H_
