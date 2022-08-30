// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_ASYNC_SHARED_STORAGE_DATABASE_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_ASYNC_SHARED_STORAGE_DATABASE_IMPL_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/shared_storage/async_shared_storage_database.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "url/origin.h"

namespace base {
class FilePath;
class Time;
class TimeDelta;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace storage {
struct SharedStorageDatabaseOptions;
class SpecialStoragePolicy;

// A wrapper around SharedStorageDatabase which makes the operations
// asynchronous.
class AsyncSharedStorageDatabaseImpl : public AsyncSharedStorageDatabase {
 public:
  using InitStatus = SharedStorageDatabase::InitStatus;
  using SetBehavior = SharedStorageDatabase::SetBehavior;
  using OperationResult = SharedStorageDatabase::OperationResult;
  using GetResult = SharedStorageDatabase::GetResult;
  using BudgetResult = SharedStorageDatabase::BudgetResult;

  // A callback type to check if a given origin matches a storage policy.
  // Can be passed empty/null where used, which means the origin will always
  // match.
  using OriginMatcherFunction = SharedStorageDatabase::OriginMatcherFunction;

  // Creates an `AsyncSharedStorageDatabase` instance. If `db_path` is empty,
  // creates a temporary, in-memory database; otherwise creates a persistent
  // database within a filesystem directory given by `db_path`, which must be an
  // absolute path. If file-backed, the database may or may not already exist at
  // `db_path`, and if it doesn't, it will be created.
  //
  // The instance will be bound to and perform all operations on
  // `blocking_task_runner`, which must support blocking operations.
  static std::unique_ptr<AsyncSharedStorageDatabase> Create(
      base::FilePath db_path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<SharedStorageDatabaseOptions> options);

  AsyncSharedStorageDatabaseImpl(const AsyncSharedStorageDatabaseImpl&) =
      delete;
  AsyncSharedStorageDatabaseImpl(const AsyncSharedStorageDatabaseImpl&&) =
      delete;

  ~AsyncSharedStorageDatabaseImpl() override;

  AsyncSharedStorageDatabaseImpl& operator=(
      const AsyncSharedStorageDatabaseImpl&) = delete;
  AsyncSharedStorageDatabaseImpl& operator=(
      const AsyncSharedStorageDatabaseImpl&&) = delete;

  // AsyncSharedStorageDatabase
  void Destroy(base::OnceCallback<void(bool)> callback) override;
  void TrimMemory(base::OnceClosure callback) override;
  void Get(url::Origin context_origin,
           std::u16string key,
           base::OnceCallback<void(GetResult)> callback) override;
  void Set(url::Origin context_origin,
           std::u16string key,
           std::u16string value,
           base::OnceCallback<void(OperationResult)> callback,
           SetBehavior behavior = SetBehavior::kDefault) override;
  void Append(url::Origin context_origin,
              std::u16string key,
              std::u16string value,
              base::OnceCallback<void(OperationResult)> callback) override;
  void Delete(url::Origin context_origin,
              std::u16string key,
              base::OnceCallback<void(OperationResult)> callback) override;
  void Clear(url::Origin context_origin,
             base::OnceCallback<void(OperationResult)> callback) override;
  void Length(url::Origin context_origin,
              base::OnceCallback<void(int)> callback) override;
  void Keys(url::Origin context_origin,
            mojo::PendingRemote<
                shared_storage_worklet::mojom::SharedStorageEntriesListener>
                pending_listener,
            base::OnceCallback<void(OperationResult)> callback) override;
  void Entries(url::Origin context_origin,
               mojo::PendingRemote<
                   shared_storage_worklet::mojom::SharedStorageEntriesListener>
                   pending_listener,
               base::OnceCallback<void(OperationResult)> callback) override;
  void PurgeMatchingOrigins(OriginMatcherFunction origin_matcher,
                            base::Time begin,
                            base::Time end,
                            base::OnceCallback<void(OperationResult)> callback,
                            bool perform_storage_cleanup = false) override;
  void PurgeStaleOrigins(
      base::TimeDelta window_to_be_deemed_active,
      base::OnceCallback<void(OperationResult)> callback) override;
  void FetchOrigins(
      base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
          callback) override;
  void MakeBudgetWithdrawal(
      url::Origin context_origin,
      double bits_debit,
      base::OnceCallback<void(OperationResult)> callback) override;
  void GetRemainingBudget(
      url::Origin context_origin,
      base::OnceCallback<void(BudgetResult)> callback) override;

  // Gets the underlying database for tests.
  base::SequenceBound<SharedStorageDatabase>*
  GetSequenceBoundDatabaseForTesting();

  // Asynchronously determines whether the database is open.
  void IsOpenForTesting(base::OnceCallback<void(bool)> callback);

  // Asynchronously determines the database `InitStatus`. Useful for testing.
  void DBStatusForTesting(base::OnceCallback<void(InitStatus)> callback);

  // Changes `last_used_time` to `new_last_used_time` for `context_origin`.
  void OverrideLastUsedTimeForTesting(url::Origin context_origin,
                                      base::Time new_last_used_time,
                                      base::OnceCallback<void(bool)> callback);

  // Overrides the `SpecialStoragePolicy` for tests.
  void OverrideSpecialStoragePolicyForTesting(
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);

  // Overrides the clock used to check the time.
  void OverrideClockForTesting(base::Clock* clock, base::OnceClosure callback);

  // Calls `callback` with the number of entries (including stale entries) in
  // the table `budget_mapping` for `context_origin`, or with -1 in case of
  // database initialization failure or SQL error.
  void GetNumBudgetEntriesForTesting(url::Origin context_origin,
                                     base::OnceCallback<void(int)> callback);

  // Calls `callback` with the total number of entries in the table for all
  // origins, or with -1 in case of database initialization failure or SQL
  // error.
  void GetTotalNumBudgetEntriesForTesting(
      base::OnceCallback<void(int)> callback);

 private:
  // Instances should be obtained from the `Create()` factory method.
  AsyncSharedStorageDatabaseImpl(
      base::FilePath db_path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<SharedStorageDatabaseOptions> options);

  base::SequenceBound<SharedStorageDatabase> database_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_ASYNC_SHARED_STORAGE_DATABASE_IMPL_H_
