// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_COPY_OR_MOVE_FILE_VALIDATOR_H_
#define STORAGE_BROWSER_FILEAPI_COPY_OR_MOVE_FILE_VALIDATOR_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "storage/browser/storage_browser_export.h"

namespace base {
class FilePath;
}

namespace storage {

class FileSystemURL;

class STORAGE_EXPORT CopyOrMoveFileValidator {
 public:
  // Callback that is invoked when validation completes. A result of
  // base::File::FILE_OK means the file validated.
  using ResultCallback = base::Callback<void(base::File::Error result)>;

  virtual ~CopyOrMoveFileValidator() {}

  // Called on a source file before copying or moving to the final
  // destination.
  virtual void StartPreWriteValidation(
      const ResultCallback& result_callback) = 0;

  // Called on a destination file after copying or moving to the final
  // destination. Suitable for running Anti-Virus checks.
  virtual void StartPostWriteValidation(
      const base::FilePath& dest_platform_path,
      const ResultCallback& result_callback) = 0;
};

class CopyOrMoveFileValidatorFactory {
 public:
  virtual ~CopyOrMoveFileValidatorFactory() {}

  // This method must always return a non-null validator. |src_url| is needed
  // in addition to |platform_path| because in the obfuscated file system
  // case, |platform_path| will be an obfuscated filename and extension.
  virtual CopyOrMoveFileValidator* CreateCopyOrMoveFileValidator(
      const FileSystemURL& src_url,
      const base::FilePath& platform_path) = 0;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_COPY_OR_MOVE_FILE_VALIDATOR_H_
