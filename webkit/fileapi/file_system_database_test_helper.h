// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_DATABASE_TEST_HELPER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_DATABASE_TEST_HELPER_H_

#include <cstddef>

#include "third_party/leveldatabase/src/db/filename.h"

namespace base {
class FilePath;
}

namespace fileapi {

void CorruptDatabase(const base::FilePath& db_path,
                     leveldb::FileType type,
                     ptrdiff_t offset,
                     size_t size);

void DeleteDatabaseFile(const base::FilePath& db_path,
                        leveldb::FileType type);

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_DATABASE_TEST_HELPER_H_
