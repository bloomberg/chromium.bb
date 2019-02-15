// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_OBFUSCATED_FILE_UTIL_MEMORY_DELEGATE_H_
#define STORAGE_BROWSER_FILEAPI_OBFUSCATED_FILE_UTIL_MEMORY_DELEGATE_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/fileapi/native_file_util.h"
#include "storage/browser/fileapi/obfuscated_file_util_delegate.h"

namespace storage {

// This delegate performs all ObfuscatedFileUtil tasks that require touching
// disk and peforms them in memory.

class COMPONENT_EXPORT(STORAGE_BROWSER) ObfuscatedFileUtilMemoryDelegate
    : public ObfuscatedFileUtilDelegate {
 public:
  ObfuscatedFileUtilMemoryDelegate();
  ~ObfuscatedFileUtilMemoryDelegate() override;

  bool DirectoryExists(const base::FilePath& path) override;
  bool DeleteFileOrDirectory(const base::FilePath& path,
                             bool recursive) override;
  bool IsLink(const base::FilePath& file_path) override;
  bool PathExists(const base::FilePath& path) override;

  NativeFileUtil::CopyOrMoveMode CopyOrMoveModeForDestination(
      const FileSystemURL& dest_url,
      bool copy) override;
  base::File CreateOrOpen(const base::FilePath& path, int file_flags) override;
  base::File::Error EnsureFileExists(const base::FilePath& path,
                                     bool* created) override;
  base::File::Error CreateDirectory(const base::FilePath& path,
                                    bool exclusive,
                                    bool recursive) override;
  base::File::Error GetFileInfo(const base::FilePath& path,
                                base::File::Info* file_info) override;
  base::File::Error Touch(const base::FilePath& path,
                          const base::Time& last_access_time,
                          const base::Time& last_modified_time) override;
  base::File::Error Truncate(const base::FilePath& path,
                             int64_t length) override;
  base::File::Error CopyOrMoveFile(
      const base::FilePath& src_path,
      const base::FilePath& dest_path,
      FileSystemOperation::CopyOrMoveOption option,
      NativeFileUtil::CopyOrMoveMode mode) override;
  base::File::Error DeleteFile(const base::FilePath& path) override;

  base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<ObfuscatedFileUtilMemoryDelegate> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(ObfuscatedFileUtilMemoryDelegate);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_OBFUSCATED_FILE_UTIL_MEMORY_DELEGATE_H_
