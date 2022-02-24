// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_TEST_UTILS_H_
#define COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_TEST_UTILS_H_

#include <queue>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom-forward.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "url/origin.h"

namespace base {
class FilePath;
}  // namespace base

namespace sql {
class Database;
}  // namespace sql

namespace storage {

using OriginMatcherFunction = SharedStorageDatabase::OriginMatcherFunction;
using InitStatus = SharedStorageDatabase::InitStatus;
using SetBehavior = SharedStorageDatabase::SetBehavior;
using OperationResult = SharedStorageDatabase::OperationResult;
using GetResult = SharedStorageDatabase::GetResult;

// Helper class for testing async operations, accessible here for unit tests
// of both `AsyncSharedStorageDatabase` and `SharedStorageManager`.
class TestDatabaseOperationReceiver {
 public:
  struct DBOperation {
    enum class Type {
      DB_DESTROY = 0,
      DB_TRIM_MEMORY = 1,
      DB_GET = 2,
      DB_SET = 3,
      DB_APPEND = 4,
      DB_DELETE = 5,
      DB_CLEAR = 6,
      DB_LENGTH = 7,
      DB_KEY = 8,
      DB_PURGE_MATCHING = 9,
      DB_PURGE_STALE = 10,
      DB_FETCH_ORIGINS = 11,
      DB_IS_OPEN = 12,
      DB_STATUS = 13,
      DB_OVERRIDE_TIME = 14,
    } type;
    url::Origin origin;
    std::vector<std::u16string> params;
    explicit DBOperation(Type type);
    DBOperation(Type type, url::Origin origin);
    DBOperation(Type type,
                url::Origin origin,
                std::vector<std::u16string> params);
    DBOperation(Type type, std::vector<std::u16string> params);
    DBOperation(const DBOperation&);
    ~DBOperation();
    bool operator==(const DBOperation& operation) const;
    bool operator!=(const DBOperation& operation) const;
    std::string Serialize() const;
  };

  TestDatabaseOperationReceiver();

  ~TestDatabaseOperationReceiver();

  // For serializing parameters to insert into `params` when creating a
  // `DBOperation` struct.
  static std::u16string SerializeTime(base::Time time);
  static std::u16string SerializeTimeDelta(base::TimeDelta delta);
  static std::u16string SerializeBool(bool b);
  static std::u16string SerializeSetBehavior(SetBehavior behavior);

  bool is_finished() const { return finished_; }

  void set_expected_operations(std::queue<DBOperation> expected_operations) {
    expected_operations_ = std::move(expected_operations);
  }

  void WaitForOperations();

  void GetResultCallbackBase(const DBOperation& current_operation,
                             GetResult* out_result,
                             GetResult result);
  base::OnceCallback<void(GetResult)> MakeGetResultCallback(
      const DBOperation& current_operation,
      GetResult* out_result);

  void OperationResultCallbackBase(const DBOperation& current_operation,
                                   OperationResult* out_result,
                                   OperationResult result);
  base::OnceCallback<void(OperationResult)> MakeOperationResultCallback(
      const DBOperation& current_operation,
      OperationResult* out_result);

  void IntCallbackBase(const DBOperation& current_operation,
                       int* out_length,
                       int length);
  base::OnceCallback<void(int)> MakeIntCallback(
      const DBOperation& current_operation,
      int* out_length);

  void BoolCallbackBase(const DBOperation& current_operation,
                        bool* out_boolean,
                        bool boolean);
  base::OnceCallback<void(bool)> MakeBoolCallback(
      const DBOperation& current_operation,
      bool* out_boolean);

  void StatusCallbackBase(const DBOperation& current_operation,
                          InitStatus* out_status,
                          InitStatus status);
  base::OnceCallback<void(InitStatus)> MakeStatusCallback(
      const DBOperation& current_operation,
      InitStatus* out_status);

  void InfosCallbackBase(const DBOperation& current_operation,
                         std::vector<mojom::StorageUsageInfoPtr>* out_infos,
                         std::vector<mojom::StorageUsageInfoPtr> infos);
  base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
  MakeInfosCallback(const DBOperation& current_operation,
                    std::vector<mojom::StorageUsageInfoPtr>* out_infos);

  void OnceClosureBase(const DBOperation& current_operation);
  base::OnceClosure MakeOnceClosure(const DBOperation& current_operation);

 private:
  bool ExpectationsMet(const DBOperation& current_operation);
  void Finish();

  base::RunLoop loop_;
  bool finished_ = true;
  std::queue<DBOperation> expected_operations_;
};

class OriginMatcherFunctionUtility {
 public:
  OriginMatcherFunctionUtility();
  ~OriginMatcherFunctionUtility();

  [[nodiscard]] static OriginMatcherFunction MakeMatcherFunction(
      std::vector<url::Origin> origins_to_match);

  [[nodiscard]] static OriginMatcherFunction MakeMatcherFunction(
      std::vector<std::string> origin_strs_to_match);

  [[nodiscard]] size_t RegisterMatcherFunction(
      std::vector<url::Origin> origins_to_match);

  [[nodiscard]] OriginMatcherFunction TakeMatcherFunctionForId(size_t id);

  [[nodiscard]] bool is_empty() const { return matcher_table_.empty(); }

  [[nodiscard]] size_t size() const { return matcher_table_.size(); }

 private:
  std::vector<OriginMatcherFunction> matcher_table_;
};

// Wraps a bool indicating if the database is in memory only,
// for the purpose of customizing a `PrintToString()` method below, which will
// be used in the parameterized test names via
// `testing::PrintToStringParamName()`.
struct SharedStorageWrappedBool {
  bool in_memory_only;
};

// Wraps bools indicating if the database is in memory only and if storage
// cleanup should be performed after purging, for the purpose of customizing a
// `PrintToString()` method below, which will be used in the parameterized test
// names via `testing::PrintToStringParamName()`.
struct PurgeMatchingOriginsParams {
  bool in_memory_only;
  bool perform_storage_cleanup;
};

[[nodiscard]] std::vector<SharedStorageWrappedBool>
GetSharedStorageWrappedBools();

// Used by `testing::PrintToStringParamName()`.
[[nodiscard]] std::string PrintToString(const SharedStorageWrappedBool& b);

[[nodiscard]] std::vector<PurgeMatchingOriginsParams>
GetPurgeMatchingOriginsParams();

// Used by testing::PrintToStringParamName().
[[nodiscard]] std::string PrintToString(const PurgeMatchingOriginsParams& p);

// Verify that the up-to-date SQL Shared Storage database has the expected
// tables and columns. Functional tests only check whether the things which
// should be there are, but do not check if extraneous items are
// present. Any extraneous items have the potential to interact
// negatively with future schema changes.
void VerifySharedStorageTablesAndColumns(sql::Database& db);

[[nodiscard]] bool GetTestDataSharedStorageDir(base::FilePath* dir);

[[nodiscard]] bool CreateDatabaseFromSQL(const base::FilePath& db_path,
                                         const char* ascii_path);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_TEST_UTILS_H_
