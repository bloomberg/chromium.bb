// Copyright (c) 2013 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef THIRD_PARTY_LEVELDATABASE_ENV_CHROMIUM_H_
#define THIRD_PARTY_LEVELDATABASE_ENV_CHROMIUM_H_

#include "base/platform_file.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb_env {

enum MethodID {
  kSequentialFileRead,
  kSequentialFileSkip,
  kRandomAccessFileRead,
  kWritableFileAppend,
  kWritableFileClose,
  kWritableFileFlush,
  kWritableFileSync,
  kNewSequentialFile,
  kNewRandomAccessFile,
  kNewWritableFile,
  kDeleteFile,
  kCreateDir,
  kDeleteDir,
  kGetFileSize,
  kRenameFile,
  kLockFile,
  kUnlockFile,
  kGetTestDirectory,
  kNewLogger,
  kNumEntries
};

leveldb::Status MakeIOError(leveldb::Slice filename,
                            const char* message,
                            MethodID method,
                            int saved_errno);
leveldb::Status MakeIOError(leveldb::Slice filename,
                            const char* message,
                            MethodID method,
                            base::PlatformFileError error);
leveldb::Status MakeIOError(leveldb::Slice filename,
                            const char* message,
                            MethodID method);

bool ParseMethodAndError(const char* string, int* method, int* error);

}  // namespace leveldb_env

#endif
