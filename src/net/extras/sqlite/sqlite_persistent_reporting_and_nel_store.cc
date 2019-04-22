// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_persistent_reporting_and_nel_store.h"

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "net/extras/sqlite/sqlite_persistent_store_backend_base.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"

namespace net {

namespace {
// Version 1: Adds a table for NEL policies.

const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;
}  // namespace

class SQLitePersistentReportingAndNELStore::Backend
    : public SQLitePersistentStoreBackendBase {
 public:
  Backend(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
      : SQLitePersistentStoreBackendBase(
            path,
            /* histogram_tag = */ "ReportingAndNEL",
            kCurrentVersionNumber,
            kCompatibleVersionNumber,
            background_task_runner,
            client_task_runner),
        num_pending_(0) {}

  void LoadNELPolicies(NELPoliciesLoadedCallback loaded_callback);
  void AddNELPolicy(const NetworkErrorLoggingService::NELPolicy& policy);
  void UpdateNELPolicyAccessTime(
      const NetworkErrorLoggingService::NELPolicy& policy);
  void DeleteNELPolicy(const NetworkErrorLoggingService::NELPolicy& policy);

  // Gets the number of queued operations.
  size_t GetQueueLengthForTesting() const;

 private:
  ~Backend() override {
    DCHECK(nel_policy_pending_ops_.empty());
    DCHECK_EQ(0u, num_pending_);
  }

  // Represents a mutating operation to the database, specified by a type (add,
  // update access time, or delete) and data representing the entry in the
  // database to be added/updated/deleted.
  template <typename DataType>
  class PendingOperation;

  // List of pending operations for a particular entry in the database.
  template <typename DataType>
  using PendingOperationsVector =
      std::vector<std::unique_ptr<PendingOperation<DataType>>>;

  // A copy of the information relevant to a NEL policy.
  struct NELPolicyInfo;
  // Map of pending operations pertaining to NEL policies, keyed on origin of
  // the policy.
  using NELPolicyPendingOperationsMap =
      std::map<url::Origin, PendingOperationsVector<NELPolicyInfo>>;

  // TODO(chlily): add types for Reporting data.

  // SQLitePersistentStoreBackendBase implementation
  bool CreateDatabaseSchema() override;
  base::Optional<int> DoMigrateDatabaseSchema() override;
  void DoCommit() override;

  // Commit a pending operation pertaining to a NEL policy.
  void CommitNELPolicyOperation(PendingOperation<NELPolicyInfo>* op);

  // Add a pending NEL Policy operation to the queue.
  void BatchNELPolicyOperation(
      const url::Origin& origin,
      std::unique_ptr<PendingOperation<NELPolicyInfo>> po);

  // If there are existing pending operations for a given key, potentially
  // remove some of the existing operations before adding |new_op|.
  // In particular, if |new_op| is a deletion, then all the previous pending
  // operations are made irrelevant and can be deleted. If |new_op| is an
  // update-access-time, and the last operation in |ops_for_key| is also an
  // update-access-time, then it can be discarded because |new_op| is about to
  // overwrite the access time with a new value anyway.
  template <typename DataType>
  void MaybeCoalesceOperations(PendingOperationsVector<DataType>* ops_for_key,
                               PendingOperation<DataType>* new_op)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // After adding a pending operation to one of the pending operations queues,
  // this method posts a task to commit all pending operations if we reached the
  // batch size, or starts a timer to commit after a time interval if we just
  // started a new batch. |num_pending| is the total number of pending
  // operations after the one we just added.
  void OnOperationBatched(size_t num_pending);

  // Loads NEL policies into a vector in the background, then posts a
  // task to the client task runner to call |loaded_callback| with the loaded
  // NEL policies.
  void LoadNELPoliciesAndNotifyInBackground(
      NELPoliciesLoadedCallback loaded_callback);

  // Calls |loaded_callback| with the loaded NEL policies (which may be empty if
  // loading was unsuccessful). If loading was successful, also report metrics.
  void CompleteLoadNELPoliciesAndNotifyInForeground(
      NELPoliciesLoadedCallback loaded_callback,
      std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies,
      bool load_success);

  // Total number of pending operations (may not match the sum of the number of
  // elements in the pending operations queues, due to operation coalescing).
  size_t num_pending_ GUARDED_BY(lock_);

  // Queue of pending operations pertaining to NEL policies, keyed on origin.
  NELPolicyPendingOperationsMap nel_policy_pending_ops_ GUARDED_BY(lock_);

  // TODO(chlily): a separate map to hold pending operations for each type of
  // Reporting data.

  // Protects |num_pending_|, and all the pending operations queues.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

namespace {

bool CreateV1NELPoliciesSchema(sql::Database* db) {
  DCHECK(!db->DoesTableExist("nel_policies"));

  std::string stmt =
      "CREATE TABLE nel_policies ("
      "  origin_scheme TEXT NOT NULL,"
      "  origin_host TEXT NOT NULL,"
      "  origin_port INTEGER NOT NULL,"
      "  received_ip_address TEXT NOT NULL,"
      "  report_to TEXT NOT NULL,"
      "  expires_us_since_epoch INTEGER NOT NULL,"
      "  success_fraction REAL NOT NULL,"
      "  failure_fraction REAL NOT NULL,"
      "  is_include_subdomains INTEGER NOT NULL,"
      "  last_access_us_since_epoch INTEGER NOT NULL,"
      // Each origin specifies at most one NEL policy.
      "  UNIQUE (origin_scheme, origin_host, origin_port)"
      ")";

  return db->Execute(stmt.c_str());
}

}  // namespace

template <typename DataType>
class SQLitePersistentReportingAndNELStore::Backend::PendingOperation {
 public:
  enum class Type { ADD, UPDATE_ACCESS_TIME, DELETE };

  PendingOperation(Type type, DataType data)
      : type_(type), data_(std::move(data)) {}

  Type type() const { return type_; }
  const DataType& data() const { return data_; }

 private:
  const Type type_;
  const DataType data_;
};

// Makes a copy of the relevant information about a NELPolicy, stored in a
// form suitable for adding to the database.
struct SQLitePersistentReportingAndNELStore::Backend::NELPolicyInfo {
  NELPolicyInfo(const NetworkErrorLoggingService::NELPolicy& nel_policy)
      : origin_scheme(nel_policy.origin.scheme()),
        origin_host(nel_policy.origin.host()),
        origin_port(nel_policy.origin.port()),
        received_ip_address(nel_policy.received_ip_address.ToString()),
        report_to(nel_policy.report_to),
        expires_us_since_epoch(
            nel_policy.expires.ToDeltaSinceWindowsEpoch().InMicroseconds()),
        success_fraction(nel_policy.success_fraction),
        failure_fraction(nel_policy.failure_fraction),
        is_include_subdomains(nel_policy.include_subdomains),
        last_access_us_since_epoch(
            nel_policy.last_used.ToDeltaSinceWindowsEpoch().InMicroseconds()) {}

  // Origin the policy was received from.
  std::string origin_scheme;
  std::string origin_host;
  int origin_port = 0;
  // IP address of the server that the policy was received from.
  std::string received_ip_address;
  // The Reporting group which the policy specifies.
  std::string report_to;
  // When the policy expires, in microseconds since the Windows epoch.
  int64_t expires_us_since_epoch = 0;
  // Sampling fractions.
  double success_fraction = 0.0;
  double failure_fraction = 0.0;
  // Whether the policy applies to subdomains of the origin.
  bool is_include_subdomains = false;
  // Last time the policy was updated or used, in microseconds since the
  // Windows epoch.
  int64_t last_access_us_since_epoch = 0;
};

void SQLitePersistentReportingAndNELStore::Backend::LoadNELPolicies(
    NELPoliciesLoadedCallback loaded_callback) {
  PostBackgroundTask(
      FROM_HERE, base::BindOnce(&Backend::LoadNELPoliciesAndNotifyInBackground,
                                this, std::move(loaded_callback)));
}

void SQLitePersistentReportingAndNELStore::Backend::AddNELPolicy(
    const NetworkErrorLoggingService::NELPolicy& policy) {
  auto po = std::make_unique<PendingOperation<NELPolicyInfo>>(
      PendingOperation<NELPolicyInfo>::Type::ADD, NELPolicyInfo(policy));
  BatchNELPolicyOperation(policy.origin, std::move(po));
}

void SQLitePersistentReportingAndNELStore::Backend::UpdateNELPolicyAccessTime(
    const NetworkErrorLoggingService::NELPolicy& policy) {
  auto po = std::make_unique<PendingOperation<NELPolicyInfo>>(
      PendingOperation<NELPolicyInfo>::Type::UPDATE_ACCESS_TIME,
      NELPolicyInfo(policy));
  BatchNELPolicyOperation(policy.origin, std::move(po));
}

void SQLitePersistentReportingAndNELStore::Backend::DeleteNELPolicy(
    const NetworkErrorLoggingService::NELPolicy& policy) {
  auto po = std::make_unique<PendingOperation<NELPolicyInfo>>(
      PendingOperation<NELPolicyInfo>::Type::DELETE, NELPolicyInfo(policy));
  BatchNELPolicyOperation(policy.origin, std::move(po));
}

size_t SQLitePersistentReportingAndNELStore::Backend::GetQueueLengthForTesting()
    const {
  size_t count = 0;
  {
    base::AutoLock locked(lock_);
    for (auto& origin_and_pending_ops : nel_policy_pending_ops_) {
      count += origin_and_pending_ops.second.size();
    }
  }
  return count;
}

bool SQLitePersistentReportingAndNELStore::Backend::CreateDatabaseSchema() {
  if (!db()->DoesTableExist("nel_policies") &&
      !CreateV1NELPoliciesSchema(db())) {
    return false;
  }

  // TODO(chlily): Initialize tables for Reporting clients, endpoint groups,
  // endpoints, and reports.

  return true;
}

base::Optional<int>
SQLitePersistentReportingAndNELStore::Backend::DoMigrateDatabaseSchema() {
  int cur_version = meta_table()->GetVersionNumber();
  if (cur_version != 1)
    return base::nullopt;

  // Future database upgrade statements go here.

  return base::make_optional(cur_version);
}

void SQLitePersistentReportingAndNELStore::Backend::DoCommit() {
  NELPolicyPendingOperationsMap nel_policy_ops;
  {
    base::AutoLock locked(lock_);
    nel_policy_pending_ops_.swap(nel_policy_ops);
    // TODO(chlily): swap out pending operations queues for Reporting data.
    num_pending_ = 0;
  }
  size_t op_count = nel_policy_ops.size();
  if (!db() || op_count == 0)
    return;

  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return;

  for (const auto& origin_and_nel_policy_ops : nel_policy_ops) {
    const PendingOperationsVector<NELPolicyInfo>& ops_for_origin =
        origin_and_nel_policy_ops.second;
    for (const std::unique_ptr<PendingOperation<NELPolicyInfo>>& nel_policy_op :
         ops_for_origin) {
      CommitNELPolicyOperation(nel_policy_op.get());
    }
  }
  // TODO(chlily): Commit operations pertaining to Reporting.

  transaction.Commit();
}

void SQLitePersistentReportingAndNELStore::Backend::CommitNELPolicyOperation(
    PendingOperation<NELPolicyInfo>* op) {
  DCHECK_EQ(1, db()->transaction_nesting());

  sql::Statement add_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO nel_policies (origin_scheme, origin_host, origin_port, "
      "received_ip_address, report_to, expires_us_since_epoch, "
      "success_fraction, failure_fraction, is_include_subdomains, "
      "last_access_us_since_epoch) VALUES (?,?,?,?,?,?,?,?,?,?)"));
  if (!add_smt.is_valid())
    return;

  sql::Statement update_access_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE nel_policies SET last_access_us_since_epoch=? WHERE "
      "origin_scheme=? AND origin_host=? AND origin_port=?"));
  if (!update_access_smt.is_valid())
    return;

  sql::Statement del_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM nel_policies WHERE "
      "origin_scheme=? AND origin_host=? AND origin_port=?"));
  if (!del_smt.is_valid())
    return;

  const NELPolicyInfo& nel_policy_info = op->data();

  switch (op->type()) {
    case PendingOperation<NELPolicyInfo>::Type::ADD:
      add_smt.Reset(true);
      add_smt.BindString(0, nel_policy_info.origin_scheme);
      add_smt.BindString(1, nel_policy_info.origin_host);
      add_smt.BindInt(2, nel_policy_info.origin_port);
      add_smt.BindString(3, nel_policy_info.received_ip_address);
      add_smt.BindString(4, nel_policy_info.report_to);
      add_smt.BindInt64(5, nel_policy_info.expires_us_since_epoch);
      add_smt.BindDouble(6, nel_policy_info.success_fraction);
      add_smt.BindDouble(7, nel_policy_info.failure_fraction);
      add_smt.BindBool(8, nel_policy_info.is_include_subdomains);
      add_smt.BindInt64(9, nel_policy_info.last_access_us_since_epoch);
      if (!add_smt.Run())
        DLOG(WARNING) << "Could not add a NEL policy to the DB.";
      break;

    case PendingOperation<NELPolicyInfo>::Type::UPDATE_ACCESS_TIME:
      update_access_smt.Reset(true);
      update_access_smt.BindInt64(0,
                                  nel_policy_info.last_access_us_since_epoch);
      update_access_smt.BindString(1, nel_policy_info.origin_scheme);
      update_access_smt.BindString(2, nel_policy_info.origin_host);
      update_access_smt.BindInt(3, nel_policy_info.origin_port);
      if (!update_access_smt.Run()) {
        DLOG(WARNING)
            << "Could not update NEL policy last access time in the DB.";
      }
      break;

    case PendingOperation<NELPolicyInfo>::Type::DELETE:
      del_smt.Reset(true);
      del_smt.BindString(0, nel_policy_info.origin_scheme);
      del_smt.BindString(1, nel_policy_info.origin_host);
      del_smt.BindInt(2, nel_policy_info.origin_port);
      if (!del_smt.Run())
        DLOG(WARNING) << "Could not delete a NEL policy from the DB.";
      break;

    default:
      NOTREACHED();
      break;
  }
}

void SQLitePersistentReportingAndNELStore::Backend::BatchNELPolicyOperation(
    const url::Origin& origin,
    std::unique_ptr<PendingOperation<NELPolicyInfo>> po) {
  DCHECK(!background_task_runner()->RunsTasksInCurrentSequence());

  size_t num_pending;
  {
    base::AutoLock locked(lock_);

    std::pair<NELPolicyPendingOperationsMap::iterator, bool> iter_and_result =
        nel_policy_pending_ops_.insert(
            std::make_pair(origin, PendingOperationsVector<NELPolicyInfo>()));
    PendingOperationsVector<NELPolicyInfo>* ops_for_origin =
        &iter_and_result.first->second;
    // If the insert failed, then we already have NEL policy operations for this
    // origin, so we try to coalesce the new operation with the existing ones.
    if (!iter_and_result.second)
      MaybeCoalesceOperations(ops_for_origin, po.get());
    ops_for_origin->push_back(std::move(po));
    // Note that num_pending_ counts number of calls to Batch*Operation(), not
    // the current length of the queue; this is intentional to guarantee
    // progress, as the length of the queue may decrease in some cases.
    num_pending = ++num_pending_;
  }

  OnOperationBatched(num_pending);
}

template <typename DataType>
void SQLitePersistentReportingAndNELStore::Backend::MaybeCoalesceOperations(
    PendingOperationsVector<DataType>* ops_for_key,
    PendingOperation<DataType>* new_op) {
  DCHECK(!ops_for_key->empty());
  if (new_op->type() == PendingOperation<DataType>::Type::DELETE) {
    // A delete makes all previous operations irrelevant.
    ops_for_key->clear();
  } else if (new_op->type() ==
             PendingOperation<DataType>::Type::UPDATE_ACCESS_TIME) {
    if (ops_for_key->back()->type() ==
        PendingOperation<DataType>::Type::UPDATE_ACCESS_TIME) {
      // Updating the access time twice in a row is equivalent to just the
      // latter update.
      ops_for_key->pop_back();
      // At most a delete and add before.
      DCHECK_LE(ops_for_key->size(), 2u);
    }
  } else {
    // Nothing special is done for an add operation. If it is overwriting an
    // existing entry, it will be preceded by at most one delete.
    DCHECK_LE(ops_for_key->size(), 1u);
  }
}

void SQLitePersistentReportingAndNELStore::Backend::OnOperationBatched(
    size_t num_pending) {
  DCHECK(!background_task_runner()->RunsTasksInCurrentSequence());
  // Commit every 30 seconds.
  static const int kCommitIntervalMs = 30 * 1000;
  // Commit right away if we have more than 512 outstanding operations.
  static const size_t kCommitAfterBatchSize = 512;

  if (num_pending == 1) {
    // We've gotten our first entry for this batch, fire off the timer.
    if (!background_task_runner()->PostDelayedTask(
            FROM_HERE, base::BindOnce(&Backend::Commit, this),
            base::TimeDelta::FromMilliseconds(kCommitIntervalMs))) {
      NOTREACHED() << "background_task_runner_ is not running.";
    }
  } else if (num_pending >= kCommitAfterBatchSize) {
    // We've reached a big enough batch, fire off a commit now.
    PostBackgroundTask(FROM_HERE, base::BindOnce(&Backend::Commit, this));
  }
}

// TODO(chlily): Discard expired policies when loading, discard and record
// problem if loaded policy is malformed.
void SQLitePersistentReportingAndNELStore::Backend::
    LoadNELPoliciesAndNotifyInBackground(
        NELPoliciesLoadedCallback loaded_callback) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());

  std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies;
  if (!InitializeDatabase()) {
    PostClientTask(
        FROM_HERE,
        base::BindOnce(&Backend::CompleteLoadNELPoliciesAndNotifyInForeground,
                       this, std::move(loaded_callback),
                       std::move(loaded_policies), false /* load_success */));
    return;
  }

  sql::Statement smt(db()->GetUniqueStatement(
      "SELECT origin_scheme, origin_host, origin_port, received_ip_address,"
      "report_to, expires_us_since_epoch, success_fraction, failure_fraction,"
      "is_include_subdomains, last_access_us_since_epoch FROM nel_policies"));
  if (!smt.is_valid()) {
    Reset();
    PostClientTask(
        FROM_HERE,
        base::BindOnce(&Backend::CompleteLoadNELPoliciesAndNotifyInForeground,
                       this, std::move(loaded_callback),
                       std::move(loaded_policies), false /* load_success */));
    return;
  }

  while (smt.Step()) {
    // Reconstitute a NEL policy from the fields stored in the database.
    NetworkErrorLoggingService::NELPolicy policy;
    policy.origin = url::Origin::CreateFromNormalizedTuple(
        /* origin_scheme = */ smt.ColumnString(0),
        /* origin_host = */ smt.ColumnString(1),
        /* origin_port = */ smt.ColumnInt(2));
    if (!policy.received_ip_address.AssignFromIPLiteral(smt.ColumnString(3)))
      policy.received_ip_address = IPAddress();
    policy.report_to = smt.ColumnString(4);
    policy.expires = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(smt.ColumnInt64(5)));
    policy.success_fraction = smt.ColumnDouble(6);
    policy.failure_fraction = smt.ColumnDouble(7);
    policy.include_subdomains = smt.ColumnBool(8);
    policy.last_used = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(smt.ColumnInt64(9)));

    loaded_policies.push_back(std::move(policy));
  }

  PostClientTask(
      FROM_HERE,
      base::BindOnce(&Backend::CompleteLoadNELPoliciesAndNotifyInForeground,
                     this, std::move(loaded_callback),
                     std::move(loaded_policies), true /* load_success */));
}

void SQLitePersistentReportingAndNELStore::Backend::
    CompleteLoadNELPoliciesAndNotifyInForeground(
        NELPoliciesLoadedCallback loaded_callback,
        std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies,
        bool load_success) {
  DCHECK(client_task_runner()->RunsTasksInCurrentSequence());

  if (load_success) {
    // TODO(chlily): report metrics
  } else {
    DCHECK(loaded_policies.empty());
  }

  std::move(loaded_callback).Run(std::move(loaded_policies));
}

SQLitePersistentReportingAndNELStore::SQLitePersistentReportingAndNELStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : backend_(new Backend(path, client_task_runner, background_task_runner)),
      weak_factory_(this) {}

SQLitePersistentReportingAndNELStore::~SQLitePersistentReportingAndNELStore() {
  backend_->Close();
}

void SQLitePersistentReportingAndNELStore::LoadNELPolicies(
    NELPoliciesLoadedCallback loaded_callback) {
  DCHECK(!loaded_callback.is_null());
  backend_->LoadNELPolicies(base::BindOnce(
      &SQLitePersistentReportingAndNELStore::CompleteLoadNELPolicies,
      weak_factory_.GetWeakPtr(), std::move(loaded_callback)));
}

void SQLitePersistentReportingAndNELStore::AddNELPolicy(
    const NetworkErrorLoggingService::NELPolicy& policy) {
  backend_->AddNELPolicy(policy);
}

void SQLitePersistentReportingAndNELStore::UpdateNELPolicyAccessTime(
    const NetworkErrorLoggingService::NELPolicy& policy) {
  backend_->UpdateNELPolicyAccessTime(policy);
}

void SQLitePersistentReportingAndNELStore::DeleteNELPolicy(
    const NetworkErrorLoggingService::NELPolicy& policy) {
  backend_->DeleteNELPolicy(policy);
}

void SQLitePersistentReportingAndNELStore::Flush() {
  backend_->Flush(base::DoNothing());
}

size_t SQLitePersistentReportingAndNELStore::GetQueueLengthForTesting() const {
  return backend_->GetQueueLengthForTesting();
}

void SQLitePersistentReportingAndNELStore::CompleteLoadNELPolicies(
    NELPoliciesLoadedCallback callback,
    std::vector<NetworkErrorLoggingService::NELPolicy> policies) {
  std::move(callback).Run(std::move(policies));
}

}  // namespace net
