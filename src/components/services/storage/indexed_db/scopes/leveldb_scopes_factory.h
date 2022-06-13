// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_FACTORY_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_FACTORY_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <tuple>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {
class LevelDBScopes;
class LevelDBState;
class ScopesLockManager;

struct LevelDBScopesOptions {
  LevelDBScopesOptions();

  LevelDBScopesOptions(const LevelDBScopesOptions&) = delete;
  LevelDBScopesOptions& operator=(const LevelDBScopesOptions&) = delete;

  LevelDBScopesOptions(LevelDBScopesOptions&&) noexcept;
  LevelDBScopesOptions& operator=(LevelDBScopesOptions&&) noexcept;

  ~LevelDBScopesOptions();

  std::vector<uint8_t> metadata_key_prefix;
  size_t max_write_batch_size = 1 * 1024 * 1024;
  raw_ptr<ScopesLockManager> lock_manager = nullptr;
  base::RepeatingCallback<void(leveldb::Status)> failure_callback;
};

// The user must still call |StartTaskRunners| on the LevelDBScopes object after
// calling |OpenLevelDBScopes|.
class LevelDBScopesFactory {
 public:
  virtual ~LevelDBScopesFactory() = default;
  virtual std::tuple<std::unique_ptr<LevelDBScopes>, leveldb::Status>
  CreateAndInitializeLevelDBScopes(LevelDBScopesOptions options,
                                   scoped_refptr<LevelDBState> level_db) = 0;
};

class DefaultLevelDBScopesFactory : public LevelDBScopesFactory {
 public:
  ~DefaultLevelDBScopesFactory() override = default;
  std::tuple<std::unique_ptr<LevelDBScopes>, leveldb::Status>
  CreateAndInitializeLevelDBScopes(
      LevelDBScopesOptions options,
      scoped_refptr<LevelDBState> level_db) override;
};

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_FACTORY_H_
