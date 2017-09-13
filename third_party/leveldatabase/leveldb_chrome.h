// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef THIRD_PARTY_LEVELDATABASE_LEVELDB_CHROME_H_
#define THIRD_PARTY_LEVELDATABASE_LEVELDB_CHROME_H_

#include "leveldb/cache.h"

namespace leveldb_chrome {

// Return the shared leveldb block cache for web APIs. The caller *does not*
// own the returned instance.
extern leveldb::Cache* GetSharedWebBlockCache();

// Return the shared leveldb block cache for browser (non web) APIs. The caller
// *does not* own the returned instance.
extern leveldb::Cache* GetSharedBrowserBlockCache();

}  // namespace leveldb_chrome

#endif  // THIRD_PARTY_LEVELDATABASE_LEVELDB_CHROME_H_
