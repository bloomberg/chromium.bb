// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_FAKE_LEVELDB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_FAKE_LEVELDB_FACTORY_H_

#include <memory>
#include <queue>
#include <utility>

#include "base/sequence_checker.h"
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

  struct FlakePoint {
    int calls_before_flake;
    leveldb::Status flake_status;
    std::string replaced_get_result;
  };

  // The returned callback will trigger the database to be broken, and forever
  // return the given status.
  static std::unique_ptr<leveldb::DB> CreateFlakyDB(
      std::unique_ptr<leveldb::DB> db,
      std::queue<FlakePoint> flake_points);

  static scoped_refptr<LevelDBState> GetBrokenLevelDB(
      leveldb::Status error_to_return,
      const base::FilePath& reported_file_path);

  // The returned callback will trigger the database to be broken, and forever
  // return the given status.
  static std::pair<std::unique_ptr<leveldb::DB>,
                   base::OnceCallback<void(leveldb::Status)>>
  CreateBreakableDB(std::unique_ptr<leveldb::DB> db);

  void EnqueueNextOpenDBResult(std::unique_ptr<leveldb::DB>,
                               leveldb::Status status);

  std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status> OpenDB(
      const leveldb_env::Options& options,
      const std::string& name) override;

  void EnqueueNextOpenLevelDBStateResult(scoped_refptr<LevelDBState> state,
                                         leveldb::Status status,
                                         bool is_disk_full);

  std::tuple<scoped_refptr<LevelDBState>, leveldb::Status, bool /*disk_full*/>
  OpenLevelDBState(const base::FilePath& file_name,
                   const LevelDBComparator* idb_comparator,
                   const leveldb::Comparator* ldb_comparator) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::queue<std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status>>
      next_dbs_;
  std::queue<std::tuple<scoped_refptr<LevelDBState>,
                        leveldb::Status,
                        bool /*disk_full*/>>
      next_leveldb_states_;
};

}  // namespace indexed_db
}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_FAKE_LEVELDB_FACTORY_H_
