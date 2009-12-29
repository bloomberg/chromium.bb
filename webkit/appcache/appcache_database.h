// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_DATABASE_H_
#define WEBKIT_APPCACHE_APPCACHE_DATABASE_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace sql {
class Connection;
class MetaTable;
class Statement;
class StatementID;
}

namespace appcache {

class AppCacheDatabase {
 public:
  struct GroupRecord {
    int64 group_id;
    GURL origin;
    GURL manifest_url;
  };

  struct CacheRecord {
    int64 cache_id;
    int64 group_id;
    bool online_wildcard;
    base::TimeTicks update_time;
  };

  struct EntryRecord {
    int64 cache_id;
    GURL url;
    int flags;
    int64 response_id;
  };

  struct FallbackNameSpaceRecord {
    int64 cache_id;
    GURL origin;  // intentionally not normalized
    GURL namespace_url;
    GURL fallback_entry_url;
  };

  struct OnlineWhiteListRecord {
    int64 cache_id;
    GURL namespace_url;
  };

  explicit AppCacheDatabase(const FilePath& path);
  ~AppCacheDatabase();

  void CloseConnection();
  bool FindOriginsWithGroups(std::set<GURL>* origins);
  bool FindLastStorageIds(
      int64* last_group_id, int64* last_cache_id, int64* last_response_id);

  bool FindGroup(int64 group_id, GroupRecord* record);
  bool FindGroupForManifestUrl(const GURL& manifest_url, GroupRecord* record);
  bool FindGroupsForOrigin(
      const GURL& origin, std::vector<GroupRecord>* records);
  bool FindGroupForCache(int64 cache_id, GroupRecord* record);
  bool InsertGroup(const GroupRecord* record);
  bool DeleteGroup(int64 group_id);

  bool FindCache(int64 cache_id, CacheRecord* record);
  bool FindCacheForGroup(int64 group_id, CacheRecord* record);
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

  // So our callers can wrap operations in transactions.
  sql::Connection* db_connection() {
    LazyOpen(true);
    return db_.get();
  }

 private:
  bool PrepareUniqueStatement(const char* sql, sql::Statement* statement);
  bool PrepareCachedStatement(
      const sql::StatementID& id, const char* sql, sql::Statement* statement);

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

  // Deletes the existing database file and the entire directory containing
  // the database file including the disk cache in which response headers
  // and bodies are stored, and then creates a new database file.
  bool DeleteExistingAndCreateNewDatabase();

  FilePath db_file_path_;
  scoped_ptr<sql::Connection> db_;
  scoped_ptr<sql::MetaTable> meta_table_;
  bool has_open_error_;
  bool is_recreating_;

  FRIEND_TEST(AppCacheDatabaseTest, CacheRecords);
  FRIEND_TEST(AppCacheDatabaseTest, EntryRecords);
  FRIEND_TEST(AppCacheDatabaseTest, FallbackNameSpaceRecords);
  FRIEND_TEST(AppCacheDatabaseTest, GroupRecords);
  FRIEND_TEST(AppCacheDatabaseTest, LazyOpen);
  FRIEND_TEST(AppCacheDatabaseTest, OnlineWhiteListRecords);
  FRIEND_TEST(AppCacheDatabaseTest, ReCreate);
  DISALLOW_COPY_AND_ASSIGN(AppCacheDatabase);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_DATABASE_H_
