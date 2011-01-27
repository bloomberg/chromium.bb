// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_DATABASE_H_
#define WEBKIT_APPCACHE_APPCACHE_DATABASE_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

namespace sql {
class Connection;
class MetaTable;
class Statement;
class StatementID;
}

namespace webkit_database {
class QuotaTable;
}

namespace appcache {

class AppCacheDatabase {
 public:
  struct GroupRecord {
    GroupRecord();
    ~GroupRecord();

    int64 group_id;
    GURL origin;
    GURL manifest_url;
    base::Time creation_time;
    base::Time last_access_time;
  };

  struct CacheRecord {
    CacheRecord()
        : cache_id(0), group_id(0), online_wildcard(false), cache_size(0) {}

    int64 cache_id;
    int64 group_id;
    bool online_wildcard;
    base::Time update_time;
    int64 cache_size;  // the sum of all response sizes in this cache
  };

  struct EntryRecord {
    EntryRecord() : cache_id(0), flags(0), response_id(0), response_size(0) {}

    int64 cache_id;
    GURL url;
    int flags;
    int64 response_id;
    int64 response_size;
  };

  struct FallbackNameSpaceRecord {
    FallbackNameSpaceRecord();
    ~FallbackNameSpaceRecord();

    int64 cache_id;
    GURL origin;  // intentionally not normalized
    GURL namespace_url;
    GURL fallback_entry_url;
  };

  struct OnlineWhiteListRecord {
    OnlineWhiteListRecord() : cache_id(0) {}

    int64 cache_id;
    GURL namespace_url;
  };

  explicit AppCacheDatabase(const FilePath& path);
  ~AppCacheDatabase();

  void CloseConnection();
  void Disable();
  bool is_disabled() const { return is_disabled_; }

  int64 GetDefaultOriginQuota() { return 5 * 1024 * 1024; }
  int64 GetOriginUsage(const GURL& origin);
  int64 GetOriginQuota(const GURL& origin);

  bool FindOriginsWithGroups(std::set<GURL>* origins);
  bool FindLastStorageIds(
      int64* last_group_id, int64* last_cache_id, int64* last_response_id,
      int64* last_deletable_response_rowid);

  bool FindGroup(int64 group_id, GroupRecord* record);
  bool FindGroupForManifestUrl(const GURL& manifest_url, GroupRecord* record);
  bool FindGroupsForOrigin(
      const GURL& origin, std::vector<GroupRecord>* records);
  bool FindGroupForCache(int64 cache_id, GroupRecord* record);
  bool UpdateGroupLastAccessTime(
      int64 group_id, base::Time last_access_time);
  bool InsertGroup(const GroupRecord* record);
  bool DeleteGroup(int64 group_id);

  bool FindCache(int64 cache_id, CacheRecord* record);
  bool FindCacheForGroup(int64 group_id, CacheRecord* record);
  bool FindCachesForOrigin(
      const GURL& origin, std::vector<CacheRecord>* records);
  bool InsertCache(const CacheRecord* record);
  bool DeleteCache(int64 cache_id);

  bool FindEntriesForCache(
      int64 cache_id, std::vector<EntryRecord>* records);
  bool FindEntriesForUrl(
      const GURL& url, std::vector<EntryRecord>* records);
  bool FindEntry(int64 cache_id, const GURL& url, EntryRecord* record);
  bool InsertEntry(const EntryRecord* record);
  bool InsertEntryRecords(
      const std::vector<EntryRecord>& records);
  bool DeleteEntriesForCache(int64 cache_id);
  bool AddEntryFlags(const GURL& entry_url, int64 cache_id,
                     int additional_flags);
  bool FindResponseIdsForCacheAsVector(
      int64 cache_id, std::vector<int64>* response_ids) {
    return FindResponseIdsForCacheHelper(cache_id, response_ids, NULL);
  }
  bool FindResponseIdsForCacheAsSet(
      int64 cache_id, std::set<int64>* response_ids) {
    return FindResponseIdsForCacheHelper(cache_id, NULL, response_ids);
  }

  bool FindFallbackNameSpacesForOrigin(
      const GURL& origin, std::vector<FallbackNameSpaceRecord>* records);
  bool FindFallbackNameSpacesForCache(
      int64 cache_id, std::vector<FallbackNameSpaceRecord>* records);
  bool InsertFallbackNameSpace(const FallbackNameSpaceRecord* record);
  bool InsertFallbackNameSpaceRecords(
      const std::vector<FallbackNameSpaceRecord>& records);
  bool DeleteFallbackNameSpacesForCache(int64 cache_id);

  bool FindOnlineWhiteListForCache(
      int64 cache_id, std::vector<OnlineWhiteListRecord>* records);
  bool InsertOnlineWhiteList(const OnlineWhiteListRecord* record);
  bool InsertOnlineWhiteListRecords(
      const std::vector<OnlineWhiteListRecord>& records);
  bool DeleteOnlineWhiteListForCache(int64 cache_id);

  bool GetDeletableResponseIds(std::vector<int64>* response_ids,
                               int64 max_rowid, int limit);
  bool InsertDeletableResponseIds(const std::vector<int64>& response_ids);
  bool DeleteDeletableResponseIds(const std::vector<int64>& response_ids);

  // So our callers can wrap operations in transactions.
  sql::Connection* db_connection() {
    LazyOpen(true);
    return db_.get();
  }

 private:
  bool RunCachedStatementWithIds(
      const sql::StatementID& statement_id, const char* sql,
      const std::vector<int64>& ids);
  bool RunUniqueStatementWithInt64Result(const char* sql, int64* result);

  bool PrepareUniqueStatement(const char* sql, sql::Statement* statement);
  bool PrepareCachedStatement(
      const sql::StatementID& id, const char* sql, sql::Statement* statement);

  bool FindResponseIdsForCacheHelper(
      int64 cache_id, std::vector<int64>* ids_vector,
      std::set<int64>* ids_set);

  // Record retrieval helpers
  void ReadGroupRecord(const sql::Statement& statement, GroupRecord* record);
  void ReadCacheRecord(const sql::Statement& statement, CacheRecord* record);
  void ReadEntryRecord(const sql::Statement& statement, EntryRecord* record);
  void ReadFallbackNameSpaceRecord(
      const sql::Statement& statement, FallbackNameSpaceRecord* record);
  void ReadOnlineWhiteListRecord(
      const sql::Statement& statement, OnlineWhiteListRecord* record);

  // Database creation
  bool LazyOpen(bool create_if_needed);
  bool EnsureDatabaseVersion();
  bool CreateSchema();
  bool UpgradeSchema();

  void ResetConnectionAndTables();

  // Deletes the existing database file and the entire directory containing
  // the database file including the disk cache in which response headers
  // and bodies are stored, and then creates a new database file.
  bool DeleteExistingAndCreateNewDatabase();

  FilePath db_file_path_;
  scoped_ptr<sql::Connection> db_;
  scoped_ptr<sql::MetaTable> meta_table_;
  scoped_ptr<webkit_database::QuotaTable> quota_table_;
  bool is_disabled_;
  bool is_recreating_;

  FRIEND_TEST_ALL_PREFIXES(AppCacheDatabaseTest, CacheRecords);
  FRIEND_TEST_ALL_PREFIXES(AppCacheDatabaseTest, EntryRecords);
  FRIEND_TEST_ALL_PREFIXES(AppCacheDatabaseTest, FallbackNameSpaceRecords);
  FRIEND_TEST_ALL_PREFIXES(AppCacheDatabaseTest, GroupRecords);
  FRIEND_TEST_ALL_PREFIXES(AppCacheDatabaseTest, LazyOpen);
  FRIEND_TEST_ALL_PREFIXES(AppCacheDatabaseTest, OnlineWhiteListRecords);
  FRIEND_TEST_ALL_PREFIXES(AppCacheDatabaseTest, ReCreate);
  FRIEND_TEST_ALL_PREFIXES(AppCacheDatabaseTest, DeletableResponseIds);
  FRIEND_TEST_ALL_PREFIXES(AppCacheDatabaseTest, Quotas);

  DISALLOW_COPY_AND_ASSIGN(AppCacheDatabase);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_DATABASE_H_
