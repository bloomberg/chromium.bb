// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_COPY_OR_MOVE_FILE_VALIDATOR_H_
#define WEBKIT_FILEAPI_COPY_OR_MOVE_FILE_VALIDATOR_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/platform_file.h"

namespace fileapi {

class CopyOrMoveFileValidator {
 public:
  // Callback that is invoked when validation completes. A result of
  // base::PLATFORM_FILE_OK means the file validated.
  typedef base::Callback<void(base::PlatformFileError result)> ResultCallback;

  virtual ~CopyOrMoveFileValidator() {}

  virtual void StartValidation(const ResultCallback& result_callback) = 0;
};

class CopyOrMoveFileValidatorFactory {
 public:
  virtual ~CopyOrMoveFileValidatorFactory() {}

  // This method must always return a non-NULL validator.
  virtual CopyOrMoveFileValidator* CreateCopyOrMoveFileValidator(
      const base::FilePath& platform_path) = 0;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_COPY_OR_MOVE_FILE_VALIDATOR_H_
