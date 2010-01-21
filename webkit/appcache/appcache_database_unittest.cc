// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "app/sql/connection.h"
#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "webkit/appcache/appcache_database.h"
#include "webkit/appcache/appcache_entry.h"

namespace {

const base::TimeTicks kZeroTimeTicks;

class TestErrorDelegate : public sql::ErrorDelegate {
 public:
  virtual ~TestErrorDelegate() { }
  virtual int OnError(
      int error, sql::Connection* connection, sql::Statement* stmt) {
    return error;
  }
};

}  // namespace

namespace appcache {

class AppCacheDatabaseTest {};

TEST(AppCacheDatabaseTest, LazyOpen) {
  // Use an empty file path to use an in-memory sqlite database.
  const FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);

  EXPECT_FALSE(db.LazyOpen(false));
  EXPECT_TRUE(db.LazyOpen(true));

  int64 group_id, cache_id, response_id, deleteable_response_rowid;
  group_id = cache_id = response_id = deleteable_response_rowid = 0;
  EXPECT_TRUE(db.FindLastStorageIds(&group_id, &cache_id, &response_id,
                                    &deleteable_response_rowid));
  EXPECT_EQ(0, group_id);
  EXPECT_EQ(0, cache_id);
  EXPECT_EQ(0, response_id);
  EXPECT_EQ(0, deleteable_response_rowid);

  std::set<GURL> origins;
  EXPECT_TRUE(db.FindOriginsWithGroups(&origins));
  EXPECT_TRUE(origins.empty());
}

TEST(AppCacheDatabaseTest, ReCreate) {
  // Real files on disk for this test.
  ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath kDbFile = temp_dir.path().AppendASCII("appcache.db");
  const FilePath kNestedDir = temp_dir.path().AppendASCII("nested");
  const FilePath kOtherFile =  kNestedDir.AppendASCII("other_file");
  EXPECT_TRUE(file_util::CreateDirectory(kNestedDir));
  EXPECT_EQ(3, file_util::WriteFile(kOtherFile, "foo", 3));

  AppCacheDatabase db(kDbFile);
  EXPECT_FALSE(db.LazyOpen(false));
  EXPECT_TRUE(db.LazyOpen(true));

  EXPECT_TRUE(file_util::PathExists(kDbFile));
  EXPECT_TRUE(file_util::DirectoryExists(kNestedDir));
  EXPECT_TRUE(file_util::PathExists(kOtherFile));

  EXPECT_TRUE(db.DeleteExistingAndCreateNewDatabase());

  EXPECT_TRUE(file_util::PathExists(kDbFile));
  EXPECT_FALSE(file_util::DirectoryExists(kNestedDir));
  EXPECT_FALSE(file_util::PathExists(kOtherFile));
}

TEST(AppCacheDatabaseTest, EntryRecords) {
  const FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  // Set an error delegate that will make all operations return false on error.
  scoped_refptr<TestErrorDelegate> error_delegate(new TestErrorDelegate);
  db.db_->set_error_delegate(error_delegate);

  AppCacheDatabase::EntryRecord entry;

  entry.cache_id = 1;
  entry.url = GURL("http://blah/1");
  entry.flags = AppCacheEntry::MASTER;
  entry.response_id = 1;
  entry.response_size = 100;
  EXPECT_TRUE(db.InsertEntry(&entry));
  EXPECT_FALSE(db.InsertEntry(&entry));

  entry.cache_id = 2;
  entry.url = GURL("http://blah/2");
  entry.flags = AppCacheEntry::EXPLICIT;
  entry.response_id = 2;
  entry.response_size = 200;
  EXPECT_TRUE(db.InsertEntry(&entry));

  entry.cache_id = 2;
  entry.url = GURL("http://blah/3");
  entry.flags = AppCacheEntry::MANIFEST;
  entry.response_id = 3;
  entry.response_size = 300;
  EXPECT_TRUE(db.InsertEntry(&entry));

  std::vector<AppCacheDatabase::EntryRecord> found;

  EXPECT_TRUE(db.FindEntriesForCache(1, &found));
  EXPECT_EQ(1U, found.size());
  EXPECT_EQ(1, found[0].cache_id);
  EXPECT_EQ(GURL("http://blah/1"), found[0].url);
  EXPECT_EQ(AppCacheEntry::MASTER, found[0].flags);
  EXPECT_EQ(1, found[0].response_id);
  EXPECT_EQ(100, found[0].response_size);
  found.clear();

  EXPECT_TRUE(db.AddEntryFlags(GURL("http://blah/1"), 1,
                               AppCacheEntry::FOREIGN));
  EXPECT_TRUE(db.FindEntriesForCache(1, &found));
  EXPECT_EQ(1U, found.size());
  EXPECT_EQ(AppCacheEntry::MASTER | AppCacheEntry::FOREIGN, found[0].flags);
  found.clear();

  EXPECT_TRUE(db.FindEntriesForCache(2, &found));
  EXPECT_EQ(2U, found.size());
  EXPECT_EQ(2, found[0].cache_id);
  EXPECT_EQ(GURL("http://blah/2"), found[0].url);
  EXPECT_EQ(AppCacheEntry::EXPLICIT, found[0].flags);
  EXPECT_EQ(2, found[0].response_id);
  EXPECT_EQ(200, found[0].response_size);
  EXPECT_EQ(2, found[1].cache_id);
  EXPECT_EQ(GURL("http://blah/3"), found[1].url);
  EXPECT_EQ(AppCacheEntry::MANIFEST, found[1].flags);
  EXPECT_EQ(3, found[1].response_id);
  EXPECT_EQ(300, found[1].response_size);
  found.clear();

  EXPECT_TRUE(db.DeleteEntriesForCache(2));
  EXPECT_TRUE(db.FindEntriesForCache(2, &found));
  EXPECT_TRUE(found.empty());
  found.clear();

  EXPECT_TRUE(db.DeleteEntriesForCache(1));
  EXPECT_FALSE(db.AddEntryFlags(GURL("http://blah/1"), 1,
                                AppCacheEntry::FOREIGN));
}

TEST(AppCacheDatabaseTest, CacheRecords) {
  const FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  scoped_refptr<TestErrorDelegate> error_delegate(new TestErrorDelegate);
  db.db_->set_error_delegate(error_delegate);

  const AppCacheDatabase::CacheRecord kZeroRecord;
  AppCacheDatabase::CacheRecord record;
  EXPECT_FALSE(db.FindCache(1, &record));

  record.cache_id = 1;
  record.group_id = 1;
  record.online_wildcard = true;
  record.update_time = kZeroTimeTicks;
  record.cache_size = 100;
  EXPECT_TRUE(db.InsertCache(&record));
  EXPECT_FALSE(db.InsertCache(&record));

  record = kZeroRecord;
  EXPECT_TRUE(db.FindCache(1, &record));
  EXPECT_EQ(1, record.cache_id);
  EXPECT_EQ(1, record.group_id);
  EXPECT_TRUE(record.online_wildcard);
  EXPECT_TRUE(kZeroTimeTicks == record.update_time);
  EXPECT_EQ(100, record.cache_size);

  record = kZeroRecord;
  EXPECT_TRUE(db.FindCacheForGroup(1, &record));
  EXPECT_EQ(1, record.cache_id);
  EXPECT_EQ(1, record.group_id);
  EXPECT_TRUE(record.online_wildcard);
  EXPECT_TRUE(kZeroTimeTicks == record.update_time);
  EXPECT_EQ(100, record.cache_size);

  EXPECT_TRUE(db.DeleteCache(1));
  EXPECT_FALSE(db.FindCache(1, &record));
  EXPECT_FALSE(db.FindCacheForGroup(1, &record));

  EXPECT_TRUE(db.DeleteCache(1));
}

TEST(AppCacheDatabaseTest, GroupRecords) {
  const FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  scoped_refptr<TestErrorDelegate> error_delegate(new TestErrorDelegate);
  db.db_->set_error_delegate(error_delegate);

  const GURL kManifestUrl("http://blah/manifest");
  const GURL kOrigin(kManifestUrl.GetOrigin());

  const AppCacheDatabase::GroupRecord kZeroRecord;
  AppCacheDatabase::GroupRecord record;
  std::vector<AppCacheDatabase::GroupRecord> records;

  // Behavior with an empty table
  EXPECT_FALSE(db.FindGroup(1, &record));
  EXPECT_FALSE(db.FindGroupForManifestUrl(kManifestUrl, &record));
  EXPECT_TRUE(db.DeleteGroup(1));
  EXPECT_TRUE(db.FindGroupsForOrigin(kOrigin, &records));
  EXPECT_TRUE(records.empty());
  EXPECT_FALSE(db.FindGroupForCache(1, &record));

  record.group_id = 1;
  record.manifest_url = kManifestUrl;
  record.origin = kOrigin;
  EXPECT_TRUE(db.InsertGroup(&record));
  EXPECT_FALSE(db.InsertGroup(&record));

  record.group_id = 2;
  EXPECT_FALSE(db.InsertGroup(&record));

  record = kZeroRecord;
  EXPECT_TRUE(db.FindGroup(1, &record));
  EXPECT_EQ(1, record.group_id);
  EXPECT_EQ(kManifestUrl, record.manifest_url);
  EXPECT_EQ(kOrigin, record.origin);

  record = kZeroRecord;
  EXPECT_TRUE(db.FindGroupForManifestUrl(kManifestUrl, &record));
  EXPECT_EQ(1, record.group_id);
  EXPECT_EQ(kManifestUrl, record.manifest_url);
  EXPECT_EQ(kOrigin, record.origin);

  record.group_id = 2;
  record.manifest_url = kOrigin;
  record.origin = kOrigin;
  EXPECT_TRUE(db.InsertGroup(&record));

  record = kZeroRecord;
  EXPECT_TRUE(db.FindGroupForManifestUrl(kOrigin, &record));
  EXPECT_EQ(2, record.group_id);
  EXPECT_EQ(kOrigin, record.manifest_url);
  EXPECT_EQ(kOrigin, record.origin);

  EXPECT_TRUE(db.FindGroupsForOrigin(kOrigin, &records));
  EXPECT_EQ(2U, records.size());
  EXPECT_EQ(1, records[0].group_id);
  EXPECT_EQ(kManifestUrl, records[0].manifest_url);
  EXPECT_EQ(kOrigin, records[0].origin);
  EXPECT_EQ(2, records[1].group_id);
  EXPECT_EQ(kOrigin, records[1].manifest_url);
  EXPECT_EQ(kOrigin, records[1].origin);

  EXPECT_TRUE(db.DeleteGroup(1));

  records.clear();
  EXPECT_TRUE(db.FindGroupsForOrigin(kOrigin, &records));
  EXPECT_EQ(1U, records.size());
  EXPECT_EQ(2, records[0].group_id);
  EXPECT_EQ(kOrigin, records[0].manifest_url);
  EXPECT_EQ(kOrigin, records[0].origin);

  std::set<GURL> origins;
  EXPECT_TRUE(db.FindOriginsWithGroups(&origins));
  EXPECT_EQ(1U, origins.size());
  EXPECT_EQ(kOrigin, *(origins.begin()));

  const GURL kManifest2("http://blah2/manifest");
  const GURL kOrigin2(kManifest2.GetOrigin());
  record.group_id = 1;
  record.manifest_url = kManifest2;
  record.origin = kOrigin2;
  EXPECT_TRUE(db.InsertGroup(&record));

  origins.clear();
  EXPECT_TRUE(db.FindOriginsWithGroups(&origins));
  EXPECT_EQ(2U, origins.size());
  EXPECT_TRUE(origins.end() != origins.find(kOrigin));
  EXPECT_TRUE(origins.end()  != origins.find(kOrigin2));

  AppCacheDatabase::CacheRecord cache_record;
  cache_record.cache_id = 1;
  cache_record.group_id = 1;
  cache_record.online_wildcard = true;
  cache_record.update_time = kZeroTimeTicks;
  EXPECT_TRUE(db.InsertCache(&cache_record));

  record = kZeroRecord;
  EXPECT_TRUE(db.FindGroupForCache(1, &record));
  EXPECT_EQ(1, record.group_id);
  EXPECT_EQ(kManifest2, record.manifest_url);
  EXPECT_EQ(kOrigin2, record.origin);
}

TEST(AppCacheDatabaseTest, FallbackNameSpaceRecords) {
  const FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  scoped_refptr<TestErrorDelegate> error_delegate(new TestErrorDelegate);
  db.db_->set_error_delegate(error_delegate);

  const GURL kFooNameSpace1("http://foo/namespace1");
  const GURL kFooNameSpace2("http://foo/namespace2");
  const GURL kFooFallbackEntry("http://foo/entry");
  const GURL kFooOrigin(kFooNameSpace1.GetOrigin());
  const GURL kBarNameSpace1("http://bar/namespace1");
  const GURL kBarNameSpace2("http://bar/namespace2");
  const GURL kBarFallbackEntry("http://bar/entry");
  const GURL kBarOrigin(kBarNameSpace1.GetOrigin());

  const AppCacheDatabase::FallbackNameSpaceRecord kZeroRecord;
  AppCacheDatabase::FallbackNameSpaceRecord record;
  std::vector<AppCacheDatabase::FallbackNameSpaceRecord> records;

  // Behavior with an empty table
  EXPECT_TRUE(db.FindFallbackNameSpacesForCache(1, &records));
  EXPECT_TRUE(records.empty());
  EXPECT_TRUE(db.FindFallbackNameSpacesForOrigin(kFooOrigin, &records));
  EXPECT_TRUE(records.empty());
  EXPECT_TRUE(db.DeleteFallbackNameSpacesForCache(1));

  // Two records for two differenent caches in the Foo origin.
  record.cache_id = 1;
  record.origin = kFooOrigin;
  record.namespace_url = kFooNameSpace1;
  record.fallback_entry_url = kFooFallbackEntry;
  EXPECT_TRUE(db.InsertFallbackNameSpace(&record));
  EXPECT_FALSE(db.InsertFallbackNameSpace(&record));

  record.cache_id = 2;
  record.origin = kFooOrigin;
  record.namespace_url = kFooNameSpace2;
  record.fallback_entry_url = kFooFallbackEntry;
  EXPECT_TRUE(db.InsertFallbackNameSpace(&record));

  records.clear();
  EXPECT_TRUE(db.FindFallbackNameSpacesForCache(1, &records));
  EXPECT_EQ(1U, records.size());
  EXPECT_EQ(1, records[0].cache_id);
  EXPECT_EQ(kFooOrigin, records[0].origin);
  EXPECT_EQ(kFooNameSpace1, records[0].namespace_url);
  EXPECT_EQ(kFooFallbackEntry, records[0].fallback_entry_url);

  records.clear();
  EXPECT_TRUE(db.FindFallbackNameSpacesForCache(2, &records));
  EXPECT_EQ(1U, records.size());
  EXPECT_EQ(2, records[0].cache_id);
  EXPECT_EQ(kFooOrigin, records[0].origin);
  EXPECT_EQ(kFooNameSpace2, records[0].namespace_url);
  EXPECT_EQ(kFooFallbackEntry, records[0].fallback_entry_url);

  records.clear();
  EXPECT_TRUE(db.FindFallbackNameSpacesForOrigin(kFooOrigin, &records));
  EXPECT_EQ(2U, records.size());
  EXPECT_EQ(1, records[0].cache_id);
  EXPECT_EQ(kFooOrigin, records[0].origin);
  EXPECT_EQ(kFooNameSpace1, records[0].namespace_url);
  EXPECT_EQ(kFooFallbackEntry, records[0].fallback_entry_url);
  EXPECT_EQ(2, records[1].cache_id);
  EXPECT_EQ(kFooOrigin, records[1].origin);
  EXPECT_EQ(kFooNameSpace2, records[1].namespace_url);
  EXPECT_EQ(kFooFallbackEntry, records[1].fallback_entry_url);

  EXPECT_TRUE(db.DeleteFallbackNameSpacesForCache(1));
  records.clear();
  EXPECT_TRUE(db.FindFallbackNameSpacesForOrigin(kFooOrigin, &records));
  EXPECT_EQ(1U, records.size());
  EXPECT_EQ(2, records[0].cache_id);
  EXPECT_EQ(kFooOrigin, records[0].origin);
  EXPECT_EQ(kFooNameSpace2, records[0].namespace_url);
  EXPECT_EQ(kFooFallbackEntry, records[0].fallback_entry_url);

  // Two more records for the same cache in the Bar origin.
  record.cache_id = 3;
  record.origin = kBarOrigin;
  record.namespace_url = kBarNameSpace1;
  record.fallback_entry_url = kBarFallbackEntry;
  EXPECT_TRUE(db.InsertFallbackNameSpace(&record));

  record.cache_id = 3;
  record.origin = kBarOrigin;
  record.namespace_url = kBarNameSpace2;
  record.fallback_entry_url = kBarFallbackEntry;
  EXPECT_TRUE(db.InsertFallbackNameSpace(&record));

  records.clear();
  EXPECT_TRUE(db.FindFallbackNameSpacesForCache(3, &records));
  EXPECT_EQ(2U, records.size());
  records.clear();
  EXPECT_TRUE(db.FindFallbackNameSpacesForOrigin(kBarOrigin, &records));
  EXPECT_EQ(2U, records.size());
}

TEST(AppCacheDatabaseTest, OnlineWhiteListRecords) {
  const FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  scoped_refptr<TestErrorDelegate> error_delegate(new TestErrorDelegate);
  db.db_->set_error_delegate(error_delegate);

  const GURL kFooNameSpace1("http://foo/namespace1");
  const GURL kFooNameSpace2("http://foo/namespace2");
  const GURL kBarNameSpace1("http://bar/namespace1");

  const AppCacheDatabase::OnlineWhiteListRecord kZeroRecord;
  AppCacheDatabase::OnlineWhiteListRecord record;
  std::vector<AppCacheDatabase::OnlineWhiteListRecord> records;

  // Behavior with an empty table
  EXPECT_TRUE(db.FindOnlineWhiteListForCache(1, &records));
  EXPECT_TRUE(records.empty());
  EXPECT_TRUE(db.DeleteOnlineWhiteListForCache(1));

  record.cache_id = 1;
  record.namespace_url = kFooNameSpace1;
  EXPECT_TRUE(db.InsertOnlineWhiteList(&record));
  record.namespace_url = kFooNameSpace2;
  EXPECT_TRUE(db.InsertOnlineWhiteList(&record));
  records.clear();
  EXPECT_TRUE(db.FindOnlineWhiteListForCache(1, &records));
  EXPECT_EQ(2U, records.size());
  EXPECT_EQ(1, records[0].cache_id);
  EXPECT_EQ(kFooNameSpace1, records[0].namespace_url);
  EXPECT_EQ(1, records[1].cache_id);
  EXPECT_EQ(kFooNameSpace2, records[1].namespace_url);

  record.cache_id = 2;
  record.namespace_url = kBarNameSpace1;
  EXPECT_TRUE(db.InsertOnlineWhiteList(&record));
  records.clear();
  EXPECT_TRUE(db.FindOnlineWhiteListForCache(2, &records));
  EXPECT_EQ(1U, records.size());

  EXPECT_TRUE(db.DeleteOnlineWhiteListForCache(1));
  records.clear();
  EXPECT_TRUE(db.FindOnlineWhiteListForCache(1, &records));
  EXPECT_TRUE(records.empty());
}

TEST(AppCacheDatabaseTest, DeletableResponseIds) {
  const FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  scoped_refptr<TestErrorDelegate> error_delegate(new TestErrorDelegate);
  db.db_->set_error_delegate(error_delegate);

  std::vector<int64> ids;

  EXPECT_TRUE(db.GetDeletableResponseIds(&ids, kint64max, 100));
  EXPECT_TRUE(ids.empty());
  ids.push_back(0);
  EXPECT_TRUE(db.DeleteDeletableResponseIds(ids));
  EXPECT_TRUE(db.InsertDeletableResponseIds(ids));

  ids.clear();
  EXPECT_TRUE(db.GetDeletableResponseIds(&ids, kint64max, 100));
  EXPECT_EQ(1U, ids.size());
  EXPECT_EQ(0, ids[0]);

  int64 unused, deleteable_response_rowid;
  unused = deleteable_response_rowid = 0;
  EXPECT_TRUE(db.FindLastStorageIds(&unused, &unused, &unused,
                                    &deleteable_response_rowid));
  EXPECT_EQ(1, deleteable_response_rowid);


  // Expected to fail due to the duplicate id, 0 is already in the table.
  ids.clear();
  ids.push_back(0);
  ids.push_back(1);
  EXPECT_FALSE(db.InsertDeletableResponseIds(ids));

  ids.clear();
  for (int i = 1; i < 10; ++i)
    ids.push_back(i);
  EXPECT_TRUE(db.InsertDeletableResponseIds(ids));
  EXPECT_TRUE(db.FindLastStorageIds(&unused, &unused, &unused,
                                    &deleteable_response_rowid));
  EXPECT_EQ(10, deleteable_response_rowid);

  ids.clear();
  EXPECT_TRUE(db.GetDeletableResponseIds(&ids, kint64max, 100));
  EXPECT_EQ(10U, ids.size());
  for (int i = 0; i < 10; ++i)
    EXPECT_EQ(i, ids[i]);

  // Ensure the limit is respected.
  ids.clear();
  EXPECT_TRUE(db.GetDeletableResponseIds(&ids, kint64max, 5));
  EXPECT_EQ(5U, ids.size());
  for (int i = 0; i < static_cast<int>(ids.size()); ++i)
    EXPECT_EQ(i, ids[i]);

  // Ensure the max_rowid is respected (the first rowid is 1).
  ids.clear();
  EXPECT_TRUE(db.GetDeletableResponseIds(&ids, 5, 100));
  EXPECT_EQ(5U, ids.size());
  for (int i = 0; i < static_cast<int>(ids.size()); ++i)
    EXPECT_EQ(i, ids[i]);

  // Ensure that we can delete from the table.
  EXPECT_TRUE(db.DeleteDeletableResponseIds(ids));
  ids.clear();
  EXPECT_TRUE(db.GetDeletableResponseIds(&ids, kint64max, 100));
  EXPECT_EQ(5U, ids.size());
  for (int i = 0; i < static_cast<int>(ids.size()); ++i)
    EXPECT_EQ(i + 5, ids[i]);
}

TEST(AppCacheDatabaseTest, Quotas) {
  const GURL kManifestUrl("http://blah/manifest");
  const GURL kManifestUrl2("http://blah/manifest2");
  const GURL kOrigin(kManifestUrl.GetOrigin());
  const GURL kOtherOriginManifestUrl("http://other/manifest");
  const GURL kOtherOrigin(kOtherOriginManifestUrl.GetOrigin());

  const FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  scoped_refptr<TestErrorDelegate> error_delegate(new TestErrorDelegate);
  db.db_->set_error_delegate(error_delegate);

  std::vector<AppCacheDatabase::CacheRecord> cache_records;
  EXPECT_EQ(db.GetDefaultOriginQuota(), db.GetOriginQuota(kOrigin));
  EXPECT_EQ(0, db.GetOriginUsage(kOrigin));
  EXPECT_TRUE(db.FindCachesForOrigin(kOrigin, &cache_records));
  EXPECT_TRUE(cache_records.empty());

  AppCacheDatabase::GroupRecord group_record;
  group_record.group_id = 1;
  group_record.manifest_url = kManifestUrl;
  group_record.origin = kOrigin;
  EXPECT_TRUE(db.InsertGroup(&group_record));
  AppCacheDatabase::CacheRecord cache_record;
  cache_record.cache_id = 1;
  cache_record.group_id = 1;
  cache_record.online_wildcard = true;
  cache_record.update_time = kZeroTimeTicks;
  cache_record.cache_size = 100;
  EXPECT_TRUE(db.InsertCache(&cache_record));

  EXPECT_EQ(100, db.GetOriginUsage(kOrigin));

  group_record.group_id = 2;
  group_record.manifest_url = kManifestUrl2;
  group_record.origin = kOrigin;
  EXPECT_TRUE(db.InsertGroup(&group_record));
  cache_record.cache_id = 2;
  cache_record.group_id = 2;
  cache_record.online_wildcard = true;
  cache_record.update_time = kZeroTimeTicks;
  cache_record.cache_size = 1000;
  EXPECT_TRUE(db.InsertCache(&cache_record));

  EXPECT_EQ(1100, db.GetOriginUsage(kOrigin));

  group_record.group_id = 3;
  group_record.manifest_url = kOtherOriginManifestUrl;
  group_record.origin = kOtherOrigin;
  EXPECT_TRUE(db.InsertGroup(&group_record));
  cache_record.cache_id = 3;
  cache_record.group_id = 3;
  cache_record.online_wildcard = true;
  cache_record.update_time = kZeroTimeTicks;
  cache_record.cache_size = 5000;
  EXPECT_TRUE(db.InsertCache(&cache_record));

  EXPECT_EQ(5000, db.GetOriginUsage(kOtherOrigin));

  EXPECT_TRUE(db.FindCachesForOrigin(kOrigin, &cache_records));
  EXPECT_EQ(2U, cache_records.size());
  cache_records.clear();
  EXPECT_TRUE(db.FindCachesForOrigin(kOtherOrigin, &cache_records));
  EXPECT_EQ(1U, cache_records.size());
}

}  // namespace appcache
