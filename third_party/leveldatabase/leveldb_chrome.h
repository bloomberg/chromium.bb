// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef THIRD_PARTY_LEVELDATABASE_LEVELDB_CHROME_H_
#define THIRD_PARTY_LEVELDATABASE_LEVELDB_CHROME_H_

#include "leveldb/cache.h"
#include "leveldb/env.h"
#include "leveldb/export.h"
#include "third_party/leveldatabase/src/db/filename.h"

namespace leveldb_chrome {

// Return the shared leveldb block cache for web APIs. The caller *does not*
// own the returned instance.
LEVELDB_EXPORT leveldb::Cache* GetSharedWebBlockCache();

// Return the shared leveldb block cache for browser (non web) APIs. The caller
// *does not* own the returned instance.
LEVELDB_EXPORT leveldb::Cache* GetSharedBrowserBlockCache();

// Return the shared leveldb block cache for in-memory Envs. The caller *does
// not* own the returned instance.
LEVELDB_EXPORT leveldb::Cache* GetSharedInMemoryBlockCache();

// Determine if a leveldb::Env stores the file data in RAM.
LEVELDB_EXPORT bool IsMemEnv(const leveldb::Env* env);

// Creates an in-memory Env for which all files are stored in the heap.
LEVELDB_EXPORT leveldb::Env* NewMemEnv(leveldb::Env* base_env);

// If filename is a leveldb file, store the type of the file in *type.
// The number encoded in the filename is stored in *number.  If the
// Returns true if the filename was successfully parsed.
LEVELDB_EXPORT bool ParseFileName(const std::string& filename,
                                  uint64_t* number,
                                  leveldb::FileType* type);

// Report leveldb UMA values.
LEVELDB_EXPORT void UpdateHistograms();

}  // namespace leveldb_chrome

#endif  // THIRD_PARTY_LEVELDATABASE_LEVELDB_CHROME_H_
