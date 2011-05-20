// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/quota_database.h"

#include <string>

#include "app/sql/connection.h"
#include "app/sql/diagnostic_error_delegate.h"
#include "app/sql/meta_table.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/auto_reset.h"
#include "base/file_util.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

namespace {

// Definitions for database schema.

const int kCurrentVersion = 2;
const int kCompatibleVersion = 2;

const char kOriginsTable[] = "Origins";
const char kHostQuotaTable[] = "HostQuotaTable";
const char kOriginLastAccessTable[] = "OriginLastAccessTable";
const char kGlobalQuotaKeyPrefix[] = "GlobalQuota-";
const char kIsOriginTableBootstrapped[] = "IsOriginTableBootstrapped";

const struct {
  const char* table_name;
  const char* columns;
} kTables[] = {
  { kHostQuotaTable,
    "(host TEXT NOT NULL,"
    " type INTEGER NOT NULL,"
    " quota INTEGER,"
    " UNIQUE(host, type))" },
  { kOriginLastAccessTable,
    "(origin TEXT NOT NULL,"
    " type INTEGER NOT NULL,"
    " used_count INTEGER,"
    " last_access_time INTEGER,"
    " UNIQUE(origin, type))" },
};

const struct {
  const char* index_name;
  const char* table_name;
  const char* columns;
  bool unique;
} kIndexes[] = {
  { "HostIndex",
    kHostQuotaTable,
    "(host)",
    false },
  { "OriginLastAccessIndex",
    kOriginLastAccessTable,
    "(origin, last_access_time)",
    false },
};

const int kTableCount = ARRAYSIZE_UNSAFE(kTables);
const int kIndexCount = ARRAYSIZE_UNSAFE(kIndexes);

class HistogramUniquifier {
 public:
  static const char* name() { return "Sqlite.Quota.Error"; }
};

bool PrepareCachedStatement(
    sql::Connection* db, const sql::StatementID& id,
    const char* sql, sql::Statement* statement) {
  DCHECK(db && sql && statement);
  statement->Assign(db->GetCachedStatement(id, sql));
  if (!statement->is_valid()) {
    NOTREACHED() << db->GetErrorMessage();
    return false;
  }
  return true;
}

std::string GetGlobalQuotaKey(quota::StorageType type) {
  if (type == quota::kStorageTypeTemporary)
    return std::string(kGlobalQuotaKeyPrefix) + "temporary";
  else if (type == quota::kStorageTypePersistent)
    return std::string(kGlobalQuotaKeyPrefix) + "persistent";
  NOTREACHED() << "Unknown storage type " << type;
  return std::string();
}

const int kCommitIntervalMs = 30000;

}  // anonymous namespace

namespace quota {

QuotaDatabase::QuotaDatabase(const FilePath& path)
    : db_file_path_(path),
      is_recreating_(false),
      is_disabled_(false) {
}

QuotaDatabase::~QuotaDatabase() {
  if (db_.get()) {
    db_->CommitTransaction();
  }
}

void QuotaDatabase::CloseConnection() {
  meta_table_.reset();
  db_.reset();
}

bool QuotaDatabase::GetHostQuota(
    const std::string& host, StorageType type, int64* quota) {
  DCHECK(quota);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT quota"
      " FROM HostQuotaTable"
      " WHERE host = ? AND type = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindString(0, host);
  statement.BindInt(1, static_cast<int>(type));
  if (!statement.Step() || !statement.Succeeded())
    return false;

  *quota = statement.ColumnInt64(0);
  return true;
}

bool QuotaDatabase::SetHostQuota(
    const std::string& host, StorageType type, int64 quota) {
  DCHECK_GE(quota, 0);
  if (!LazyOpen(true))
    return false;

  sql::Statement statement;

  int64 dummy;
  if (GetHostQuota(host, type, &dummy)) {
    const char* kSql =
        "UPDATE HostQuotaTable"
        " SET quota = ?"
        " WHERE host = ? AND type = ?";
    if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
      return false;
  } else {
    const char* kSql =
        "INSERT INTO HostQuotaTable"
        " (quota, host, type)"
        " VALUES (?, ?, ?)";
    if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
      return false;
  }

  statement.BindInt64(0, quota);
  statement.BindString(1, host);
  statement.BindInt(2, static_cast<int>(type));
  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::SetOriginLastAccessTime(
    const GURL& origin, StorageType type, base::Time last_access_time) {
  if (!LazyOpen(true))
    return false;

  sql::Statement statement;

  int used_count = 0;
  if (FindOriginUsedCount(origin, type, &used_count)) {
    ++used_count;
    const char* kSql =
        "UPDATE OriginLastAccessTable"
        " SET used_count = ?, last_access_time = ?"
        " WHERE origin = ? AND type = ?";
    if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
      return false;
  } else  {
    const char* kSql =
        "INSERT INTO OriginLastAccessTable"
        " (used_count, last_access_time, origin, type)"
        " VALUES (?, ?, ?, ?)";
    if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
      return false;
  }

  statement.BindInt(0, used_count);
  statement.BindInt64(1, last_access_time.ToInternalValue());
  statement.BindString(2, origin.spec());
  statement.BindInt(3, static_cast<int>(type));
  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::RegisterOrigins(const std::set<GURL>& origins,
                                    StorageType type,
                                    base::Time last_access_time) {
  if (!LazyOpen(true))
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  sql::Statement statement;

  typedef std::set<GURL>::const_iterator itr_type;
  for (itr_type itr = origins.begin(), end = origins.end();
       itr != end; ++itr) {
    const char* kSql =
        "INSERT OR IGNORE INTO OriginLastAccessTable"
        " (used_count, last_access_time, origin, type)"
        " VALUES (?, ?, ?, ?)";
    if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
      return false;

    statement.BindInt(0, 0);  // used_count
    statement.BindInt64(1, last_access_time.ToInternalValue());
    statement.BindString(2, itr->spec());
    statement.BindInt(3, static_cast<int>(type));
    if (!statement.Run())
      return false;
  }

  return transaction.Commit();
}

bool QuotaDatabase::DeleteHostQuota(
    const std::string& host, StorageType type) {
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "DELETE FROM HostQuotaTable"
      " WHERE host = ? AND type = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindString(0, host);
  statement.BindInt(1, static_cast<int>(type));
  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::DeleteOriginLastAccessTime(
    const GURL& origin, StorageType type) {
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "DELETE FROM OriginLastAccessTable"
      " WHERE origin = ? AND type = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindString(0, origin.spec());
  statement.BindInt(1, static_cast<int>(type));
  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::GetGlobalQuota(StorageType type, int64* quota) {
  if (!LazyOpen(false))
    return false;
  return meta_table_->GetValue(GetGlobalQuotaKey(type).c_str(), quota);
}

bool QuotaDatabase::SetGlobalQuota(StorageType type, int64 quota) {
  if (!LazyOpen(true))
    return false;
  return meta_table_->SetValue(GetGlobalQuotaKey(type).c_str(), quota);
}

bool QuotaDatabase::GetLRUOrigin(
    StorageType type,
    const std::set<GURL>& exceptions,
    GURL* origin) {
  DCHECK(origin);
  if (!LazyOpen(false))
    return false;

  const char* kSql = "SELECT origin FROM OriginLastAccessTable"
                     " WHERE type = ?"
                     " ORDER BY last_access_time ASC";

  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
    return false;
  statement.BindInt(0, static_cast<int>(type));

  while (statement.Step()) {
    GURL url(statement.ColumnString(0));
    if (exceptions.find(url) == exceptions.end()) {
      *origin = url;
      return true;
    }
  }

  *origin = GURL();
  return statement.Succeeded();
}

bool QuotaDatabase::IsOriginDatabaseBootstrapped() {
  if (!LazyOpen(true))
    return false;

  int flag = 0;
  return meta_table_->GetValue(kIsOriginTableBootstrapped, &flag) && flag;
}

bool QuotaDatabase::SetOriginDatabaseBootstrapped(bool bootstrap_flag) {
  if (!LazyOpen(true))
    return false;

  return meta_table_->SetValue(kIsOriginTableBootstrapped, bootstrap_flag);
}

void QuotaDatabase::Commit() {
  if (!db_.get())
    return;

  // Note: for now this will be called only by ScheduleCommit, but when it
  // becomes untrue we should call timer_.Stop() here.
  DCHECK(!timer_.IsRunning());

  db_->CommitTransaction();
  db_->BeginTransaction();
}

void QuotaDatabase::ScheduleCommit() {
  if (timer_.IsRunning())
    return;
  timer_.Start(base::TimeDelta::FromMilliseconds(kCommitIntervalMs), this,
               &QuotaDatabase::Commit);
}

bool QuotaDatabase::FindOriginUsedCount(
    const GURL& origin, StorageType type, int* used_count) {
  DCHECK(used_count);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT used_count FROM OriginLastAccessTable"
      " WHERE origin = ? AND type = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindString(0, origin.spec());
  statement.BindInt(1, static_cast<int>(type));
  if (!statement.Step() || !statement.Succeeded())
    return false;

  *used_count = statement.ColumnInt(0);
  return true;
}

bool QuotaDatabase::LazyOpen(bool create_if_needed) {
  if (db_.get())
    return true;

  // If we tried and failed once, don't try again in the same session
  // to avoid creating an incoherent mess on disk.
  if (is_disabled_)
    return false;

  bool in_memory_only = db_file_path_.empty();
  if (!create_if_needed &&
      (in_memory_only || !file_util::PathExists(db_file_path_))) {
    return false;
  }

  db_.reset(new sql::Connection);
  meta_table_.reset(new sql::MetaTable);

  bool opened = false;
  if (in_memory_only) {
    opened = db_->OpenInMemory();
  } else if (!file_util::CreateDirectory(db_file_path_.DirName())) {
      LOG(ERROR) << "Failed to create quota database directory.";
  } else {
    opened = db_->Open(db_file_path_);
    if (opened)
      db_->Preload();
  }

  if (!opened || !EnsureDatabaseVersion()) {
    LOG(ERROR) << "Failed to open the quota database.";
    is_disabled_ = true;
    db_.reset();
    meta_table_.reset();
    return false;
  }

  // Start a long-running transaction.
  db_->BeginTransaction();

  return true;
}

bool QuotaDatabase::EnsureDatabaseVersion() {
  if (!sql::MetaTable::DoesTableExist(db_.get()))
    return CreateSchema();

  if (!meta_table_->Init(db_.get(), kCurrentVersion, kCompatibleVersion))
    return false;

  if (meta_table_->GetCompatibleVersionNumber() > kCurrentVersion) {
    LOG(WARNING) << "Quota database is too new.";
    return false;
  }

  if (meta_table_->GetVersionNumber() < kCurrentVersion) {
    // TODO(kinuko): Support schema upgrade.
    return ResetSchema();
  }

#ifndef NDEBUG
  DCHECK(sql::MetaTable::DoesTableExist(db_.get()));
  for (int i = 0; i < kTableCount; ++i) {
    DCHECK(db_->DoesTableExist(kTables[i].table_name));
  }
#endif

  return true;
}

bool QuotaDatabase::CreateSchema() {
  // TODO(kinuko): Factor out the common code to create databases.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  if (!meta_table_->Init(db_.get(), kCurrentVersion, kCompatibleVersion))
    return false;

  for (int i = 0; i < kTableCount; ++i) {
    std::string sql("CREATE TABLE ");
    sql += kTables[i].table_name;
    sql += kTables[i].columns;
    if (!db_->Execute(sql.c_str())) {
      VLOG(1) << "Failed to execute " << sql;
      return false;
    }
  }

  for (int i = 0; i < kIndexCount; ++i) {
    std::string sql;
    if (kIndexes[i].unique)
      sql += "CREATE UNIQUE INDEX ";
    else
      sql += "CREATE INDEX ";
    sql += kIndexes[i].index_name;
    sql += " ON ";
    sql += kIndexes[i].table_name;
    sql += kIndexes[i].columns;
    if (!db_->Execute(sql.c_str())) {
      VLOG(1) << "Failed to execute " << sql;
      return false;
    }
  }

  return transaction.Commit();
}

bool QuotaDatabase::ResetSchema() {
  DCHECK(!db_file_path_.empty());
  DCHECK(file_util::PathExists(db_file_path_));
  VLOG(1) << "Deleting existing quota data and starting over.";

  db_.reset();
  meta_table_.reset();

  if (!file_util::Delete(db_file_path_, true))
    return false;

  // Make sure the steps above actually deleted things.
  if (file_util::PathExists(db_file_path_))
    return false;

  // So we can't go recursive.
  if (is_recreating_)
    return false;

  AutoReset<bool> auto_reset(&is_recreating_, true);
  return LazyOpen(true);
}

}  // quota namespace
