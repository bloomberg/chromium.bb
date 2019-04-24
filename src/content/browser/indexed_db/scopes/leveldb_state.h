// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_SCOPES_LEVELDB_STATE_H_
#define CONTENT_BROWSER_INDEXED_DB_SCOPES_LEVELDB_STATE_H_

#include "base/memory/ref_counted.h"

#include <memory>

#include "base/files/file_path.h"
#include "content/browser/indexed_db/leveldb/leveldb_comparator.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"

namespace content {

// Encapsulates a leveldb database and comparator, allowing them to be used
// safely across thread boundaries.
class CONTENT_EXPORT LevelDBState
    : public base::RefCountedThreadSafe<LevelDBState> {
 public:
  static scoped_refptr<LevelDBState> CreateForDiskDB(
      const leveldb::Comparator* comparator,
      const LevelDBComparator* idb_comparator,
      std::unique_ptr<leveldb::DB> database,
      base::FilePath database_path);

  static scoped_refptr<LevelDBState> CreateForInMemoryDB(
      std::unique_ptr<leveldb::Env> in_memory_env,
      const leveldb::Comparator* comparator,
      const LevelDBComparator* idb_comparator,
      std::unique_ptr<leveldb::DB> in_memory_database,
      std::string name_for_tracing);

  const leveldb::Comparator* comparator() const { return comparator_; }
  const LevelDBComparator* idb_comparator() const { return idb_comparator_; }
  leveldb::DB* db() const { return db_.get(); }
  const std::string& name_for_tracing() const { return name_for_tracing_; }

  // Null for on-disk databases.
  leveldb::Env* in_memory_env() const { return in_memory_env_.get(); }
  // Empty for in-memory databases.
  const base::FilePath& database_path() const { return database_path_; }

 private:
  friend class base::RefCountedThreadSafe<LevelDBState>;

  LevelDBState(std::unique_ptr<leveldb::Env> optional_in_memory_env,
               const leveldb::Comparator* comparator,
               const LevelDBComparator* idb_comparator,
               std::unique_ptr<leveldb::DB> database,
               base::FilePath database_path,
               std::string name_for_tracing);
  ~LevelDBState();

  const std::unique_ptr<leveldb::Env> in_memory_env_;
  const leveldb::Comparator* comparator_;
  const LevelDBComparator* idb_comparator_;
  const std::unique_ptr<leveldb::DB> db_;
  const base::FilePath database_path_;
  const std::string name_for_tracing_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_SCOPES_LEVELDB_STATE_H_
