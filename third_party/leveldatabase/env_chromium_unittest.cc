// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/test_suite.h"
#include "base/trace_event/process_memory_dump.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

#define FPL FILE_PATH_LITERAL

using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::ProcessMemoryDump;
using leveldb::DB;
using leveldb::Env;
using leveldb::ReadOptions;
using leveldb::Slice;
using leveldb::Status;
using leveldb::WritableFile;
using leveldb::WriteOptions;
using leveldb_env::ChromiumEnv;
using leveldb_env::DBTracker;
using leveldb_env::MethodID;
using leveldb_env::Options;

namespace leveldb_env {

static const int kReadOnlyFileLimit = 4;

TEST(ErrorEncoding, OnlyAMethod) {
  const MethodID in_method = leveldb_env::kSequentialFileRead;
  const Status s = MakeIOError("Somefile.txt", "message", in_method);
  MethodID method;
  base::File::Error error = base::File::FILE_ERROR_MAX;
  EXPECT_EQ(leveldb_env::METHOD_ONLY, ParseMethodAndError(s, &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(base::File::FILE_ERROR_MAX, error);
}

TEST(ErrorEncoding, FileError) {
  const MethodID in_method = leveldb_env::kWritableFileClose;
  const base::File::Error fe = base::File::FILE_ERROR_INVALID_OPERATION;
  const Status s = MakeIOError("Somefile.txt", "message", in_method, fe);
  MethodID method;
  base::File::Error error;
  EXPECT_EQ(leveldb_env::METHOD_AND_BFE,
            ParseMethodAndError(s, &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(fe, error);
}

TEST(ErrorEncoding, NoEncodedMessage) {
  Status s = Status::IOError("Some message", "from leveldb itself");
  MethodID method = leveldb_env::kRandomAccessFileRead;
  base::File::Error error = base::File::FILE_ERROR_MAX;
  EXPECT_EQ(leveldb_env::NONE, ParseMethodAndError(s, &method, &error));
  EXPECT_EQ(leveldb_env::kRandomAccessFileRead, method);
  EXPECT_EQ(base::File::FILE_ERROR_MAX, error);
}

template <typename T>
class ChromiumEnvMultiPlatformTests : public ::testing::Test {
 public:
};

typedef ::testing::Types<ChromiumEnv> ChromiumEnvMultiPlatformTestsTypes;
TYPED_TEST_CASE(ChromiumEnvMultiPlatformTests,
                ChromiumEnvMultiPlatformTestsTypes);

int CountFilesWithExtension(const base::FilePath& dir,
                            const base::FilePath::StringType& extension) {
  int matching_files = 0;
  base::FileEnumerator dir_reader(
      dir, false, base::FileEnumerator::FILES);
  for (base::FilePath fname = dir_reader.Next(); !fname.empty();
       fname = dir_reader.Next()) {
    if (fname.MatchesExtension(extension))
      matching_files++;
  }
  return matching_files;
}

bool GetFirstLDBFile(const base::FilePath& dir, base::FilePath* ldb_file) {
  base::FileEnumerator dir_reader(
      dir, false, base::FileEnumerator::FILES);
  for (base::FilePath fname = dir_reader.Next(); !fname.empty();
       fname = dir_reader.Next()) {
    if (fname.MatchesExtension(FPL(".ldb"))) {
      *ldb_file = fname;
      return true;
    }
  }
  return false;
}

TEST(ChromiumEnv, DeleteBackupTables) {
  Options options;
  options.create_if_missing = true;
  options.env = Env::Default();

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath dir = scoped_temp_dir.GetPath();

  DB* db;
  Status status = DB::Open(options, dir.AsUTF8Unsafe(), &db);
  EXPECT_TRUE(status.ok()) << status.ToString();
  status = db->Put(WriteOptions(), "key", "value");
  EXPECT_TRUE(status.ok()) << status.ToString();
  Slice a = "a";
  Slice z = "z";
  db->CompactRange(&a, &z);  // Ensure manifest written out to table.
  delete db;
  db = nullptr;

  // Current ChromiumEnv no longer makes backup tables - verify for sanity.
  EXPECT_EQ(1, CountFilesWithExtension(dir, FPL(".ldb")));
  EXPECT_EQ(0, CountFilesWithExtension(dir, FPL(".bak")));

  // Manually create our own backup table to simulate opening db created by
  // prior release.
  base::FilePath ldb_path;
  ASSERT_TRUE(GetFirstLDBFile(dir, &ldb_path));
  base::FilePath bak_path = ldb_path.ReplaceExtension(FPL(".bak"));
  ASSERT_TRUE(base::CopyFile(ldb_path, bak_path));
  EXPECT_EQ(1, CountFilesWithExtension(dir, FPL(".bak")));

  // Now reopen and close then verify the backup file was deleted.
  status = DB::Open(options, dir.AsUTF8Unsafe(), &db);
  EXPECT_TRUE(status.ok()) << status.ToString();
  EXPECT_EQ(0, CountFilesWithExtension(dir, FPL(".bak")));
  delete db;
  EXPECT_EQ(1, CountFilesWithExtension(dir, FPL(".ldb")));
  EXPECT_EQ(0, CountFilesWithExtension(dir, FPL(".bak")));
}

TEST(ChromiumEnv, GetChildrenEmptyDir) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath dir = scoped_temp_dir.GetPath();

  Env* env = Env::Default();
  std::vector<std::string> result;
  leveldb::Status status = env->GetChildren(dir.AsUTF8Unsafe(), &result);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(0U, result.size());
}

TEST(ChromiumEnv, GetChildrenPriorResults) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath dir = scoped_temp_dir.GetPath();

  base::FilePath new_file_dir = dir.Append(FPL("tmp_file"));
  FILE* f = fopen(new_file_dir.AsUTF8Unsafe().c_str(), "w");
  if (f) {
    fputs("Temp file contents", f);
    fclose(f);
  }

  Env* env = Env::Default();
  std::vector<std::string> result;
  leveldb::Status status = env->GetChildren(dir.AsUTF8Unsafe(), &result);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(1U, result.size());

  // And a second time should also return one result
  status = env->GetChildren(dir.AsUTF8Unsafe(), &result);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(1U, result.size());
}

TEST(ChromiumEnv, TestWriteBufferSize) {
  // If can't get disk size, use leveldb defaults.
  const int64_t MB = 1024 * 1024;
  EXPECT_EQ(size_t(4 * MB), leveldb_env::WriteBufferSize(-1));

  // A very small disk (check lower clamp value).
  EXPECT_EQ(size_t(1 * MB), leveldb_env::WriteBufferSize(1 * MB));

  // Some value on the linear equation between min and max.
  EXPECT_EQ(size_t(2.5 * MB), leveldb_env::WriteBufferSize(25 * MB));

  // The disk size equating to the max buffer size
  EXPECT_EQ(size_t(4 * MB), leveldb_env::WriteBufferSize(40 * MB));

  // Make sure sizes larger than 40MB are clamped to max buffer size.
  EXPECT_EQ(size_t(4 * MB), leveldb_env::WriteBufferSize(80 * MB));

  // Check for very large disk size (catch overflow).
  EXPECT_EQ(size_t(4 * MB), leveldb_env::WriteBufferSize(100 * MB * MB));
}

TEST(ChromiumEnv, LockFile) {
  base::FilePath tmp_file_path;
  base::CreateTemporaryFile(&tmp_file_path);
  leveldb::FileLock* lock = nullptr;

  Env* env = Env::Default();
  EXPECT_TRUE(env->LockFile(tmp_file_path.MaybeAsASCII(), &lock).ok());
  EXPECT_NE(nullptr, lock);

  leveldb::FileLock* failed_lock = nullptr;
  EXPECT_FALSE(env->LockFile(tmp_file_path.MaybeAsASCII(), &failed_lock).ok());
  EXPECT_EQ(nullptr, failed_lock);

  EXPECT_TRUE(env->UnlockFile(lock).ok());
  EXPECT_TRUE(env->LockFile(tmp_file_path.MaybeAsASCII(), &lock).ok());
  EXPECT_TRUE(env->UnlockFile(lock).ok());
}

TEST(ChromiumEnvTest, TestOpenOnRead) {
  // Write some test data to a single file that will be opened |n| times.
  base::FilePath tmp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&tmp_file_path));

  FILE* f = fopen(tmp_file_path.AsUTF8Unsafe().c_str(), "w");
  ASSERT_TRUE(f != NULL);
  const char kFileData[] = "abcdefghijklmnopqrstuvwxyz";
  fputs(kFileData, f);
  fclose(f);

  std::unique_ptr<ChromiumEnv> env(new ChromiumEnv());
  env->SetReadOnlyFileLimitForTesting(kReadOnlyFileLimit);

  // Open test file some number greater than kReadOnlyFileLimit to force the
  // open-on-read behavior of POSIX Env leveldb::RandomAccessFile.
  const int kNumFiles = kReadOnlyFileLimit + 5;
  leveldb::RandomAccessFile* files[kNumFiles] = {0};
  for (int i = 0; i < kNumFiles; i++) {
    ASSERT_TRUE(
        env->NewRandomAccessFile(tmp_file_path.AsUTF8Unsafe(), &files[i]).ok());
  }
  char scratch;
  Slice read_result;
  for (int i = 0; i < kNumFiles; i++) {
    ASSERT_TRUE(files[i]->Read(i, 1, &read_result, &scratch).ok());
    ASSERT_EQ(kFileData[i], read_result[0]);
  }
  for (int i = 0; i < kNumFiles; i++) {
    delete files[i];
  }
  ASSERT_TRUE(env->DeleteFile(tmp_file_path.AsUTF8Unsafe()).ok());
}

class ChromiumEnvDBTrackerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  const base::FilePath& temp_path() const { return scoped_temp_dir_.GetPath(); }

  using VisitedDBSet = std::set<DBTracker::TrackedDB*>;

  static VisitedDBSet VisitDatabases() {
    VisitedDBSet visited;
    auto db_visitor = [](VisitedDBSet* visited, DBTracker::TrackedDB* db) {
      ASSERT_TRUE(visited->insert(db).second)
          << "Database " << std::hex << db << " visited for the second time";
    };
    DBTracker::GetInstance()->VisitDatabases(
        base::BindRepeating(db_visitor, base::Unretained(&visited)));
    return visited;
  }

  using LiveDBSet = std::vector<std::unique_ptr<DBTracker::TrackedDB>>;

  void AssertEqualSets(const LiveDBSet& live_dbs,
                       const VisitedDBSet& visited_dbs) {
    for (const auto& live_db : live_dbs) {
      ASSERT_EQ(1u, visited_dbs.count(live_db.get()))
          << "Database " << std::hex << live_db.get() << " was not visited";
    }
    ASSERT_EQ(live_dbs.size(), visited_dbs.size())
        << "Extra databases were visited";
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(ChromiumEnvDBTrackerTest, GetOrCreateAllocatorDump) {
  Options options;
  options.create_if_missing = true;
  std::string name = temp_path().AsUTF8Unsafe();
  DBTracker::TrackedDB* tracked_db;
  Status s = DBTracker::GetInstance()->OpenDatabase(options, name, &tracked_db);
  ASSERT_TRUE(s.ok()) << s.ToString();

  const MemoryDumpArgs detailed_args = {MemoryDumpLevelOfDetail::DETAILED};
  ProcessMemoryDump pmd(nullptr, detailed_args);
  auto* mad =
      DBTracker::GetInstance()->GetOrCreateAllocatorDump(&pmd, tracked_db);
  delete tracked_db;
  ASSERT_TRUE(mad != nullptr);

  // Check that the size was added.
  auto& entries = mad->entries();
  ASSERT_EQ(1ul, entries.size());
  EXPECT_EQ(base::trace_event::MemoryAllocatorDump::kNameSize, entries[0].name);
  EXPECT_EQ(base::trace_event::MemoryAllocatorDump::kUnitsBytes,
            entries[0].units);
  EXPECT_GE(entries[0].value_uint64, 0ul);
}

TEST_F(ChromiumEnvDBTrackerTest, OpenDatabase) {
  struct KeyValue {
    const char* key;
    const char* value;
  };
  constexpr KeyValue db_data[] = {
      {"banana", "yellow"}, {"sky", "blue"}, {"enthusiasm", ""},
  };

  // Open a new database using DBTracker::Open, write some data.
  Options options;
  options.create_if_missing = true;
  std::string name = temp_path().AsUTF8Unsafe();
  DBTracker::TrackedDB* tracked_db;
  Status status =
      DBTracker::GetInstance()->OpenDatabase(options, name, &tracked_db);
  ASSERT_TRUE(status.ok()) << status.ToString();
  for (const auto& kv : db_data) {
    status = tracked_db->Put(WriteOptions(), kv.key, kv.value);
    ASSERT_TRUE(status.ok()) << status.ToString();
  }

  // Close the database.
  delete tracked_db;

  // Open the database again with DB::Open, and check the data.
  options.create_if_missing = false;
  leveldb::DB* plain_db = nullptr;
  status = leveldb::DB::Open(options, name, &plain_db);
  ASSERT_TRUE(status.ok()) << status.ToString();
  for (const auto& kv : db_data) {
    std::string value;
    status = plain_db->Get(ReadOptions(), kv.key, &value);
    ASSERT_TRUE(status.ok()) << status.ToString();
    ASSERT_EQ(value, kv.value);
  }
  delete plain_db;
}

TEST_F(ChromiumEnvDBTrackerTest, TrackedDBInfo) {
  Options options;
  options.create_if_missing = true;
  std::string name = temp_path().AsUTF8Unsafe();
  DBTracker::TrackedDB* db;
  Status status = DBTracker::GetInstance()->OpenDatabase(options, name, &db);
  ASSERT_TRUE(status.ok()) << status.ToString();

  // Check that |db| reports info that was used to open it.
  ASSERT_EQ(name, db->name());

  delete db;
}

TEST_F(ChromiumEnvDBTrackerTest, VisitDatabases) {
  LiveDBSet live_dbs;

  // Open several databases.
  for (const char* tag : {"poets", "movies", "recipes", "novels"}) {
    Options options;
    options.create_if_missing = true;
    std::string name = temp_path().AppendASCII(tag).AsUTF8Unsafe();
    DBTracker::TrackedDB* db;
    Status status = DBTracker::GetInstance()->OpenDatabase(options, name, &db);
    ASSERT_TRUE(status.ok()) << status.ToString();
    live_dbs.emplace_back(db);
  }

  // Check that all live databases are visited.
  AssertEqualSets(live_dbs, VisitDatabases());

  // Close couple of a databases.
  live_dbs.erase(live_dbs.begin());
  live_dbs.erase(live_dbs.begin() + 1);

  // Check that only remaining live databases are visited.
  AssertEqualSets(live_dbs, VisitDatabases());
}

TEST_F(ChromiumEnvDBTrackerTest, OpenDBTracking) {
  Options options;
  options.create_if_missing = true;
  std::unique_ptr<leveldb::DB> db;
  auto status = leveldb_env::OpenDB(options, temp_path().AsUTF8Unsafe(), &db);
  ASSERT_TRUE(status.ok()) << status.ToString();

  auto visited_dbs = VisitDatabases();

  // Databases returned by OpenDB() should be tracked.
  ASSERT_EQ(1u, visited_dbs.size());
  ASSERT_EQ(db.get(), *visited_dbs.begin());
}

TEST_F(ChromiumEnvDBTrackerTest, IsTrackedDB) {
  leveldb_env::Options options;
  options.create_if_missing = true;
  leveldb::DB* untracked_db;
  base::ScopedTempDir untracked_temp_dir;
  ASSERT_TRUE(untracked_temp_dir.CreateUniqueTempDir());
  leveldb::Status s = leveldb::DB::Open(
      options, untracked_temp_dir.GetPath().AsUTF8Unsafe(), &untracked_db);
  ASSERT_TRUE(s.ok());
  EXPECT_FALSE(DBTracker::GetInstance()->IsTrackedDB(untracked_db));

  // Now a tracked db.
  std::unique_ptr<leveldb::DB> tracked_db;
  base::ScopedTempDir tracked_temp_dir;
  ASSERT_TRUE(tracked_temp_dir.CreateUniqueTempDir());
  s = leveldb_env::OpenDB(options, tracked_temp_dir.GetPath().AsUTF8Unsafe(),
                          &tracked_db);
  ASSERT_TRUE(s.ok());
  EXPECT_TRUE(DBTracker::GetInstance()->IsTrackedDB(tracked_db.get()));

  delete untracked_db;
}

TEST_F(ChromiumEnvDBTrackerTest, CheckMemEnv) {
  Env* env = leveldb::Env::Default();
  ASSERT_TRUE(env != nullptr);
  EXPECT_FALSE(leveldb_chrome::IsMemEnv(env));

  std::unique_ptr<leveldb::Env> memenv(leveldb_chrome::NewMemEnv(env));
  EXPECT_TRUE(leveldb_chrome::IsMemEnv(memenv.get()));
}

}  // namespace leveldb_env

int main(int argc, char** argv) { return base::TestSuite(argc, argv).Run(); }
