// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/quota_database.h"

#include "app/sql/connection.h"
#include "app/sql/diagnostic_error_delegate.h"
#include "app/sql/meta_table.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/auto_reset.h"
#include "base/file_util.h"
#include "googleurl/src/gurl.h"

namespace {

const int64 kUninitializedId = -1;
const int64 kUninitializedQuota = -1;

// Definitions for database schema.

const int kCurrentVersion = 1;
const int kCompatibleVersion = 1;

const char kOriginsTable[] = "Origins";
const char kStorageInfoTable[] = "StorageInfo";
const char kGlobalQuotaKeyPrefix[] = "GlobalQuota-";

const struct {
  const char* table_name;
  const char* columns;
} kTables[] = {
  { kOriginsTable,
    "(origin_url TEXT NOT NULL)" },
  { kStorageInfoTable,
    "(origin_rowid INTEGER,"
    " type INTEGER NOT NULL,"
    " quota INTEGER,"
    " used_count INTEGER,"
    " last_access_time INTEGER)" },
};

const struct {
  const char* index_name;
  const char* table_name;
  const char* columns;
  bool unique;
} kIndexes[] = {
  { "OriginsIndex",
    kOriginsTable,
    "(origin_url)",
    true },
  { "StorageInfoIndex",
    kStorageInfoTable,
    "(origin_rowid)",
    false },
};

const int kTableCount = ARRAYSIZE_UNSAFE(kTables);
const int kIndexCount = ARRAYSIZE_UNSAFE(kIndexes);

class HistogramUniquifier {
 public:
  static const char* name() { return "Sqlite.Quota.Error"; }
};

sql::ErrorDelegate* GetErrorHandlerForQuotaManagerDb() {
  return new sql::DiagnosticErrorDelegate<HistogramUniquifier>();
}

bool PrepareUniqueStatement(
    sql::Connection* db, const char* sql, sql::Statement* statement) {
  DCHECK(db && sql && statement);
  statement->Assign(db->GetUniqueStatement(sql));
  if (!statement->is_valid()) {
    NOTREACHED() << db->GetErrorMessage();
    return false;
  }
  return true;
}

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

}  // anonymous namespace

namespace quota {

struct QuotaDatabase::StorageInfoRecord {
  StorageInfoRecord()
      : rowid(kUninitializedId),
        origin_rowid(kUninitializedId),
        type(kStorageTypeUnknown),
        quota(kUninitializedQuota),
        used_count(0) {}
  int64 rowid;
  int64 origin_rowid;
  StorageType type;
  int64 quota;
  int used_count;
  base::Time last_access_time;
};

QuotaDatabase::QuotaDatabase(const FilePath& path)
    : db_file_path_(path),
      is_recreating_(false),
      is_disabled_(false) {
}

QuotaDatabase::~QuotaDatabase() {
}

void QuotaDatabase::CloseConnection() {
  meta_table_.reset();
  db_.reset();
}

bool QuotaDatabase::GetOriginQuota(
    const GURL& origin, StorageType type, int64* quota) {
  DCHECK(quota);
  StorageInfoRecord record;
  if (!FindStorageInfo(origin, type, &record))
    return false;
  if (record.quota == kUninitializedQuota)
    return false;
  *quota = record.quota;
  return true;
}

bool QuotaDatabase::SetOriginQuota(
    const GURL& origin, StorageType type, int64 quota) {
  DCHECK(quota >= 0);
  if (!LazyOpen(true))
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  int64 origin_rowid;
  if (!FindOrigin(origin, &origin_rowid)) {
    if (!InsertOrigin(origin, &origin_rowid))
      return false;
  }

  StorageInfoRecord record;
  if (!FindStorageInfo(origin_rowid, type, &record)) {
    record.origin_rowid = origin_rowid;
    record.type = type;
    record.quota = quota;
    if (!InsertStorageInfo(record))
      return false;
    return transaction.Commit();
  }

  const char* kSql =
      "UPDATE StorageInfo"
      " SET quota = ?"
      " WHERE rowid = ?";
  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, quota);
  statement.BindInt64(1, record.rowid);
  if (!statement.Run())
    return false;
  return transaction.Commit();
}

bool QuotaDatabase::SetOriginLastAccessTime(
    const GURL& origin, StorageType type, base::Time last_access_time) {
  if (!LazyOpen(true))
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  int64 origin_rowid;
  if (!FindOrigin(origin, &origin_rowid)) {
    if (!InsertOrigin(origin, &origin_rowid))
      return false;
  }
  StorageInfoRecord record;
  if (!FindStorageInfo(origin_rowid, type, &record)) {
    record.origin_rowid = origin_rowid;
    record.type = type;
    record.used_count = 0;
    record.last_access_time = last_access_time;
    if (!InsertStorageInfo(record))
      return false;
    return transaction.Commit();
  }

  const char* kSql =
      "UPDATE StorageInfo"
      " SET used_count = ?, last_access_time = ?"
      " WHERE rowid = ?";
  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt(0, record.used_count + 1);
  statement.BindInt64(1, last_access_time.ToInternalValue());
  statement.BindInt64(2, record.rowid);
  if (!statement.Run())
    return false;

  return transaction.Commit();
}

bool QuotaDatabase::DeleteStorageInfo(const GURL& origin, StorageType type) {
  if (!LazyOpen(false))
    return false;

  int64 origin_rowid;
  if (!FindOrigin(origin, &origin_rowid))
    return false;

  StorageInfoRecord record;
  if (!FindStorageInfo(origin_rowid, type, &record))
    return false;

  const char* kSql =
      "DELETE FROM StorageInfo"
      " WHERE rowid = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, record.rowid);
  return statement.Run();
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

bool QuotaDatabase::GetLRUOrigins(
    StorageType type, std::vector<GURL>* origins,
    int max_used_count, int num_origins_limit) {
  DCHECK(origins);
  DCHECK(num_origins_limit > 0);
  if (!LazyOpen(false))
    return false;

  const char* kSqlBase =
      "SELECT o.origin_url FROM Origins o, StorageInfo s"
      " WHERE o.rowid = s.origin_rowid AND"
      "       s.type = ?";

  // TODO(kinuko): depending on how we call this method, consider creating
  // an index to avoid frequent full table scan.
  const char* kSqlSuffix =
      " ORDER BY s.last_access_time ASC "
      " LIMIT ?";

  std::string sql(kSqlBase);
  sql::StatementID id = SQL_FROM_HERE;
  if (max_used_count >= 0) {
    sql += " AND s.used_count <= ?";
    id = SQL_FROM_HERE;
  }
  sql += kSqlSuffix;

  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), id, sql.c_str(), &statement))
    return false;

  int column = 0;
  statement.BindInt(column++, static_cast<int>(type));
  if (max_used_count >= 0)
    statement.BindInt(column++, max_used_count);
  statement.BindInt(column++, num_origins_limit);

  origins->clear();
  while (statement.Step())
    origins->push_back(GURL(statement.ColumnString(0)));

  DCHECK(origins->size() <= static_cast<size_t>(num_origins_limit));

  return statement.Succeeded();
}

bool QuotaDatabase::FindOrigin(const GURL& origin_url, int64* origin_rowid) {
  DCHECK(origin_rowid);
  if (!LazyOpen(false))
    return false;

  const char* kSql = "SELECT rowid FROM Origins WHERE origin_url = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindString(0, origin_url.spec());
  if (!statement.Step() || !statement.Succeeded())
    return false;

  *origin_rowid = statement.ColumnInt64(0);
  return true;
}

bool QuotaDatabase::InsertOrigin(const GURL& origin_url, int64 *origin_rowid) {
  DCHECK(origin_rowid);
  if (!LazyOpen(true))
    return false;

  const char* kSql =
      "INSERT INTO Origins (origin_url) VALUES(?)";

  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
    return false;


  statement.BindString(0, origin_url.spec());
  if (!statement.Run())
    return false;

  *origin_rowid = db_->GetLastInsertRowId();
  return true;
}

bool QuotaDatabase::FindStorageInfo(int64 origin_rowid, StorageType type,
                                    StorageInfoRecord* record) {
  DCHECK(record);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT rowid, origin_rowid, quota, used_count, last_access_time"
      " FROM StorageInfo"
      " WHERE origin_rowid = ? AND type = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, origin_rowid);
  statement.BindInt(1, static_cast<int>(type));
  if (!statement.Step() || !statement.Succeeded())
    return false;

  record->rowid = statement.ColumnInt64(0);
  record->origin_rowid = statement.ColumnInt64(1);
  record->quota = statement.ColumnInt64(2);
  record->used_count = statement.ColumnInt(3);
  record->last_access_time = base::Time::FromInternalValue(
      statement.ColumnInt64(4));

  return true;
}

bool QuotaDatabase::FindStorageInfo(const GURL& origin, StorageType type,
                                    StorageInfoRecord* record) {
  DCHECK(record);
  if (!LazyOpen(false))
    return false;

  int64 origin_rowid;
  if (!FindOrigin(origin, &origin_rowid))
    return false;

  return FindStorageInfo(origin_rowid, type, record);
}

bool QuotaDatabase::InsertStorageInfo(const StorageInfoRecord& record) {
  if (!LazyOpen(true))
    return false;

  DCHECK(record.type == kStorageTypeTemporary ||
         record.type == kStorageTypePersistent);

  const char* kSql =
      "INSERT INTO StorageInfo"
      " (origin_rowid, type, quota, used_count, last_access_time)"
      " VALUES(?, ?, ?, ?, ?)";

  sql::Statement statement;
  if (!PrepareCachedStatement(db_.get(), SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, record.origin_rowid);
  statement.BindInt(1, static_cast<int>(record.type));
  statement.BindInt64(2, record.quota);
  statement.BindInt(3, record.used_count);
  statement.BindInt64(4, record.last_access_time.ToInternalValue());
  return statement.Run();
}

bool QuotaDatabase::LazyOpen(bool create_if_needed) {
  if (db_.get())
    return true;

  // If we tried and failed once, don't try again in the same session
  // to avoid creating an incoherent mess on disk.
  if (is_disabled_)
    return false;

  if (!create_if_needed && !file_util::PathExists(db_file_path_))
    return false;

  db_.reset(new sql::Connection);
  meta_table_.reset(new sql::MetaTable);

  bool opened = false;
  if (!file_util::CreateDirectory(db_file_path_.DirName())) {
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
    if (!db_->Execute(sql.c_str()))
      return false;
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
    if (!db_->Execute(sql.c_str()))
      return false;
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
