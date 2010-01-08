// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_database.h"

#include "app/sql/connection.h"
#include "app/sql/meta_table.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/auto_reset.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "webkit/appcache/appcache_entry.h"

// Schema -------------------------------------------------------------------
namespace {

const int kCurrentVersion = 0;
const int kCompatibleVersion = 0;

const char* kGroupsTable = "Groups";
const char* kCachesTable = "Caches";
const char* kEntriesTable = "Entries";
const char* kFallbackNameSpacesTable = "FallbackNameSpaces";
const char* kOnlineWhiteListsTable = "OnlineWhiteLists";
const char* kDeletableResponseIdsTable = "DeletableResponseIds";

const struct {
  const char* table_name;
  const char* columns;
} kTables[] = {
  { kGroupsTable,
    "(group_id INTEGER PRIMARY KEY,"
    " origin TEXT,"
    " manifest_url TEXT)" },

  { kCachesTable,
    "(cache_id INTEGER PRIMARY KEY,"
    " group_id INTEGER,"
    " online_wildcard INTEGER CHECK(online_wildcard IN (0, 1)),"
    " update_time INTEGER,"
    " cache_size INTEGER)" },  // intentionally not normalized

  { kEntriesTable,
    "(cache_id INTEGER,"
    " url TEXT,"
    " flags INTEGER,"
    " response_id INTEGER,"
    " response_size INTEGER)" },

  { kFallbackNameSpacesTable,
    "(cache_id INTEGER,"
    " origin TEXT,"  // intentionally not normalized
    " namespace_url TEXT,"
    " fallback_entry_url TEXT)" },

  { kOnlineWhiteListsTable,
    "(cache_id INTEGER,"
    " namespace_url TEXT)" },

  { kDeletableResponseIdsTable,
    "(response_id INTEGER NOT NULL)" },
};

const struct {
  const char* index_name;
  const char* table_name;
  const char* columns;
  bool unique;
} kIndexes[] = {
  { "GroupsOriginIndex",
    kGroupsTable,
    "(origin)",
    false },

  { "GroupsManifestIndex",
    kGroupsTable,
    "(manifest_url)",
    true },

  { "CachesGroupIndex",
    kCachesTable,
    "(group_id)",
    false },

  { "EntriesCacheIndex",
    kEntriesTable,
    "(cache_id)",
    false },

  { "EntriesCacheAndUrlIndex",
    kEntriesTable,
    "(cache_id, url)",
    true },

  { "FallbackNameSpacesCacheIndex",
    kFallbackNameSpacesTable,
    "(cache_id)",
    false },

  { "FallbackNameSpacesOriginIndex",
    kFallbackNameSpacesTable,
    "(origin)",
    false },

  { "FallbackNameSpacesCacheAndUrlIndex",
    kFallbackNameSpacesTable,
    "(cache_id, namespace_url)",
    true },

  { "OnlineWhiteListCacheIndex",
    kOnlineWhiteListsTable,
    "(cache_id)",
    false },

  { "DeletableResponsesIdIndex",
    kDeletableResponseIdsTable,
    "(response_id)",
    true },
};

const int kTableCount = ARRAYSIZE_UNSAFE(kTables);
const int kIndexCount = ARRAYSIZE_UNSAFE(kIndexes);

}  // anon namespace


// AppCacheDatabase ----------------------------------------------------------
namespace appcache {

AppCacheDatabase::AppCacheDatabase(const FilePath& path)
    : db_file_path_(path), has_open_error_(false), is_recreating_(false) {
}

AppCacheDatabase::~AppCacheDatabase() {
}

void AppCacheDatabase::CloseConnection() {
  // We can't close the connection for an in-memory database w/o
  // losing all of the data, so we don't do that.
  if (!db_file_path_.empty()) {
    meta_table_.reset();
    db_.reset();
  }
}

bool AppCacheDatabase::FindOriginsWithGroups(std::set<GURL>* origins) {
  DCHECK(origins && origins->empty());
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT DISTINCT(origin) FROM Groups";

  sql::Statement statement;
  if (!PrepareUniqueStatement(kSql, &statement))
    return false;

  while (statement.Step())
    origins->insert(GURL(statement.ColumnString(0)));

  return statement.Succeeded();
}

bool AppCacheDatabase::FindLastStorageIds(
    int64* last_group_id, int64* last_cache_id, int64* last_response_id,
    int64* last_deletable_response_rowid) {
  DCHECK(last_group_id && last_cache_id && last_response_id &&
         last_deletable_response_rowid);

  *last_group_id = 0;
  *last_cache_id = 0;
  *last_response_id = 0;
  *last_deletable_response_rowid = 0;

  if (!LazyOpen(false))
    return false;

  const char* kMaxGroupIdSql = "SELECT MAX(group_id) FROM Groups";
  const char* kMaxCacheIdSql = "SELECT MAX(cache_id) FROM Caches";
  const char* kMaxDeletableResponseRowIdSql =
      "SELECT MAX(rowid) FROM DeletableResponseIds";
  int64 group_id;
  int64 cache_id;
  int64 deletable_response_rowid;
  if (!RunUniqueStatementWithInt64Result(kMaxGroupIdSql, &group_id) ||
      !RunUniqueStatementWithInt64Result(kMaxCacheIdSql, &cache_id) ||
      !RunUniqueStatementWithInt64Result(kMaxDeletableResponseRowIdSql,
                                         &deletable_response_rowid)) {
    return false;
  }

  // TODO(michaeln): SELECT MAX(responseId) FROM somewhere,
  // or retrieve from the meta_table.
  int64 response_id = 0;

  *last_group_id = group_id;
  *last_cache_id = cache_id;
  *last_response_id = response_id;
  *last_deletable_response_rowid = deletable_response_rowid;
  return true;
}

bool AppCacheDatabase::FindGroup(int64 group_id, GroupRecord* record) {
  DCHECK(record);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT group_id, origin, manifest_url FROM Groups WHERE group_id = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, group_id);
  if (!statement.Step() || !statement.Succeeded())
    return false;

  ReadGroupRecord(statement, record);
  DCHECK(record->group_id == group_id);
  return true;
}

bool AppCacheDatabase::FindGroupForManifestUrl(
    const GURL& manifest_url, GroupRecord* record) {
  DCHECK(record);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT group_id, origin, manifest_url FROM Groups"
      "  WHERE manifest_url = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindString(0, manifest_url.spec());
  if (!statement.Step() || !statement.Succeeded())
    return false;

  ReadGroupRecord(statement, record);
  DCHECK(record->manifest_url == manifest_url);
  return true;
}

bool AppCacheDatabase::FindGroupsForOrigin(
    const GURL& origin, std::vector<GroupRecord>* records) {
  DCHECK(records && records->empty());
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT group_id, origin, manifest_url FROM Groups WHERE origin = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindString(0, origin.spec());
  while (statement.Step()) {
    records->push_back(GroupRecord());
    ReadGroupRecord(statement, &records->back());
    DCHECK(records->back().origin == origin);
  }

  return statement.Succeeded();
}

bool AppCacheDatabase::FindGroupForCache(int64 cache_id, GroupRecord* record) {
  DCHECK(record);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT g.group_id, g.origin, g.manifest_url FROM Groups g, Caches c"
      "  WHERE c.cache_id = ? AND c.group_id = g.group_id";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, cache_id);
  if (!statement.Step() || !statement.Succeeded())
    return false;

  ReadGroupRecord(statement, record);
  return true;
}

bool AppCacheDatabase::InsertGroup(const GroupRecord* record) {
  if (!LazyOpen(true))
    return false;

  const char* kSql =
      "INSERT INTO Groups (group_id, origin, manifest_url)"
      "  VALUES(?, ?, ?)";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, record->group_id);
  statement.BindString(1, record->origin.spec());
  statement.BindString(2, record->manifest_url.spec());
  return statement.Run();
}

bool AppCacheDatabase::DeleteGroup(int64 group_id) {
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "DELETE FROM Groups WHERE group_id = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, group_id);
  return statement.Run();
}

bool AppCacheDatabase::FindCache(int64 cache_id, CacheRecord* record) {
  DCHECK(record);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT cache_id, group_id, online_wildcard, update_time, cache_size"
      " FROM Caches WHERE cache_id = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, cache_id);
  if (!statement.Step() || !statement.Succeeded())
    return false;

  ReadCacheRecord(statement, record);
  return true;
}

bool AppCacheDatabase::FindCacheForGroup(int64 group_id, CacheRecord* record) {
  DCHECK(record);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT cache_id, group_id, online_wildcard, update_time, cache_size"
      "  FROM Caches WHERE group_id = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, group_id);
  if (!statement.Step() || !statement.Succeeded())
    return false;

  ReadCacheRecord(statement, record);
  return true;
}

bool AppCacheDatabase::InsertCache(const CacheRecord* record) {
  if (!LazyOpen(true))
    return false;

  const char* kSql =
      "INSERT INTO Caches (cache_id, group_id, online_wildcard,"
      "                    update_time, cache_size)"
      "  VALUES(?, ?, ?, ?, ?)";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, record->cache_id);
  statement.BindInt64(1, record->group_id);
  statement.BindBool(2, record->online_wildcard);

  // There are no convinient methods to convert TimeTicks to or
  // from microseconds directly, so we compute TimeDelta's
  // as an intermediary step.
  statement.BindInt64(3,
    (record->update_time - base::TimeTicks()).InMicroseconds());

  statement.BindInt64(4, record->cache_size);

  return statement.Run();
}

bool AppCacheDatabase::DeleteCache(int64 cache_id) {
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "DELETE FROM Caches WHERE cache_id = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, cache_id);
  return statement.Run();
}

bool AppCacheDatabase::FindEntriesForCache(
    int64 cache_id, std::vector<EntryRecord>* records) {
  DCHECK(records && records->empty());
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT cache_id, url, flags, response_id, response_size FROM Entries"
      "  WHERE cache_id = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, cache_id);
  while (statement.Step()) {
    records->push_back(EntryRecord());
    ReadEntryRecord(statement, &records->back());
    DCHECK(records->back().cache_id == cache_id);
  }

  return statement.Succeeded();
}

bool AppCacheDatabase::FindEntriesForUrl(
    const GURL& url, std::vector<EntryRecord>* records) {
  DCHECK(records && records->empty());
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT cache_id, url, flags, response_id, response_size FROM Entries"
      "  WHERE url = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindString(0, url.spec());
  while (statement.Step()) {
    records->push_back(EntryRecord());
    ReadEntryRecord(statement, &records->back());
    DCHECK(records->back().url == url);
  }

  return statement.Succeeded();
}

bool AppCacheDatabase::FindEntry(
    int64 cache_id, const GURL& url, EntryRecord* record) {
  DCHECK(record);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT cache_id, url, flags, response_id, response_size FROM Entries"
      "  WHERE cache_id = ? AND url = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, cache_id);
  statement.BindString(1, url.spec());
  if (!statement.Step() || !statement.Succeeded())
    return false;

  ReadEntryRecord(statement, record);
  DCHECK(record->cache_id == cache_id);
  DCHECK(record->url == url);
  return true;
}

bool AppCacheDatabase::InsertEntry(const EntryRecord* record) {
  if (!LazyOpen(true))
    return false;

  const char* kSql =
      "INSERT INTO Entries (cache_id, url, flags, response_id, response_size)"
      "  VALUES(?, ?, ?, ?, ?)";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, record->cache_id);
  statement.BindString(1, record->url.spec());
  statement.BindInt(2, record->flags);
  statement.BindInt64(3, record->response_id);
  statement.BindInt64(4, record->response_size);
  return statement.Run();
}

bool AppCacheDatabase::InsertEntryRecords(
    const std::vector<EntryRecord>& records) {
  if (records.empty())
    return true;
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;
  std::vector<EntryRecord>::const_iterator iter = records.begin();
  while (iter != records.end()) {
    if (!InsertEntry(&(*iter)))
      return false;
    ++iter;
  }
  return transaction.Commit();
}

bool AppCacheDatabase::DeleteEntriesForCache(int64 cache_id) {
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "DELETE FROM Entries WHERE cache_id = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, cache_id);
  return statement.Run();
}

bool AppCacheDatabase::AddEntryFlags(
    const GURL& entry_url, int64 cache_id, int additional_flags) {
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "UPDATE Entries SET flags = flags | ? WHERE cache_id = ? AND url = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt(0, additional_flags);
  statement.BindInt64(1, cache_id);
  statement.BindString(2, entry_url.spec());
  return statement.Run() && db_->GetLastChangeCount();
}

bool AppCacheDatabase::FindFallbackNameSpacesForOrigin(
    const GURL& origin, std::vector<FallbackNameSpaceRecord>* records) {
  DCHECK(records && records->empty());
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT cache_id, origin, namespace_url, fallback_entry_url"
      "  FROM FallbackNameSpaces WHERE origin = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindString(0, origin.spec());
  while (statement.Step()) {
    records->push_back(FallbackNameSpaceRecord());
    ReadFallbackNameSpaceRecord(statement, &records->back());
    DCHECK(records->back().origin == origin);
  }
  return statement.Succeeded();
}

bool AppCacheDatabase::FindFallbackNameSpacesForCache(
    int64 cache_id, std::vector<FallbackNameSpaceRecord>* records) {
  DCHECK(records && records->empty());
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT cache_id, origin, namespace_url, fallback_entry_url"
      "  FROM FallbackNameSpaces WHERE cache_id = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, cache_id);
  while (statement.Step()) {
    records->push_back(FallbackNameSpaceRecord());
    ReadFallbackNameSpaceRecord(statement, &records->back());
    DCHECK(records->back().cache_id == cache_id);
  }
  return statement.Succeeded();
}

bool AppCacheDatabase::InsertFallbackNameSpace(
    const FallbackNameSpaceRecord* record) {
  if (!LazyOpen(true))
    return false;

  const char* kSql =
      "INSERT INTO FallbackNameSpaces"
      "  (cache_id, origin, namespace_url, fallback_entry_url)"
      "  VALUES (?, ?, ?, ?)";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, record->cache_id);
  statement.BindString(1, record->origin.spec());
  statement.BindString(2, record->namespace_url.spec());
  statement.BindString(3, record->fallback_entry_url.spec());
  return statement.Run();
}

bool AppCacheDatabase::InsertFallbackNameSpaceRecords(
    const std::vector<FallbackNameSpaceRecord>& records) {
  if (records.empty())
    return true;
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;
  std::vector<FallbackNameSpaceRecord>::const_iterator iter = records.begin();
  while (iter != records.end()) {
    if (!InsertFallbackNameSpace(&(*iter)))
      return false;
    ++iter;
  }
  return transaction.Commit();
}

bool AppCacheDatabase::DeleteFallbackNameSpacesForCache(int64 cache_id) {
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "DELETE FROM FallbackNameSpaces WHERE cache_id = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, cache_id);
  return statement.Run();
}

bool AppCacheDatabase::FindOnlineWhiteListForCache(
    int64 cache_id, std::vector<OnlineWhiteListRecord>* records) {
  DCHECK(records && records->empty());
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT cache_id, namespace_url FROM OnlineWhiteLists"
      "  WHERE cache_id = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, cache_id);
  while (statement.Step()) {
    records->push_back(OnlineWhiteListRecord());
    this->ReadOnlineWhiteListRecord(statement, &records->back());
    DCHECK(records->back().cache_id == cache_id);
  }
  return statement.Succeeded();
}

bool AppCacheDatabase::InsertOnlineWhiteList(
    const OnlineWhiteListRecord* record) {
  if (!LazyOpen(true))
    return false;

  const char* kSql =
      "INSERT INTO OnlineWhiteLists (cache_id, namespace_url) VALUES (?, ?)";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, record->cache_id);
  statement.BindString(1, record->namespace_url.spec());
  return statement.Run();
}

bool AppCacheDatabase::InsertOnlineWhiteListRecords(
    const std::vector<OnlineWhiteListRecord>& records) {
  if (records.empty())
    return true;
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;
  std::vector<OnlineWhiteListRecord>::const_iterator iter = records.begin();
  while (iter != records.end()) {
    if (!InsertOnlineWhiteList(&(*iter)))
      return false;
    ++iter;
  }
  return transaction.Commit();
}

bool AppCacheDatabase::DeleteOnlineWhiteListForCache(int64 cache_id) {
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "DELETE FROM OnlineWhiteLists WHERE cache_id = ?";

  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, cache_id);
  return statement.Run();
}

bool AppCacheDatabase::GetDeletableResponseIds(
    std::vector<int64>* response_ids, int64 max_rowid, int limit) {
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT response_id FROM DeletableResponseIds "
      "  WHERE rowid <= ?"
      "  LIMIT ?";
  sql::Statement statement;
  if (!PrepareCachedStatement(SQL_FROM_HERE, kSql, &statement))
    return false;

  statement.BindInt64(0, max_rowid);
  statement.BindInt64(1, limit);
  while (statement.Step())
    response_ids->push_back(statement.ColumnInt64(0));
  return statement.Succeeded();
}

bool AppCacheDatabase::InsertDeletableResponseIds(
    const std::vector<int64>& response_ids) {
  const char* kSql =
      "INSERT INTO DeletableResponseIds (response_id) VALUES (?)";
  return RunCachedStatementWithIds(SQL_FROM_HERE, kSql, response_ids);
}

bool AppCacheDatabase::DeleteDeletableResponseIds(
    const std::vector<int64>& response_ids) {
  const char* kSql =
      "DELETE FROM DeletableResponseIds WHERE response_id = ?";
  return RunCachedStatementWithIds(SQL_FROM_HERE, kSql, response_ids);
}

bool AppCacheDatabase::RunCachedStatementWithIds(
    const sql::StatementID& statement_id, const char* sql,
    const std::vector<int64>& ids) {
  if (!LazyOpen(true))
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  sql::Statement statement;
  if (!PrepareCachedStatement(statement_id, sql, &statement))
    return false;

  std::vector<int64>::const_iterator iter = ids.begin();
  while (iter != ids.end()) {
    statement.BindInt64(0, *iter);
    if (!statement.Run())
      return false;
    statement.Reset();
    ++iter;
  }

  return transaction.Commit();
}

bool AppCacheDatabase::RunUniqueStatementWithInt64Result(
    const char* sql, int64* result) {
  sql::Statement statement;
  if (!PrepareUniqueStatement(sql, &statement) ||
      !statement.Step()) {
    return false;
  }
  *result = statement.ColumnInt64(0);
  return true;
}

bool AppCacheDatabase::PrepareUniqueStatement(
    const char* sql, sql::Statement* statement) {
  DCHECK(sql && statement);
  statement->Assign(db_->GetUniqueStatement(sql));
  if (!statement->is_valid()) {
    NOTREACHED() << db_->GetErrorMessage();
    return false;
  }
  return true;
}

bool AppCacheDatabase::PrepareCachedStatement(
    const sql::StatementID& id, const char* sql, sql::Statement* statement) {
  DCHECK(sql && statement);
  statement->Assign(db_->GetCachedStatement(id, sql));
  if (!statement->is_valid()) {
    NOTREACHED() << db_->GetErrorMessage();
    return false;
  }
  return true;
}

void AppCacheDatabase::ReadGroupRecord(
    const sql::Statement& statement, GroupRecord* record) {
  record->group_id = statement.ColumnInt64(0);
  record->origin = GURL(statement.ColumnString(1));
  record->manifest_url = GURL(statement.ColumnString(2));
}

void AppCacheDatabase::ReadCacheRecord(
    const sql::Statement& statement, CacheRecord* record) {
  record->cache_id = statement.ColumnInt64(0);
  record->group_id = statement.ColumnInt64(1);
  record->online_wildcard = statement.ColumnBool(2);

  // There are no convinient methods to convert TimeTicks to or
  // from microseconds directly, so we compute TimeDelta's
  // as an intermediary step.
  record->update_time = base::TimeTicks() +
      base::TimeDelta::FromMicroseconds(statement.ColumnInt64(3));

  record->cache_size = statement.ColumnInt64(4);
}

void AppCacheDatabase::ReadEntryRecord(
    const sql::Statement& statement, EntryRecord* record) {
  record->cache_id = statement.ColumnInt64(0);
  record->url = GURL(statement.ColumnString(1));
  record->flags = statement.ColumnInt(2);
  record->response_id = statement.ColumnInt64(3);
  record->response_size = statement.ColumnInt64(4);
}

void AppCacheDatabase::ReadFallbackNameSpaceRecord(
    const sql::Statement& statement, FallbackNameSpaceRecord* record) {
  record->cache_id = statement.ColumnInt64(0);
  record->origin = GURL(statement.ColumnString(1));
  record->namespace_url = GURL(statement.ColumnString(2));
  record->fallback_entry_url = GURL(statement.ColumnString(3));
}

void AppCacheDatabase::ReadOnlineWhiteListRecord(
    const sql::Statement& statement, OnlineWhiteListRecord* record) {
  record->cache_id = statement.ColumnInt64(0);
  record->namespace_url = GURL(statement.ColumnString(1));
}

bool AppCacheDatabase::LazyOpen(bool create_if_needed) {
  if (db_.get())
    return true;

  // If we tried and failed once, don't try again in the same session
  // to avoid creating an incoherent mess on disk.
  if (has_open_error_)
    return false;

  // Avoid creating a database at all if we can.
  bool use_in_memory_db = db_file_path_.empty();
  if (!create_if_needed &&
      (use_in_memory_db || !file_util::PathExists(db_file_path_))) {
    return false;
  }

  db_.reset(new sql::Connection);
  meta_table_.reset(new sql::MetaTable);

  bool opened = use_in_memory_db ? db_->OpenInMemory()
                                 : db_->Open(db_file_path_);
  if (opened)
    db_->Preload();
  if (!opened || !EnsureDatabaseVersion()) {
    has_open_error_ = true;
    meta_table_.reset();
    db_.reset();
    return false;
  }

  return true;
}

bool AppCacheDatabase::EnsureDatabaseVersion() {
  if (!sql::MetaTable::DoesTableExist(db_.get()))
    return CreateSchema();

  if (!meta_table_->Init(db_.get(), kCurrentVersion, kCompatibleVersion))
    return false;

  if (meta_table_->GetCompatibleVersionNumber() > kCurrentVersion) {
    LOG(WARNING) << "AppCache database is too new.";
    return false;
  }

  if (meta_table_->GetVersionNumber() < kCurrentVersion)
    return UpgradeSchema();

#ifndef NDEBUG
  DCHECK(sql::MetaTable::DoesTableExist(db_.get()));
  for (int i = 0; i < kTableCount; ++i) {
    DCHECK(db_->DoesTableExist(kTables[i].table_name));
  }
#endif

  return true;
}

bool AppCacheDatabase::CreateSchema() {
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

bool AppCacheDatabase::UpgradeSchema() {
  DCHECK(false);  // We don't have any upgrades yet since we're at version 0

  // Upgrade logic goes here

  // If there is no upgrade path for the version on disk to the current
  // version, nuke everything and start over.
  return DeleteExistingAndCreateNewDatabase();
}

bool AppCacheDatabase::DeleteExistingAndCreateNewDatabase() {
  DCHECK(!db_file_path_.empty());
  DCHECK(file_util::PathExists(db_file_path_));

  meta_table_.reset();
  db_.reset();

  FilePath directory = db_file_path_.DirName();
  if (!file_util::Delete(directory, true) ||
      !file_util::CreateDirectory(directory)) {
    return false;
  }

  // Make sure the steps above actually deleted things.
  if (file_util::PathExists(db_file_path_))
    return false;

  // So we can't go recursive.
  if (is_recreating_)
    return false;

  AutoReset auto_reset(&is_recreating_, true);
  return LazyOpen(true);
}

}  // namespace appcache
