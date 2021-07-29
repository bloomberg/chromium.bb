// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_database.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "storage/browser/quota/quota_database_migrations.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/gurl.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {
namespace {

// Version number of the database schema.
//
// We support migrating the database schema from versions that are at most 2
// years old. Older versions are unsupported, and will cause the database to get
// razed.
//
// Version 1 - 2011-03-17 - http://crrev.com/78521 (unsupported)
// Version 2 - 2011-04-25 - http://crrev.com/82847 (unsupported)
// Version 3 - 2011-07-08 - http://crrev.com/91835 (unsupported)
// Version 4 - 2011-10-17 - http://crrev.com/105822 (unsupported)
// Version 5 - 2015-10-19 - https://crrev.com/354932
// Version 6 - 2021-04-27 - https://crrev.com/c/2757450
// Version 7 - 2021-05-20 - https://crrev.com/c/2910136
const int kQuotaDatabaseCurrentSchemaVersion = 7;
const int kQuotaDatabaseCompatibleVersion = 7;

// Definitions for database schema.
const char kHostQuotaTable[] = "quota";
const char kBucketTable[] = "buckets";
const char kIsOriginTableBootstrapped[] = "IsOriginTableBootstrapped";

const int kCommitIntervalMs = 30000;

}  // anonymous namespace

// static
const char QuotaDatabase::kDefaultBucketName[] = "default";

const QuotaDatabase::TableSchema QuotaDatabase::kTables[] = {
    {kHostQuotaTable,
     "(host TEXT NOT NULL,"
     " type INTEGER NOT NULL,"
     " quota INTEGER NOT NULL,"
     " PRIMARY KEY(host, type))"
     " WITHOUT ROWID"},
    {kBucketTable,
     "(id INTEGER PRIMARY KEY,"
     // TODO(crbug.com/1215208): Rename column to storage_key and create a DB
     // migration for it.
     " origin TEXT NOT NULL,"
     " type INTEGER NOT NULL,"
     " name TEXT NOT NULL,"
     " use_count INTEGER NOT NULL,"
     " last_accessed INTEGER NOT NULL,"
     " last_modified INTEGER NOT NULL,"
     " expiration INTEGER NOT NULL,"
     " quota INTEGER NOT NULL)"}};
const size_t QuotaDatabase::kTableCount = base::size(QuotaDatabase::kTables);

// static
const QuotaDatabase::IndexSchema QuotaDatabase::kIndexes[] = {
    {"buckets_by_storage_key", kBucketTable, "(origin, type, name)", true},
    {"buckets_by_last_accessed", kBucketTable, "(type, last_accessed)", false},
    {"buckets_by_last_modified", kBucketTable, "(type, last_modified)", false},
    {"buckets_by_expiration", kBucketTable, "(expiration)", false},
};
const size_t QuotaDatabase::kIndexCount = base::size(QuotaDatabase::kIndexes);

QuotaDatabase::BucketTableEntry::BucketTableEntry() = default;

QuotaDatabase::BucketTableEntry::~BucketTableEntry() = default;

QuotaDatabase::BucketTableEntry::BucketTableEntry(const BucketTableEntry&) =
    default;
QuotaDatabase::BucketTableEntry& QuotaDatabase::BucketTableEntry::operator=(
    const QuotaDatabase::BucketTableEntry&) = default;

QuotaDatabase::BucketTableEntry::BucketTableEntry(
    BucketId bucket_id,
    StorageKey storage_key,
    StorageType type,
    std::string name,
    int use_count,
    const base::Time& last_accessed,
    const base::Time& last_modified)
    : bucket_id(std::move(bucket_id)),
      storage_key(std::move(storage_key)),
      type(type),
      name(std::move(name)),
      use_count(use_count),
      last_accessed(last_accessed),
      last_modified(last_modified) {}

// QuotaDatabase ------------------------------------------------------------
QuotaDatabase::QuotaDatabase(const base::FilePath& path)
    : db_file_path_(path),
      is_recreating_(false),
      is_disabled_(false) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

QuotaDatabase::~QuotaDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_) {
    db_->CommitTransaction();
  }
}

bool QuotaDatabase::GetHostQuota(const std::string& host,
                                 StorageType type,
                                 int64_t* quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(quota);
  if (LazyOpen(LazyOpenMode::kFailIfNotFound) != QuotaError::kNone)
    return false;

  static constexpr char kSql[] =
      "SELECT quota FROM quota WHERE host = ? AND type = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, host);
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Step())
    return false;

  *quota = statement.ColumnInt64(0);
  return true;
}

bool QuotaDatabase::SetHostQuota(const std::string& host,
                                 StorageType type,
                                 int64_t quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(quota, 0);
  if (LazyOpen(LazyOpenMode::kCreateIfNotFound) != QuotaError::kNone)
    return false;

  if (quota == 0)
    return DeleteHostQuota(host, type);
  if (!InsertOrReplaceHostQuota(host, type, quota))
    return false;
  ScheduleCommit();
  return true;
}

QuotaErrorOr<BucketInfo> QuotaDatabase::GetOrCreateBucket(
    const StorageKey& storage_key,
    const std::string& bucket_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  QuotaErrorOr<BucketInfo> bucket_result =
      GetBucket(storage_key, bucket_name, StorageType::kTemporary);

  if (bucket_result.ok())
    return bucket_result;

  if (bucket_result.error() != QuotaError::kNotFound)
    return bucket_result.error();

  base::Time now = base::Time::Now();
  return CreateBucketInternal(storage_key, StorageType::kTemporary, bucket_name,
                              /*use_count=*/0, now, now);
}

QuotaErrorOr<BucketInfo> QuotaDatabase::CreateBucketForTesting(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType storage_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug/1210252): Update to not execute 2 sql statements on creation.
  QuotaErrorOr<BucketInfo> bucket_result =
      GetBucket(storage_key, bucket_name, storage_type);

  if (bucket_result.ok())
    return QuotaError::kEntryExistsError;

  if (bucket_result.error() != QuotaError::kNotFound)
    return bucket_result.error();

  base::Time now = base::Time::Now();
  return CreateBucketInternal(storage_key, storage_type, bucket_name,
                              /*use_count=*/0, now, now);
}

QuotaErrorOr<BucketInfo> QuotaDatabase::GetBucket(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType storage_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = LazyOpen(LazyOpenMode::kFailIfNotFound);
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT id, expiration, quota "
        "FROM buckets "
        "WHERE origin = ? AND type = ? AND name = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindInt(1, static_cast<int>(storage_type));
  statement.BindString(2, bucket_name);

  if (!statement.Step()) {
    return statement.Succeeded() ? QuotaError::kNotFound
                                 : QuotaError::kDatabaseError;
  }

  return BucketInfo(BucketId(statement.ColumnInt64(0)), storage_key,
                    storage_type, bucket_name, statement.ColumnTime(1),
                    statement.ColumnInt(2));
}

bool QuotaDatabase::SetStorageKeyLastAccessTime(const StorageKey& storage_key,
                                                StorageType type,
                                                base::Time last_accessed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (LazyOpen(LazyOpenMode::kCreateIfNotFound) != QuotaError::kNone)
    return false;

  BucketTableEntry entry;
  if (!GetStorageKeyInfo(storage_key, type, &entry)) {
    QuotaErrorOr<BucketInfo> result =
        CreateBucketInternal(storage_key, type, kDefaultBucketName,
                             /*use_count=*/1, last_accessed, last_accessed);
    return result.ok();
  }

  ++entry.use_count;
  static constexpr char kSql[] =
      // clang-format off
      "UPDATE buckets "
        "SET use_count = ?, last_accessed = ? "
        "WHERE origin = ? AND type = ? AND name = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, entry.use_count);
  statement.BindTime(1, last_accessed);
  statement.BindString(2, storage_key.Serialize());
  statement.BindInt(3, static_cast<int>(type));
  statement.BindString(4, kDefaultBucketName);

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::SetBucketLastAccessTime(const BucketId bucket_id,
                                            base::Time last_accessed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bucket_id.is_null());
  if (LazyOpen(LazyOpenMode::kCreateIfNotFound) != QuotaError::kNone)
    return false;

  BucketTableEntry entry;
  if (!GetBucketInfo(bucket_id, &entry))
    return false;

  ++entry.use_count;
  static constexpr char kSql[] =
      "UPDATE buckets SET use_count = ?, last_accessed = ? WHERE id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, entry.use_count);
  statement.BindTime(1, last_accessed);
  statement.BindInt64(2, bucket_id.value());

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::SetStorageKeyLastModifiedTime(const StorageKey& storage_key,
                                                  StorageType type,
                                                  base::Time last_modified) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (LazyOpen(LazyOpenMode::kCreateIfNotFound) != QuotaError::kNone)
    return false;

  BucketTableEntry entry;
  if (!GetStorageKeyInfo(storage_key, type, &entry)) {
    QuotaErrorOr<BucketInfo> result =
        CreateBucketInternal(storage_key, type, kDefaultBucketName,
                             /*use_count=*/0, last_modified, last_modified);
    return result.ok();
  }

  static constexpr char kSql[] =
      // clang-format off
      "UPDATE buckets "
        "SET last_modified = ? "
        "WHERE origin = ? AND type = ? AND name = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindTime(0, last_modified);
  statement.BindString(1, storage_key.Serialize());
  statement.BindInt(2, static_cast<int>(type));
  statement.BindString(3, kDefaultBucketName);

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::SetBucketLastModifiedTime(const BucketId bucket_id,
                                              base::Time last_modified) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bucket_id.is_null());
  if (LazyOpen(LazyOpenMode::kCreateIfNotFound) != QuotaError::kNone)
    return false;

  BucketTableEntry entry;
  if (!GetBucketInfo(bucket_id, &entry))
    return false;

  static constexpr char kSql[] =
      "UPDATE buckets SET last_modified = ? WHERE id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindTime(0, last_modified);
  statement.BindInt64(1, bucket_id.value());

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::RegisterInitialStorageKeyInfo(
    const std::set<StorageKey>& storage_keys,
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (LazyOpen(LazyOpenMode::kCreateIfNotFound) != QuotaError::kNone)
    return false;

  for (const auto& storage_key : storage_keys) {
    static constexpr char kSql[] =
        // clang-format off
        "INSERT OR IGNORE INTO buckets("
            "origin,"
            "type,"
            "name,"
            "use_count,"
            "last_accessed,"
            "last_modified,"
            "expiration,"
            "quota) "
          "VALUES (?, ?, ?, 0, 0, 0, ?, 0)";
    // clang-format on
    sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindString(0, storage_key.Serialize());
    statement.BindInt(1, static_cast<int>(type));
    statement.BindString(2, kDefaultBucketName);
    statement.BindTime(3, base::Time::Max());

    if (!statement.Run())
      return false;
  }

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::GetStorageKeyInfo(const StorageKey& storage_key,
                                      StorageType type,
                                      QuotaDatabase::BucketTableEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (LazyOpen(LazyOpenMode::kFailIfNotFound) != QuotaError::kNone)
    return false;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT "
          "id,"
          "use_count,"
          "last_accessed,"
          "last_modified "
        "FROM buckets "
        "WHERE origin = ? AND type = ? AND name = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindInt(1, static_cast<int>(type));
  statement.BindString(2, kDefaultBucketName);

  if (!statement.Step())
    return false;

  *entry = BucketTableEntry(BucketId(statement.ColumnInt64(0)), storage_key,
                            type, kDefaultBucketName, statement.ColumnInt(1),
                            statement.ColumnTime(2), statement.ColumnTime(3));
  return true;
}

bool QuotaDatabase::GetBucketInfo(const BucketId bucket_id,
                                  QuotaDatabase::BucketTableEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bucket_id.is_null());
  if (LazyOpen(LazyOpenMode::kFailIfNotFound) != QuotaError::kNone)
    return false;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT "
          "origin,"
          "type,"
          "name,"
          "use_count,"
          "last_accessed,"
          "last_modified "
        "FROM buckets "
        "WHERE id = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, bucket_id.value());

  if (!statement.Step())
    return false;

  absl::optional<StorageKey> storage_key =
      StorageKey::Deserialize(statement.ColumnString(0));
  if (!storage_key.has_value())
    return false;

  *entry = BucketTableEntry(bucket_id, std::move(storage_key).value(),
                            static_cast<StorageType>(statement.ColumnInt(1)),
                            statement.ColumnString(2), statement.ColumnInt(3),
                            statement.ColumnTime(4), statement.ColumnTime(5));
  return true;
}

bool QuotaDatabase::DeleteHostQuota(
    const std::string& host, StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (LazyOpen(LazyOpenMode::kFailIfNotFound) != QuotaError::kNone)
    return false;

  static constexpr char kSql[] =
      "DELETE FROM quota WHERE host = ? AND type = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, host);
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::DeleteStorageKeyInfo(const StorageKey& storage_key,
                                         StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (LazyOpen(LazyOpenMode::kFailIfNotFound) != QuotaError::kNone)
    return false;

  static constexpr char kSql[] =
      "DELETE FROM buckets WHERE origin = ? AND type = ? AND name = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindInt(1, static_cast<int>(type));
  statement.BindString(2, kDefaultBucketName);

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::DeleteBucketInfo(const BucketId bucket_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bucket_id.is_null());
  if (LazyOpen(LazyOpenMode::kFailIfNotFound) != QuotaError::kNone)
    return false;

  static constexpr char kSql[] = "DELETE FROM buckets WHERE id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, bucket_id.value());

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::GetLRUStorageKey(
    StorageType type,
    const std::set<StorageKey>& exceptions,
    SpecialStoragePolicy* special_storage_policy,
    absl::optional<StorageKey>* storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(storage_key);
  if (LazyOpen(LazyOpenMode::kFailIfNotFound) != QuotaError::kNone)
    return false;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT origin FROM buckets "
        "WHERE type = ? AND name = ? "
        "ORDER BY last_accessed";
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));
  statement.BindString(1, kDefaultBucketName);

  while (statement.Step()) {
    absl::optional<StorageKey> read_storage_key =
        StorageKey::Deserialize(statement.ColumnString(0));
    if (!read_storage_key.has_value())
      continue;

    if (base::Contains(exceptions, *read_storage_key))
      continue;

    GURL read_gurl = read_storage_key->origin().GetURL();
    if (special_storage_policy &&
        (special_storage_policy->IsStorageDurable(read_gurl) ||
         special_storage_policy->IsStorageUnlimited(read_gurl))) {
      continue;
    }

    *storage_key = std::move(read_storage_key).value();
    return true;
  }

  storage_key->reset();
  return statement.Succeeded();
}

bool QuotaDatabase::GetLRUBucket(StorageType type,
                                 const std::set<StorageKey>& exceptions,
                                 SpecialStoragePolicy* special_storage_policy,
                                 absl::optional<BucketId>* bucket_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(bucket_id);
  if (LazyOpen(LazyOpenMode::kFailIfNotFound) != QuotaError::kNone)
    return false;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT id, origin FROM buckets "
        "WHERE type = ? "
        "ORDER BY last_accessed";
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));

  while (statement.Step()) {
    int64_t read_bucket_id = statement.ColumnInt64(0);
    absl::optional<StorageKey> read_storage_key =
        StorageKey::Deserialize(statement.ColumnString(1));
    if (!read_storage_key.has_value())
      continue;

    if (base::Contains(exceptions, *read_storage_key))
      continue;

    // TODO(crbug/1176774): Once BucketTable holds bucket durability info,
    // add logic to allow durable buckets to also bypass eviction.
    GURL read_gurl = read_storage_key->origin().GetURL();
    if (special_storage_policy &&
        (special_storage_policy->IsStorageDurable(read_gurl) ||
         special_storage_policy->IsStorageUnlimited(read_gurl))) {
      continue;
    }

    *bucket_id = BucketId(read_bucket_id);
    return true;
  }

  bucket_id->reset();
  return statement.Succeeded();
}

bool QuotaDatabase::GetStorageKeysModifiedBetween(
    StorageType type,
    std::set<StorageKey>* storage_keys,
    base::Time begin,
    base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(storage_keys);
  if (LazyOpen(LazyOpenMode::kFailIfNotFound) != QuotaError::kNone)
    return false;

  DCHECK(!begin.is_max());
  DCHECK(end != base::Time());
  static constexpr char kSql[] =
      // clang-format off
      "SELECT origin FROM buckets "
        "WHERE type = ? AND name = ?"
        "AND last_modified >= ? AND last_modified < ?";
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));
  statement.BindString(1, kDefaultBucketName);
  statement.BindTime(2, begin);
  statement.BindTime(3, end);

  storage_keys->clear();
  while (statement.Step()) {
    absl::optional<StorageKey> read_storage_key =
        StorageKey::Deserialize(statement.ColumnString(0));
    if (!read_storage_key.has_value())
      continue;
    storage_keys->insert(std::move(read_storage_key).value());
  }

  return statement.Succeeded();
}

bool QuotaDatabase::GetBucketsModifiedBetween(StorageType type,
                                              std::set<BucketId>* bucket_ids,
                                              base::Time begin,
                                              base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(bucket_ids);
  if (LazyOpen(LazyOpenMode::kFailIfNotFound) != QuotaError::kNone)
    return false;

  DCHECK(!begin.is_max());
  DCHECK(end != base::Time());
  static constexpr char kSql[] =
      // clang-format off
      "SELECT id FROM buckets "
        "WHERE type = ? AND last_modified >= ? AND last_modified < ?";
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));
  statement.BindTime(1, begin);
  statement.BindTime(2, end);

  bucket_ids->clear();
  while (statement.Step())
    bucket_ids->insert(BucketId(statement.ColumnInt64(0)));

  return statement.Succeeded();
}

bool QuotaDatabase::IsStorageKeyDatabaseBootstrapped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (LazyOpen(LazyOpenMode::kCreateIfNotFound) != QuotaError::kNone)
    return false;

  int flag = 0;
  return meta_table_->GetValue(kIsOriginTableBootstrapped, &flag) && flag;
}

bool QuotaDatabase::SetStorageKeyDatabaseBootstrapped(bool bootstrap_flag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (LazyOpen(LazyOpenMode::kCreateIfNotFound) != QuotaError::kNone)
    return false;

  return meta_table_->SetValue(kIsOriginTableBootstrapped, bootstrap_flag);
}

void QuotaDatabase::Commit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return;

  if (timer_.IsRunning())
    timer_.Stop();

  DCHECK_EQ(1, db_->transaction_nesting());
  db_->CommitTransaction();
  DCHECK_EQ(0, db_->transaction_nesting());
  db_->BeginTransaction();
  DCHECK_EQ(1, db_->transaction_nesting());
}

void QuotaDatabase::ScheduleCommit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timer_.IsRunning())
    return;
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kCommitIntervalMs),
               this, &QuotaDatabase::Commit);
}

QuotaError QuotaDatabase::LazyOpen(LazyOpenMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_)
    return QuotaError::kNone;

  // If we tried and failed once, don't try again in the same session
  // to avoid creating an incoherent mess on disk.
  if (is_disabled_)
    return QuotaError::kDatabaseError;

  bool in_memory_only = db_file_path_.empty();
  if (mode == LazyOpenMode::kFailIfNotFound &&
      (in_memory_only || !base::PathExists(db_file_path_))) {
    return QuotaError::kNotFound;
  }

  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{
      .exclusive_locking = true,
      .page_size = 4096,
      .cache_size = 500,
  });
  meta_table_ = std::make_unique<sql::MetaTable>();

  db_->set_histogram_tag("Quota");

  bool opened = false;
  if (in_memory_only) {
    opened = db_->OpenInMemory();
  } else if (!base::CreateDirectory(db_file_path_.DirName())) {
      LOG(ERROR) << "Failed to create quota database directory.";
  } else {
    opened = db_->Open(db_file_path_);
    if (opened)
      db_->Preload();
  }

  if (!opened || !EnsureDatabaseVersion()) {
    LOG(ERROR) << "Could not open the quota database, resetting.";
    if (!ResetSchema()) {
      LOG(ERROR) << "Failed to reset the quota database.";
      is_disabled_ = true;
      db_.reset();
      meta_table_.reset();
      return QuotaError::kDatabaseError;
    }
  }

  // Start a long-running transaction.
  db_->BeginTransaction();

  return QuotaError::kNone;
}

bool QuotaDatabase::EnsureDatabaseVersion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sql::MetaTable::DoesTableExist(db_.get()))
    return CreateSchema();

  if (!meta_table_->Init(db_.get(), kQuotaDatabaseCurrentSchemaVersion,
                         kQuotaDatabaseCompatibleVersion))
    return false;

  if (meta_table_->GetCompatibleVersionNumber() >
      kQuotaDatabaseCurrentSchemaVersion) {
    LOG(WARNING) << "Quota database is too new.";
    return false;
  }

  if (meta_table_->GetVersionNumber() < kQuotaDatabaseCurrentSchemaVersion) {
    if (!QuotaDatabaseMigrations::UpgradeSchema(*this))
      return ResetSchema();
  }

#if DCHECK_IS_ON()
  DCHECK(sql::MetaTable::DoesTableExist(db_.get()));
  for (const TableSchema& table : kTables)
    DCHECK(db_->DoesTableExist(table.table_name));
#endif

  return true;
}

bool QuotaDatabase::CreateSchema() {
  // TODO(kinuko): Factor out the common code to create databases.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  if (!meta_table_->Init(db_.get(), kQuotaDatabaseCurrentSchemaVersion,
                         kQuotaDatabaseCompatibleVersion)) {
    return false;
  }

  for (const TableSchema& table : kTables) {
    if (!CreateTable(table))
      return false;
  }

  for (const IndexSchema& index : kIndexes) {
    if (!CreateIndex(index))
      return false;
  }

  return transaction.Commit();
}

bool QuotaDatabase::CreateTable(const TableSchema& table) {
  std::string sql("CREATE TABLE ");
  sql += table.table_name;
  sql += table.columns;
  if (!db_->Execute(sql.c_str())) {
    VLOG(1) << "Failed to execute " << sql;
    return false;
  }
  return true;
}

bool QuotaDatabase::CreateIndex(const IndexSchema& index) {
  std::string sql;
  if (index.unique)
    sql += "CREATE UNIQUE INDEX ";
  else
    sql += "CREATE INDEX ";
  sql += index.index_name;
  sql += " ON ";
  sql += index.table_name;
  sql += index.columns;
  if (!db_->Execute(sql.c_str())) {
    VLOG(1) << "Failed to execute " << sql;
    return false;
  }
  return true;
}

bool QuotaDatabase::ResetSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!db_file_path_.empty());
  DCHECK(base::PathExists(db_file_path_));
  DCHECK(!db_ || !db_->transaction_nesting());
  VLOG(1) << "Deleting existing quota data and starting over.";

  db_.reset();
  meta_table_.reset();

  if (!sql::Database::Delete(db_file_path_))
    return false;

  // So we can't go recursive.
  if (is_recreating_)
    return false;

  base::AutoReset<bool> auto_reset(&is_recreating_, true);
  return LazyOpen(LazyOpenMode::kCreateIfNotFound) == QuotaError::kNone;
}

bool QuotaDatabase::InsertOrReplaceHostQuota(const std::string& host,
                                             StorageType type,
                                             int64_t quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_.get());
  static constexpr char kSql[] =
      // clang-format off
      "INSERT OR REPLACE INTO quota(quota, host, type)"
        "VALUES (?, ?, ?)";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, quota);
  statement.BindString(1, host);
  statement.BindInt(2, static_cast<int>(type));
  return statement.Run();
}

bool QuotaDatabase::DumpQuotaTable(const QuotaTableCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (LazyOpen(LazyOpenMode::kCreateIfNotFound) != QuotaError::kNone)
    return false;

  static constexpr char kSql[] = "SELECT * FROM quota";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));

  while (statement.Step()) {
    QuotaTableEntry entry = {
        .host = statement.ColumnString(0),
        .type = static_cast<StorageType>(statement.ColumnInt(1)),
        .quota = statement.ColumnInt64(2)};

    if (!callback.Run(entry))
      return true;
  }

  return statement.Succeeded();
}

bool QuotaDatabase::DumpBucketTable(const BucketTableCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyOpen(LazyOpenMode::kCreateIfNotFound) != QuotaError::kNone)
    return false;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT "
          "id,"
          "origin,"
          "type,"
          "name,"
          "use_count,"
          "last_accessed,"
          "last_modified "
        "FROM buckets";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));

  while (statement.Step()) {
    BucketId bucket_id = BucketId(statement.ColumnInt64(0));
    absl::optional<StorageKey> storage_key =
        StorageKey::Deserialize(statement.ColumnString(1));
    if (!storage_key.has_value())
      continue;

    BucketTableEntry entry(std::move(bucket_id), std::move(storage_key).value(),
                           static_cast<StorageType>(statement.ColumnInt(2)),
                           statement.ColumnString(3), statement.ColumnInt(4),
                           statement.ColumnTime(5), statement.ColumnTime(6));

    if (!callback.Run(entry))
      return true;
  }

  return statement.Succeeded();
}

QuotaErrorOr<BucketInfo> QuotaDatabase::CreateBucketInternal(
    const blink::StorageKey& storage_key,
    StorageType type,
    const std::string& bucket_name,
    int use_count,
    base::Time last_accessed,
    base::Time last_modified) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug/1210259): Add DCHECKs for input validation.
  QuotaError open_error = LazyOpen(LazyOpenMode::kCreateIfNotFound);
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "INSERT INTO buckets("
        "origin,"
        "type,"
        "name,"
        "use_count,"
        "last_accessed,"
        "last_modified,"
        "expiration,"
        "quota) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, 0)";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindInt(1, static_cast<int>(type));
  statement.BindString(2, bucket_name);
  statement.BindInt(3, use_count);
  statement.BindTime(4, last_accessed);
  statement.BindTime(5, last_modified);
  statement.BindTime(6, base::Time::Max());

  if (!statement.Run())
    return QuotaError::kDatabaseError;

  ScheduleCommit();

  int64_t bucket_id = db_->GetLastInsertRowId();
  DCHECK_GT(bucket_id, 0);
  return BucketInfo(BucketId(bucket_id), storage_key, type, bucket_name,
                    base::Time::Max(), 0);
}

bool operator<(const QuotaDatabase::QuotaTableEntry& lhs,
               const QuotaDatabase::QuotaTableEntry& rhs) {
  return std::tie(lhs.host, lhs.type, lhs.quota) <
         std::tie(rhs.host, rhs.type, rhs.quota);
}

bool operator<(const QuotaDatabase::BucketTableEntry& lhs,
               const QuotaDatabase::BucketTableEntry& rhs) {
  return std::tie(lhs.storage_key, lhs.type, lhs.use_count, lhs.last_accessed) <
         std::tie(rhs.storage_key, rhs.type, rhs.use_count, rhs.last_accessed);
}

}  // namespace storage
