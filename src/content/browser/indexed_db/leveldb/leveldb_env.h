// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ENV_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ENV_H_

#include <tuple>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "content/browser/indexed_db/leveldb/leveldb_comparator.h"
#include "content/browser/indexed_db/scopes/leveldb_state.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {

// The leveldb::Env used by the Indexed DB backend.
class LevelDBEnv : public leveldb_env::ChromiumEnv {
  LevelDBEnv();

 public:
  friend struct base::LazyInstanceTraitsBase<LevelDBEnv>;

  CONTENT_EXPORT static LevelDBEnv* Get();
};

namespace indexed_db {

// Factory class used to open leveldb databases, and stores all necessary
// objects in a LevelDBState. This interface exists so that it can be mocked out
// for tests.
class CONTENT_EXPORT LevelDBFactory {
 public:
  virtual ~LevelDBFactory() {}

  // Opens a leveldb database with the given comparators and populates it in
  // |output_state|. If the |file_name| is empty, then the database will be
  // in-memory. The comparator names must match.
  virtual std::
      tuple<scoped_refptr<LevelDBState>, leveldb::Status, bool /* disk_full*/>
      OpenLevelDB(const base::FilePath& file_name,
                  const LevelDBComparator* idb_comparator,
                  const leveldb::Comparator* ldb_comparator) const = 0;

  // A somewhat safe way to destroy a leveldb database. This asserts that there
  // are no other references to the given LevelDBState, and deletes the database
  // on disk. |level_db_state| must only have one ref.
  virtual leveldb::Status DestroyLevelDB(
      scoped_refptr<LevelDBState> output_state) const = 0;

  // Assumes that there is no leveldb database currently running for this path.
  virtual leveldb::Status DestroyLevelDB(const base::FilePath& path) const = 0;
};

class CONTENT_EXPORT DefaultLevelDBFactory : public LevelDBFactory {
 public:
  DefaultLevelDBFactory() = default;
  ~DefaultLevelDBFactory() override {}
  std::tuple<scoped_refptr<LevelDBState>, leveldb::Status, bool /* disk_full*/>
  OpenLevelDB(const base::FilePath& file_name,
              const LevelDBComparator* idb_comparator,
              const leveldb::Comparator* ldb_comparator) const override;
  leveldb::Status DestroyLevelDB(
      scoped_refptr<LevelDBState> output_state) const override;
  leveldb::Status DestroyLevelDB(const base::FilePath& path) const override;
};

// Returns a singleton default factory.
CONTENT_EXPORT DefaultLevelDBFactory* GetDefaultLevelDBFactory();

}  // namespace indexed_db
}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ENV_H_
