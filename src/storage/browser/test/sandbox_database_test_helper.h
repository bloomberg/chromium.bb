// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILEAPI_SANDBOX_DATABASE_TEST_HELPER_H_
#define CONTENT_BROWSER_FILEAPI_SANDBOX_DATABASE_TEST_HELPER_H_

#include <stddef.h>

#include "third_party/leveldatabase/src/db/filename.h"

namespace base {
class FilePath;
}

namespace content {

void CorruptDatabase(const base::FilePath& db_path,
                     leveldb::FileType type,
                     ptrdiff_t offset,
                     size_t size);

void DeleteDatabaseFile(const base::FilePath& db_path,
                        leveldb::FileType type);

}  // namespace content

#endif  // CONTENT_BROWSER_FILEAPI_SANDBOX_DATABASE_TEST_HELPER_H_
