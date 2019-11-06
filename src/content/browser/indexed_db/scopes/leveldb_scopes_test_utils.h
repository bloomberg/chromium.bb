// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_SCOPES_LEVELDB_SCOPES_TEST_UTILS_H_
#define CONTENT_BROWSER_INDEXED_DB_SCOPES_LEVELDB_SCOPES_TEST_UTILS_H_

#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/indexed_db/leveldb/fake_leveldb_factory.h"
#include "content/browser/indexed_db/scopes/leveldb_scopes_coding.h"
#include "content/browser/indexed_db/scopes/leveldb_state.h"
#include "content/browser/indexed_db/scopes/scopes_lock_manager.h"
#include "content/browser/indexed_db/scopes/scopes_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {

class LevelDBScopesTestBase : public testing::Test {
 public:
  static constexpr const size_t kWriteBatchSizeForTesting = 1024;

  LevelDBScopesTestBase();
  ~LevelDBScopesTestBase() override;

  void SetUp() override;

  // Ensures that |leveldb_| is destroyed correctly, and all contents are
  // deleted off disk.
  void TearDown() override;

  // Initializes |leveldb_| to be a read database backed on disk.
  void SetUpRealDatabase();

  // Initializes |leveldb_| to be a database that will break when |callback| is
  // called, with the given status.
  void SetUpBreakableDB(base::OnceCallback<void(leveldb::Status)>* callback);

  // Initializes |leveldb_| to be a flaky database that will
  void SetUpFlakyDB(
      std::queue<indexed_db::FakeLevelDBFactory::FlakePoint> flake_points);

  // Writes |metadata_buffer_| to disk.
  void WriteScopesMetadata(int64_t scope_number, bool ignore_cleanup_tasks);

  // Writes |undo_task_buffer_| to disk.
  void WriteUndoTask(int64_t scope_number, int64_t sequence_number);

  // Writes |cleanup_task_buffer_| to disk.
  void WriteCleanupTask(int64_t scope_number, int64_t sequence_number);

  // Writes a large value to the given key (|large_string_|).
  void WriteLargeValue(const std::string& key);

  // Loads the given key into |value_buffer_|.
  leveldb::Status LoadAt(const std::string& key);

  // Loads the scope metadata into |value_buffer_|.
  leveldb::Status LoadScopeMetadata(int64_t scope_number);

  // Loads the undo task into |value_buffer_|.
  leveldb::Status LoadUndoTask(int64_t scope_number, int64_t sequence_number);

  // Loads the cleanup task into |value_buffer_|.
  leveldb::Status LoadCleanupTask(int64_t scope_number,
                                  int64_t sequence_number);

  // Returns if the database doesn't have any entries with the given key prefix.
  bool IsPrefixedRangeEmptyInDB(leveldb::Slice key);

  // Returns if all of the scope log data and metadata is gone.
  bool IsScopeCleanedUp(int64_t scope_number);

  // Returns if any scope data exists, including metadata or log data (other
  // than global metadata).
  bool ScopeDataExistsOnDisk();

  // Creates a shared lock request from |simple_lock_begin_| to
  // |simple_lock_end_|.
  ScopesLockManager::ScopeLockRequest CreateSimpleSharedLock();
  // Creates a exclusive lock request from |simple_lock_begin_| to
  // |simple_lock_end_|.
  ScopesLockManager::ScopeLockRequest CreateSimpleExclusiveLock();

  ScopesLockManager::ScopeLockRequest CreateSharedLock(int i);
  ScopesLockManager::ScopeLockRequest CreateExclusiveLock(int i);

 protected:
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_;
  base::test::ScopedTaskEnvironment task_env_;
  base::ScopedTempDir temp_directory_;

  const std::string simple_lock_begin_ = "0000000001";
  const std::string simple_lock_end_ = "0000000010";
  const std::vector<uint8_t> metadata_prefix_ = {'a'};
  const std::vector<uint8_t> db_prefix_ = {'b'};

  scoped_refptr<LevelDBState> leveldb_;
  std::string large_string_;
  LevelDBScopesUndoTask undo_task_buffer_;
  LevelDBScopesCleanupTask cleanup_task_buffer_;
  LevelDBScopesScopeMetadata metadata_buffer_;

  ScopesEncoder scopes_encoder_;
  std::string value_buffer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_SCOPES_LEVELDB_SCOPES_TEST_UTILS_H_
