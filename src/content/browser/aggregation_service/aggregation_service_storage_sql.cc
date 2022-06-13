// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/public_key.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("AggregationService");

// Version number of the database.
//
// Version 1 - 2021/07/16 - https://crrev.com/c/3038364
constexpr int kCurrentVersionNumber = 1;

// Earliest version which can use a `kCurrentVersionNumber` database
// without failing.
constexpr int kCompatibleVersionNumber = 1;

// Latest version of the database that cannot be upgraded to
// `kCurrentVersionNumber` without razing the database. No versions are
// currently deprecated.
constexpr int kDeprecatedVersionNumber = 0;

constexpr size_t kDeleteStepSize = 100;

bool UpgradeAggregationServiceStorageSqlSchema(sql::Database& db,
                                               sql::MetaTable& meta_table) {
  // Placeholder for database migration logic.
  NOTREACHED();

  return true;
}

void RecordInitializationStatus(
    const AggregationServiceStorageSql::InitStatus status) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.AggregationService.Storage.Sql.InitStatus", status);
}

}  // namespace

AggregationServiceStorageSql::AggregationServiceStorageSql(
    bool run_in_memory,
    const base::FilePath& path_to_database,
    const base::Clock* clock)
    : run_in_memory_(run_in_memory),
      path_to_database_(run_in_memory_
                            ? base::FilePath()
                            : path_to_database.Append(kDatabasePath)),
      clock_(*clock),
      db_(sql::DatabaseOptions{.exclusive_locking = true,
                               .page_size = 4096,
                               .cache_size = 32}) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(clock);

  db_.set_histogram_tag("AggregationService");

  // base::Unretained is safe here because the callback will only be called
  // while the sql::Database in `db_` is alive, and this instance owns `db_`.
  db_.set_error_callback(
      base::BindRepeating(&AggregationServiceStorageSql::DatabaseErrorCallback,
                          base::Unretained(this)));
}

AggregationServiceStorageSql::~AggregationServiceStorageSql() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::vector<PublicKey> AggregationServiceStorageSql::GetPublicKeys(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network::IsOriginPotentiallyTrustworthy(origin));

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return {};

  static constexpr char kGetOriginIdSql[] =
      "SELECT origin_id FROM origins WHERE origin = ? AND expiry_time > ?";
  sql::Statement get_origin_id_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetOriginIdSql));
  get_origin_id_statement.BindString(0, origin.Serialize());
  get_origin_id_statement.BindTime(1, clock_.Now());
  if (!get_origin_id_statement.Step())
    return {};

  int64_t origin_id = get_origin_id_statement.ColumnInt64(0);

  static constexpr char kGetKeysSql[] =
      "SELECT key_id, key FROM keys WHERE origin_id = ? ORDER BY key_id";

  sql::Statement get_keys_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetKeysSql));
  get_keys_statement.BindInt64(0, origin_id);

  // Partial results are not returned in case of any error.
  std::vector<PublicKey> result;
  while (get_keys_statement.Step()) {
    if (result.size() >= PublicKeyset::kMaxNumberKeys)
      return {};

    std::string id = get_keys_statement.ColumnString(0);

    std::vector<uint8_t> key;
    get_keys_statement.ColumnBlobAsVector(1, &key);

    if (id.size() > PublicKey::kMaxIdSize ||
        key.size() != PublicKey::kKeyByteLength) {
      return {};
    }

    result.emplace_back(std::move(id), std::move(key));
  }

  if (!get_keys_statement.Succeeded())
    return {};

  return result;
}

void AggregationServiceStorageSql::SetPublicKeys(const url::Origin& origin,
                                                 const PublicKeyset& keyset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network::IsOriginPotentiallyTrustworthy(origin));
  DCHECK_LE(keyset.keys.size(), PublicKeyset::kMaxNumberKeys);

  // TODO(crbug.com/1231703): Add an allowlist for helper server origins and
  // validate the origin.

  // Force the creation of the database if it doesn't exist, as we need to
  // persist the public keys.
  if (!EnsureDatabaseOpen(DbCreationPolicy::kCreateIfAbsent))
    return;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  // Replace the public keys for the origin. Deleting the existing rows and
  // inserting new ones to reduce the complexity.
  if (!ClearPublicKeysImpl(origin))
    return;

  if (!InsertPublicKeysImpl(origin, keyset))
    return;

  transaction.Commit();
}

void AggregationServiceStorageSql::ClearPublicKeys(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network::IsOriginPotentiallyTrustworthy(origin));

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  ClearPublicKeysImpl(origin);

  transaction.Commit();
}

void AggregationServiceStorageSql::ClearPublicKeysFetchedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return;

  // Treat null times as unbounded lower or upper range. This is used by
  // browsing data remover.
  if (delete_begin.is_null())
    delete_begin = base::Time::Min();

  if (delete_end.is_null())
    delete_end = base::Time::Max();

  if (delete_begin.is_min() && delete_end.is_max()) {
    ClearAllPublicKeys();
    return;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  static constexpr char kSelectOriginRangeSql[] =
      "SELECT origin_id FROM origins WHERE fetch_time BETWEEN ? AND ? "
      "    ORDER by fetch_time, origin_id";
  sql::Statement select_origins_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectOriginRangeSql));
  select_origins_statement.BindTime(0, delete_begin);
  select_origins_statement.BindTime(1, delete_end);

  while (true) {  // Delete at most kDeleteStepSize origins at a time.
    std::vector<int64_t> origin_ids_to_delete;
    while (origin_ids_to_delete.size() < kDeleteStepSize &&
           select_origins_statement.Step()) {
      int64_t origin_id = select_origins_statement.ColumnInt64(0);
      origin_ids_to_delete.push_back(origin_id);
    }

    // Don't proceed to delete partial results if one of the statements fails.
    if (!select_origins_statement.Succeeded())
      return;

    ClearPublicKeysByOriginIds(origin_ids_to_delete);

    if (origin_ids_to_delete.size() < kDeleteStepSize)
      break;

    select_origins_statement.Reset(/*clear_bound_vars = */ false);
  }

  transaction.Commit();
}

void AggregationServiceStorageSql::ClearPublicKeysExpiredBy(
    base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!delete_end.is_null());

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  static constexpr char kSelectOriginRangeSql[] =
      "SELECT origin_id FROM origins WHERE expiry_time <= ? "
      "    ORDER BY expiry_time, origin_id";
  sql::Statement select_origins_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectOriginRangeSql));
  select_origins_statement.BindTime(0, delete_end);

  while (true) {  // Delete at most kDeleteStepSize origins at a time.
    std::vector<int64_t> origin_ids_to_delete;
    while (origin_ids_to_delete.size() < kDeleteStepSize &&
           select_origins_statement.Step()) {
      int64_t origin_id = select_origins_statement.ColumnInt64(0);
      origin_ids_to_delete.push_back(origin_id);
    }

    // Don't proceed to delete partial results if one of the statements fails.
    if (!select_origins_statement.Succeeded())
      return;

    ClearPublicKeysByOriginIds(origin_ids_to_delete);

    if (origin_ids_to_delete.size() < kDeleteStepSize)
      break;

    select_origins_statement.Reset(/*clear_bound_vars=*/false);
  }

  transaction.Commit();
}

bool AggregationServiceStorageSql::InsertPublicKeysImpl(
    const url::Origin& origin,
    const PublicKeyset& keyset) {
  DCHECK(!keyset.fetch_time.is_null());
  DCHECK(!keyset.expiry_time.is_null());
  DCHECK(db_.HasActiveTransactions());

  static constexpr char kInsertOriginSql[] =
      "INSERT INTO origins(origin, fetch_time, expiry_time) VALUES (?,?,?)";

  sql::Statement insert_origin_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertOriginSql));
  insert_origin_statement.BindString(0, origin.Serialize());
  insert_origin_statement.BindTime(1, keyset.fetch_time);
  insert_origin_statement.BindTime(2, keyset.expiry_time);

  if (!insert_origin_statement.Run())
    return false;

  int64_t origin_id = db_.GetLastInsertRowId();

  static constexpr char kInsertKeySql[] =
      "INSERT INTO keys(origin_id, key_id, key) VALUES (?,?,?)";
  sql::Statement insert_key_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertKeySql));

  for (const PublicKey& key : keyset.keys) {
    DCHECK_LE(key.id.size(), PublicKey::kMaxIdSize);
    DCHECK_EQ(key.key.size(), PublicKey::kKeyByteLength);

    insert_key_statement.Reset(/*clear_bound_vars=*/true);
    insert_key_statement.BindInt64(0, origin_id);
    insert_key_statement.BindString(1, key.id);
    insert_key_statement.BindBlob(2, key.key);

    if (!insert_key_statement.Run())
      return false;
  }

  return true;
}

bool AggregationServiceStorageSql::ClearPublicKeysImpl(
    const url::Origin& origin) {
  DCHECK(db_.HasActiveTransactions());

  static constexpr char kSelectOriginSql[] =
      "SELECT origin_id FROM origins WHERE origin = ? ORDER BY origin_id";
  sql::Statement select_origin_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectOriginSql));
  select_origin_statement.BindString(0, origin.Serialize());

  bool has_matched_origin = select_origin_statement.Step();

  if (!select_origin_statement.Succeeded())
    return false;

  if (!has_matched_origin)
    return true;

  int64_t origin_id = select_origin_statement.ColumnInt64(0);
  return ClearPublicKeysByOriginIds({origin_id});
}

bool AggregationServiceStorageSql::ClearPublicKeysByOriginIds(
    const std::vector<int64_t>& origin_ids) {
  DCHECK(db_.HasActiveTransactions());

  static constexpr char kDeleteOriginSql[] =
      "DELETE FROM origins WHERE origin_id = ?";
  sql::Statement delete_origin_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteOriginSql));

  for (int64_t origin_id : origin_ids) {
    delete_origin_statement.Reset(/*clear_bound_vars=*/true);
    delete_origin_statement.BindInt64(0, origin_id);
    if (!delete_origin_statement.Run())
      return false;
  }

  static constexpr char kDeleteKeysSql[] =
      "DELETE FROM keys WHERE origin_id = ?";
  sql::Statement delete_keys_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteKeysSql));

  for (int64_t origin_id : origin_ids) {
    delete_keys_statement.Reset(/*clear_bound_vars=*/true);
    delete_keys_statement.BindInt64(0, origin_id);
    if (!delete_keys_statement.Run())
      return false;
  }

  return true;
}

void AggregationServiceStorageSql::ClearAllPublicKeys() {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  static constexpr char kDeleteAllOriginsSql[] = "DELETE FROM origins";
  sql::Statement delete_all_origins_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteAllOriginsSql));
  if (!delete_all_origins_statement.Run())
    return;

  static constexpr char kDeleteAllKeysSql[] = "DELETE FROM keys";
  sql::Statement delete_all_keys_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteAllKeysSql));
  if (!delete_all_keys_statement.Run())
    return;

  transaction.Commit();
}

void AggregationServiceStorageSql::HandleInitializationFailure(
    const InitStatus status) {
  RecordInitializationStatus(status);
  db_init_status_ = DbStatus::kClosed;
}

bool AggregationServiceStorageSql::EnsureDatabaseOpen(
    DbCreationPolicy creation_policy) {
  if (!db_init_status_) {
    if (run_in_memory_) {
      db_init_status_ = DbStatus::kDeferringCreation;
    } else {
      db_init_status_ = base::PathExists(path_to_database_)
                            ? DbStatus::kDeferringOpen
                            : DbStatus::kDeferringCreation;
    }
  }

  switch (*db_init_status_) {
    // If the database file has not been created, we defer creation until
    // storage needs to be used for an operation which needs to operate even on
    // an empty database.
    case DbStatus::kDeferringCreation:
      if (creation_policy == DbCreationPolicy::kFailIfAbsent)
        return false;
      break;
    case DbStatus::kDeferringOpen:
      break;
    case DbStatus::kClosed:
      return false;
    case DbStatus::kOpen:
      return true;
  }

  if (run_in_memory_) {
    if (!db_.OpenInMemory()) {
      HandleInitializationFailure(InitStatus::kFailedToOpenDbInMemory);
      return false;
    }
  } else {
    const base::FilePath& dir = path_to_database_.DirName();
    const bool dir_exists_or_was_created =
        base::DirectoryExists(dir) || base::CreateDirectory(dir);
    if (!dir_exists_or_was_created) {
      DLOG(ERROR)
          << "Failed to create directory for AggregationService database";
      HandleInitializationFailure(InitStatus::kFailedToCreateDir);
      return false;
    }
    if (!db_.Open(path_to_database_)) {
      HandleInitializationFailure(InitStatus::kFailedToOpenDbFile);
      return false;
    }
  }

  if (!InitializeSchema(db_init_status_ == DbStatus::kDeferringCreation)) {
    HandleInitializationFailure(InitStatus::kFailedToInitializeSchema);
    return false;
  }

  db_init_status_ = DbStatus::kOpen;
  RecordInitializationStatus(InitStatus::kSuccess);
  return true;
}

bool AggregationServiceStorageSql::InitializeSchema(bool db_empty) {
  if (db_empty)
    return CreateSchema();

  if (!meta_table_.Init(&db_, kCurrentVersionNumber, kCompatibleVersionNumber))
    return false;

  int current_version = meta_table_.GetVersionNumber();
  if (current_version == kCurrentVersionNumber)
    return true;

  if (current_version <= kDeprecatedVersionNumber) {
    // Note that this also razes the meta table, so it will need to be
    // initialized again.
    db_.Raze();
    return CreateSchema();
  }

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    // In this case the database version is too new to be used. The DB will
    // never work until Chrome is re-upgraded. Assume the user will continue
    // using this Chrome version and raze the DB to get aggregation service
    // storage working.
    db_.Raze();
    return CreateSchema();
  }

  return UpgradeAggregationServiceStorageSqlSchema(db_, meta_table_);
}

bool AggregationServiceStorageSql::CreateSchema() {
  // All of the columns in this table are designed to be "const".
  // `origin` is the origin of the helper server.
  // `fetch_time` is when the key is fetched and inserted into database, and
  // will be used for data deletion.
  // `expiry_time` is when the key becomes invalid and will be used for data
  // pruning.
  static constexpr char kOriginsTableSql[] =
      "CREATE TABLE IF NOT EXISTS origins("
      "    origin_id INTEGER PRIMARY KEY NOT NULL,"
      "    origin TEXT NOT NULL,"
      "    fetch_time INTEGER NOT NULL,"
      "    expiry_time INTEGER NOT NULL)";
  if (!db_.Execute(kOriginsTableSql))
    return false;

  static constexpr char kOriginsByOriginIndexSql[] =
      "CREATE UNIQUE INDEX IF NOT EXISTS origins_by_origin_idx "
      "    ON origins(origin)";
  if (!db_.Execute(kOriginsByOriginIndexSql))
    return false;

  // Will be used to optimize key lookup by fetch time for data clearing (see
  // crbug.com/1231689).
  static constexpr char kFetchTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS fetch_time_idx ON origins(fetch_time)";
  if (!db_.Execute(kFetchTimeIndexSql))
    return false;

  // Will be used to optimize key lookup by expiry time for data pruning (see
  // crbug.com/1231696).
  static constexpr char kExpiryTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS expiry_time_idx ON origins(expiry_time)";
  if (!db_.Execute(kExpiryTimeIndexSql))
    return false;

  // All of the columns in this table are designed to be "const".
  // `origin_id` is the primary key of a row in the `origins` table.
  // `key_id` is an arbitrary string identifying the key which is set by helper
  // servers and not required to be unique, but is required to be unique per
  // origin.
  // `key` is the public key as a sequence of bytes.
  static constexpr char kKeysTableSql[] =
      "CREATE TABLE IF NOT EXISTS keys("
      "    origin_id INTEGER NOT NULL,"
      "    key_id TEXT NOT NULL,"
      "    key BLOB NOT NULL,"
      "    PRIMARY KEY(origin_id, key_id)) WITHOUT ROWID";
  if (!db_.Execute(kKeysTableSql))
    return false;

  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    return false;
  }

  return true;
}

void AggregationServiceStorageSql::DatabaseErrorCallback(int extended_error,
                                                         sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error) &&
      !ignore_errors_for_testing_)
    DLOG(FATAL) << db_.GetErrorMessage();

  // Consider the database closed to avoid further errors.
  db_init_status_ = DbStatus::kClosed;
}

}  // namespace content
