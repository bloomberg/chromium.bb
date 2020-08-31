// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_DATABASE_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_DATABASE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/browser/appcache/appcache_namespace.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/content_export.h"
#include "sql/statement_id.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace sql {
class Database;
class MetaTable;
class Statement;
}

namespace content {
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, CacheRecords);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, EntryRecords);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, QuickIntegrityCheck);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, NamespaceRecords);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, GroupRecords);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, GroupAccessAndEvictionTimes);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, LazyOpen);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, ExperimentalFlags);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, OnlineWhiteListRecords);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, ReCreate);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, DeletableResponseIds);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, OriginUsage);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, FindCachesForOrigin);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest,
                     UpgradeSchemaForVersionsWithoutSupportedMigrations);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, UpgradeSchemaFrom7);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, UpgradeSchemaFrom8);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, UpgradeSchemaFrom9);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, WasCorrutionDetected);
class AppCacheDatabaseTest;
class AppCacheStorageImplTest;

// A wrapper around the SQLite database that serves one StoragePartition.
class CONTENT_EXPORT AppCacheDatabase {
 public:
  struct CONTENT_EXPORT GroupRecord {
    GroupRecord();
    GroupRecord(const GroupRecord& other);
    ~GroupRecord();

    int64_t group_id;
    url::Origin origin;
    GURL manifest_url;
    base::Time creation_time;
    base::Time last_access_time;
    base::Time last_full_update_check_time;
    base::Time first_evictable_error_time;
    base::Time token_expires;
  };

  struct CONTENT_EXPORT CacheRecord {
    CacheRecord();
    CacheRecord(const CacheRecord& other);
    ~CacheRecord();

    int64_t cache_id = 0;
    int64_t group_id = 0;
    bool online_wildcard = false;
    base::Time update_time;
    int64_t cache_size = 0;    // the sum of all response sizes in this cache
    int64_t padding_size = 0;  // the sum of all padding sizes in this cache
    int64_t manifest_parser_version = -1;
    std::string manifest_scope;
    base::Time token_expires;
  };

  struct EntryRecord {
    EntryRecord()
        : cache_id(0),
          flags(0),
          response_id(0),
          response_size(0),
          padding_size(0) {}

    int64_t cache_id;
    GURL url;
    int flags;
    int64_t response_id;
    int64_t response_size;
    int64_t padding_size;  // space added to obfuscate quota used by this entry
    base::Time token_expires;
  };

  struct CONTENT_EXPORT NamespaceRecord {
    NamespaceRecord();
    ~NamespaceRecord();

    int64_t cache_id;
    url::Origin origin;
    AppCacheNamespace namespace_;
    base::Time token_expires;
  };

  using NamespaceRecordVector = std::vector<NamespaceRecord>;

  struct OnlineWhiteListRecord {
    OnlineWhiteListRecord() : cache_id(0) {}

    int64_t cache_id;
    GURL namespace_url;
  };

  explicit AppCacheDatabase(const base::FilePath& path);
  ~AppCacheDatabase();

  void Disable();
  bool is_disabled() const { return is_disabled_; }
  bool was_corruption_detected() const { return was_corruption_detected_; }

  int64_t GetOriginUsage(const url::Origin& origin);
  bool GetAllOriginUsage(std::map<url::Origin, int64_t>* usage_map);

  bool FindOriginsWithGroups(std::set<url::Origin>* origins);
  bool FindLastStorageIds(int64_t* last_group_id,
                          int64_t* last_cache_id,
                          int64_t* last_response_id,
                          int64_t* last_deletable_response_rowid);

  bool FindGroup(int64_t group_id, GroupRecord* record);
  bool FindGroupForManifestUrl(const GURL& manifest_url, GroupRecord* record);
  bool FindGroupsForOrigin(const url::Origin& origin,
                           std::vector<GroupRecord>* records);
  bool FindGroupForCache(int64_t cache_id, GroupRecord* record);
  bool InsertGroup(const GroupRecord* record);
  bool DeleteGroup(int64_t group_id);

  // The access, eviction time, and manifest version/scope update methods do not
  // fail when given invalid group_ids. The return value only indicates whether
  // the database is functioning.
  bool UpdateLastAccessTime(int64_t group_id, base::Time last_access_time);
  bool LazyUpdateLastAccessTime(int64_t group_id, base::Time last_access_time);
  bool UpdateEvictionTimes(int64_t group_id,
                           base::Time last_full_update_check_time,
                           base::Time first_evictable_error_time);
  bool CommitLazyLastAccessTimes();  // The destructor calls this too.

  bool FindCache(int64_t cache_id, CacheRecord* record);
  bool FindCacheForGroup(int64_t group_id, CacheRecord* record);
  bool FindCachesForOrigin(const url::Origin& origin,
                           std::vector<CacheRecord>* records);
  bool InsertCache(const CacheRecord* record);
  bool DeleteCache(int64_t cache_id);

  bool FindEntriesForCache(int64_t cache_id, std::vector<EntryRecord>* records);
  bool FindEntriesForUrl(
      const GURL& url, std::vector<EntryRecord>* records);
  bool FindEntry(int64_t cache_id, const GURL& url, EntryRecord* record);
  bool InsertEntry(const EntryRecord* record);
  bool InsertEntryRecords(
      const std::vector<EntryRecord>& records);
  bool DeleteEntriesForCache(int64_t cache_id);
  bool AddEntryFlags(const GURL& entry_url,
                     int64_t cache_id,
                     int additional_flags);
  bool FindResponseIdsForCacheAsVector(int64_t cache_id,
                                       std::vector<int64_t>* response_ids) {
    return FindResponseIdsForCacheHelper(cache_id, response_ids, nullptr);
  }
  bool FindResponseIdsForCacheAsSet(int64_t cache_id,
                                    std::set<int64_t>* response_ids) {
    return FindResponseIdsForCacheHelper(cache_id, nullptr, response_ids);
  }

  bool FindNamespacesForOrigin(const url::Origin& origin,
                               NamespaceRecordVector* intercepts,
                               NamespaceRecordVector* fallbacks);
  bool FindNamespacesForCache(int64_t cache_id,
                              NamespaceRecordVector* intercepts,
                              std::vector<NamespaceRecord>* fallbacks);
  bool InsertNamespaceRecords(
      const NamespaceRecordVector& records);
  bool InsertNamespace(const NamespaceRecord* record);
  bool DeleteNamespacesForCache(int64_t cache_id);

  bool FindOnlineWhiteListForCache(int64_t cache_id,
                                   std::vector<OnlineWhiteListRecord>* records);
  bool InsertOnlineWhiteList(const OnlineWhiteListRecord* record);
  bool InsertOnlineWhiteListRecords(
      const std::vector<OnlineWhiteListRecord>& records);
  bool DeleteOnlineWhiteListForCache(int64_t cache_id);

  bool GetDeletableResponseIds(std::vector<int64_t>* response_ids,
                               int64_t max_rowid,
                               int limit);
  bool InsertDeletableResponseIds(const std::vector<int64_t>& response_ids);
  bool DeleteDeletableResponseIds(const std::vector<int64_t>& response_ids);

  void SetOriginTrialRequired(bool req) { is_origin_trial_required_ = req; }

  // So our callers can wrap operations in transactions.
  sql::Database* db_connection() {
    LazyOpen(true);
    return db_.get();
  }

  bool HasValidOriginTrialToken(CacheRecord* record);

 private:
  bool RunCachedStatementWithIds(sql::StatementID statement_id,
                                 const char* sql,
                                 const std::vector<int64_t>& ids);
  bool RunUniqueStatementWithInt64Result(const char* sql, int64_t* result);

  bool FindResponseIdsForCacheHelper(int64_t cache_id,
                                     std::vector<int64_t>* ids_vector,
                                     std::set<int64_t>* ids_set);

  // Record retrieval helpers
  void ReadGroupRecord(const sql::Statement& statement, GroupRecord* record);
  void ReadCacheRecord(const sql::Statement& statement, CacheRecord* record);
  void ReadEntryRecord(const sql::Statement& statement, EntryRecord* record);
  void ReadNamespaceRecords(
      sql::Statement* statement,
      NamespaceRecordVector* intercepts,
      NamespaceRecordVector* fallbacks);
  void ReadNamespaceRecord(
      const sql::Statement* statement, NamespaceRecord* record);
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

  void OnDatabaseError(int err, sql::Statement* stmt);

  base::FilePath db_file_path_;
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<sql::MetaTable> meta_table_;
  std::map<int64_t, base::Time> lazy_last_access_times_;
  bool is_disabled_;
  bool is_recreating_;
  bool was_corruption_detected_;
  bool is_origin_trial_required_ = false;

  friend class content::AppCacheDatabaseTest;
  friend class content::AppCacheStorageImplTest;

  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, CacheRecords);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, EntryRecords);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, QuickIntegrityCheck);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, NamespaceRecords);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, GroupRecords);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest,
                           GroupAccessAndEvictionTimes);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, LazyOpen);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, ExperimentalFlags);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest,
                           OnlineWhiteListRecords);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, ReCreate);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, DeletableResponseIds);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, OriginUsage);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, FindCachesForOrigin);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest,
                           UpgradeSchemaForVersionsWithoutSupportedMigrations);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, UpgradeSchemaFrom7);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, UpgradeSchemaFrom8);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, UpgradeSchemaFrom9);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, WasCorrutionDetected);

  DISALLOW_COPY_AND_ASSIGN(AppCacheDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_DATABASE_H_
