// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_FAKE_LEVELDB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_FAKE_LEVELDB_FACTORY_H_

#include <queue>
#include <utility>

#include "content/browser/indexed_db/leveldb/leveldb_env.h"

namespace content {
namespace indexed_db {

// Used for unittests, this factory will only create in-memory leveldb
// databases, and will optionally allow the user to override the next
// LevelDBState returned with |EnqueueNextLevelDBState|.
class FakeLevelDBFactory : public DefaultLevelDBFactory {
 public:
  FakeLevelDBFactory();
  ~FakeLevelDBFactory() override;

  static scoped_refptr<LevelDBState> GetBrokenLevelDB(
      leveldb::Status error_to_return,
      const base::FilePath& reported_file_path);

  void EnqueueNextOpenLevelDBResult(scoped_refptr<LevelDBState> state,
                                    leveldb::Status status,
                                    bool is_disk_full);

  std::tuple<scoped_refptr<LevelDBState>, leveldb::Status, bool /*disk_full*/>
  OpenLevelDB(const base::FilePath& file_name,
              const LevelDBComparator* idb_comparator,
              const leveldb::Comparator* ldb_comparator) const override;

 private:
  mutable std::queue<std::tuple<scoped_refptr<LevelDBState>,
                                leveldb::Status,
                                bool /*disk_full*/>>
      next_leveldb_states_;
};

}  // namespace indexed_db
}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_FAKE_LEVELDB_FACTORY_H_
