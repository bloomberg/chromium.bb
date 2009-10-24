// Copyright (c) 2009 The Chromium Authos. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_storage.h"
#include "webkit/appcache/mock_appcache_service.h"

namespace appcache {

class MockAppCacheStorageTest : public testing::Test {
 public:
  class MockStorageDelegate : public AppCacheStorage::Delegate {
   public:
    explicit MockStorageDelegate()
        : loaded_cache_id_(0), stored_group_success_(false),
          obsoleted_success_(false), found_cache_id_(kNoCacheId) {
    }

    void OnCacheLoaded(AppCache* cache, int64 cache_id) {
      loaded_cache_ = cache;
      loaded_cache_id_ = cache_id;
    }

    void OnGroupLoaded(AppCacheGroup* group, const GURL& manifest_url) {
      loaded_group_ = group;
      loaded_manifest_url_ = manifest_url;
    }

    void OnGroupAndNewestCacheStored(AppCacheGroup* group, bool success) {
      stored_group_ = group;
      stored_group_success_ = success;
    }

    void OnGroupMadeObsolete(AppCacheGroup* group, bool success) {
      obsoleted_group_ = group;
      obsoleted_success_ = success;
    }

    void OnMainResponseFound(const GURL& url, const AppCacheEntry& entry,
                             int64 cache_id, const GURL& manifest_url) {
      found_url_ = url;
      found_entry_ = entry;
      found_cache_id_ = cache_id;
      found_manifest_url_ = manifest_url;
    }

    scoped_refptr<AppCache> loaded_cache_;
    int64 loaded_cache_id_;
    scoped_refptr<AppCacheGroup> loaded_group_;
    GURL loaded_manifest_url_;
    scoped_refptr<AppCacheGroup> stored_group_;
    bool stored_group_success_;
    scoped_refptr<AppCacheGroup> obsoleted_group_;
    bool obsoleted_success_;
    GURL found_url_;
    AppCacheEntry found_entry_;
    int64 found_cache_id_;
    GURL found_manifest_url_;
  };
};


TEST_F(MockAppCacheStorageTest, LoadCache_Miss) {
  // Attempt to load a cache that doesn't exist. Should
  // complete asyncly.
  MockAppCacheService service;
  MockStorageDelegate delegate;
  service.storage()->LoadCache(111, &delegate);
  EXPECT_NE(111, delegate.loaded_cache_id_);
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(111, delegate.loaded_cache_id_);
  EXPECT_FALSE(delegate.loaded_cache_);
}

TEST_F(MockAppCacheStorageTest, LoadCache_NearHit) {
  // Attempt to load a cache that is currently in use
  // and does not require loading from disk. This
  // load should complete syncly.
  MockAppCacheService service;

  // Setup some preconditions. Make an 'unstored' cache for
  // us to load. The ctor should put it in the working set.
  int64 cache_id = service.storage()->NewCacheId();
  scoped_refptr<AppCache> cache = new AppCache(&service, cache_id);

  // Conduct the test.
  MockStorageDelegate delegate;
  service.storage()->LoadCache(cache_id, &delegate);
  EXPECT_EQ(cache_id, delegate.loaded_cache_id_);
  EXPECT_EQ(cache.get(), delegate.loaded_cache_.get());
}

TEST_F(MockAppCacheStorageTest, CreateGroup) {
  // Attempt to load/create a group that doesn't exist.
  // Should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());
  MockStorageDelegate delegate;
  GURL manifest_url("http://blah/");
  service.storage()->LoadOrCreateGroup(manifest_url, &delegate);
  EXPECT_NE(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_FALSE(delegate.loaded_group_.get());
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_TRUE(delegate.loaded_group_.get());
  EXPECT_TRUE(delegate.loaded_group_->HasOneRef());
  EXPECT_FALSE(delegate.loaded_group_->newest_complete_cache());
  EXPECT_TRUE(storage->stored_groups_.empty());
}

TEST_F(MockAppCacheStorageTest, LoadGroup_NearHit) {
  // Attempt to load a group that is currently in use
  // and does not require loading from disk. This
  // load should complete syncly.
  MockAppCacheService service;
  MockStorageDelegate delegate;

  // Setup some preconditions. Create a group that appears
  // to be "unstored" and "currently in use".
  GURL manifest_url("http://blah/");
  service.storage()->LoadOrCreateGroup(manifest_url, &delegate);
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_TRUE(delegate.loaded_group_.get());

  // Reset our delegate, and take a reference to the new group.
  scoped_refptr<AppCacheGroup> group;
  group.swap(delegate.loaded_group_);
  delegate.loaded_manifest_url_ = GURL();

  // Conduct the test.
  service.storage()->LoadOrCreateGroup(manifest_url, &delegate);
  EXPECT_EQ(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_EQ(group.get(), delegate.loaded_group_.get());
}

TEST_F(MockAppCacheStorageTest, LoadGroupAndCache_FarHit) {
  // Attempt to load a cache that is not currently in use
  // and does require loading from disk. This
  // load should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a group and newest cache that
  // appears to be "stored" and "not currently in use".
  GURL manifest_url("http://blah/");
  scoped_refptr<AppCacheGroup> group =
      new AppCacheGroup(&service, manifest_url);
  int64 cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> cache = new AppCache(&service, cache_id);
  cache->set_complete(true);
  group->AddCache(cache);
  storage->AddStoredGroup(group);
  storage->AddStoredCache(cache);

  // Drop the references from above so the only refs to these
  // objects are from within the storage class. This is to make
  // these objects appear as "not currently in use".
  AppCache* cache_ptr = cache.get();
  AppCacheGroup* group_ptr = group.get();
  cache = NULL;
  group = NULL;

  // Setup a delegate to receive completion callbacks.
  MockStorageDelegate delegate;

  // Conduct the cache load test.
  EXPECT_NE(cache_id, delegate.loaded_cache_id_);
  EXPECT_NE(cache_ptr, delegate.loaded_cache_.get());
  storage->LoadCache(cache_id, &delegate);
  EXPECT_NE(cache_id, delegate.loaded_cache_id_);
  EXPECT_NE(cache_ptr, delegate.loaded_cache_.get());
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(cache_id, delegate.loaded_cache_id_);
  EXPECT_EQ(cache_ptr, delegate.loaded_cache_.get());
  delegate.loaded_cache_ = NULL;

  // Conduct the group load test.
  EXPECT_NE(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_FALSE(delegate.loaded_group_.get());
  storage->LoadOrCreateGroup(manifest_url, &delegate);
  EXPECT_NE(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_FALSE(delegate.loaded_group_.get());
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_EQ(group_ptr, delegate.loaded_group_.get());
}

TEST_F(MockAppCacheStorageTest, StoreNewGroup) {
  // Store a group and its newest cache. Should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a group and newest cache that
  // appears to be "unstored".
  GURL manifest_url("http://blah/");
  scoped_refptr<AppCacheGroup> group =
      new AppCacheGroup(&service, manifest_url);
  int64 cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> cache = new AppCache(&service, cache_id);
  cache->set_complete(true);
  group->AddCache(cache);
  // Hold a ref to the cache simulate the UpdateJob holding that ref,
  // and hold a ref to the group to simulate the CacheHost holding that ref.

  // Conduct the store test.
  MockStorageDelegate delegate;
  EXPECT_TRUE(storage->stored_caches_.empty());
  EXPECT_TRUE(storage->stored_groups_.empty());
  storage->StoreGroupAndNewestCache(group, &delegate);
  EXPECT_FALSE(delegate.stored_group_success_);
  EXPECT_TRUE(storage->stored_caches_.empty());
  EXPECT_TRUE(storage->stored_groups_.empty());
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_TRUE(delegate.stored_group_success_);
  EXPECT_FALSE(storage->stored_caches_.empty());
  EXPECT_FALSE(storage->stored_groups_.empty());
}

TEST_F(MockAppCacheStorageTest, StoreExistingGroup) {
  // Store a group and its newest cache. Should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a group and old complete cache
  // that appear to be "stored", and a newest unstored complete cache.
  GURL manifest_url("http://blah/");
  scoped_refptr<AppCacheGroup> group =
      new AppCacheGroup(&service, manifest_url);
  int64 old_cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> old_cache = new AppCache(&service, old_cache_id);
  old_cache->set_complete(true);
  group->AddCache(old_cache);
  storage->AddStoredGroup(group);
  storage->AddStoredCache(old_cache);
  int64 new_cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> new_cache = new AppCache(&service, new_cache_id);
  new_cache->set_complete(true);
  group->AddCache(new_cache);
  EXPECT_EQ(new_cache.get(), group->newest_complete_cache());
  // Hold our refs to simulate the UpdateJob holding these refs.

  // Conduct the test.
  MockStorageDelegate delegate;
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  EXPECT_TRUE(storage->IsCacheStored(old_cache));
  EXPECT_FALSE(storage->IsCacheStored(new_cache));
  storage->StoreGroupAndNewestCache(group, &delegate);
  EXPECT_FALSE(delegate.stored_group_success_);
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  EXPECT_TRUE(storage->IsCacheStored(old_cache));
  EXPECT_FALSE(storage->IsCacheStored(new_cache));
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_TRUE(delegate.stored_group_success_);
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  EXPECT_FALSE(storage->IsCacheStored(old_cache));
  EXPECT_TRUE(storage->IsCacheStored(new_cache));
}

TEST_F(MockAppCacheStorageTest, MakeGroupObsolete) {
  // Make a group obsolete, should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a group and newest cache that
  // appears to be "stored" and "currently in use".
  GURL manifest_url("http://blah/");
  scoped_refptr<AppCacheGroup> group =
      new AppCacheGroup(&service, manifest_url);
  int64 cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> cache = new AppCache(&service, cache_id);
  cache->set_complete(true);
  group->AddCache(cache);
  storage->AddStoredGroup(group);
  storage->AddStoredCache(cache);
  // Hold our refs to simulate the UpdateJob holding these refs.

  // Conduct the test.
  MockStorageDelegate delegate;
  EXPECT_FALSE(group->is_obsolete());
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  EXPECT_FALSE(cache->HasOneRef());
  EXPECT_FALSE(group->HasOneRef());
  storage->MakeGroupObsolete(group, &delegate);
  EXPECT_FALSE(group->is_obsolete());
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  EXPECT_FALSE(cache->HasOneRef());
  EXPECT_FALSE(group->HasOneRef());
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_TRUE(delegate.obsoleted_success_);
  EXPECT_EQ(group.get(), delegate.obsoleted_group_.get());
  EXPECT_TRUE(group->is_obsolete());
  EXPECT_TRUE(storage->stored_caches_.empty());
  EXPECT_TRUE(storage->stored_groups_.empty());
  EXPECT_TRUE(cache->HasOneRef());
  EXPECT_FALSE(group->HasOneRef());
  delegate.obsoleted_group_ = NULL;
  cache = NULL;
  EXPECT_TRUE(group->HasOneRef());
}

TEST_F(MockAppCacheStorageTest, MarkEntryAsForeign) {
  // Should complete syncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a cache with an entry.
  GURL entry_url("http://blan/entry");
  int64 cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> cache = new AppCache(&service, cache_id);
  cache->AddEntry(entry_url, AppCacheEntry(AppCacheEntry::EXPLICIT));

  // Conduct the test.
  MockStorageDelegate delegate;
  EXPECT_FALSE(cache->GetEntry(entry_url)->IsForeign());
  storage->MarkEntryAsForeign(entry_url, cache_id);
  EXPECT_TRUE(cache->GetEntry(entry_url)->IsForeign());
  EXPECT_TRUE(cache->GetEntry(entry_url)->IsExplicit());
}

TEST_F(MockAppCacheStorageTest, FindNoMainResponse) {
  // Should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Conduct the test.
  MockStorageDelegate delegate;
  GURL url("http://blah/some_url");
  EXPECT_NE(url, delegate.found_url_);
  storage->FindResponseForMainRequest(url, &delegate);
  EXPECT_NE(url, delegate.found_url_);
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(url, delegate.found_url_);
  EXPECT_TRUE(delegate.found_manifest_url_.is_empty());
  EXPECT_EQ(kNoCacheId, delegate.found_cache_id_);
  EXPECT_EQ(kNoResponseId, delegate.found_entry_.response_id());
  EXPECT_EQ(0, delegate.found_entry_.types());
}

}  // namespace appcache

