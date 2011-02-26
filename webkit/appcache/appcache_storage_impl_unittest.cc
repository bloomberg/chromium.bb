// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stack>

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_database.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_policy.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/appcache_storage_impl.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"

namespace appcache {

namespace {

const base::Time kZeroTime;
const GURL kManifestUrl("http://blah/manifest");
const GURL kManifestUrl2("http://blah/manifest2");
const GURL kEntryUrl("http://blah/entry");
const GURL kEntryUrl2("http://blah/entry2");
const GURL kFallbackNamespace("http://blah/fallback_namespace/");
const GURL kFallbackNamespace2("http://blah/fallback_namespace/longer");
const GURL kFallbackTestUrl("http://blah/fallback_namespace/longer/test");
const GURL kOnlineNamespace("http://blah/online_namespace");
const GURL kOnlineNamespaceWithinFallback(
    "http://blah/fallback_namespace/online/");

// For the duration of this test case, we hijack the AppCacheThread API
// calls and implement them in terms of the io and db threads created here.

scoped_ptr<base::Thread> io_thread;
scoped_ptr<base::Thread> db_thread;

class TestThreadProvider : public SimpleAppCacheSystem::ThreadProvider {
 public:
  virtual bool PostTask(
      int id,
      const tracked_objects::Location& from_here,
      Task* task) {
    GetMessageLoop(id)->PostTask(from_here, task);
    return true;
  }

  virtual bool CurrentlyOn(int id) {
    return MessageLoop::current() == GetMessageLoop(id);
  }

  MessageLoop* GetMessageLoop(int id) {
    DCHECK(io_thread.get() && db_thread.get());
    if (id == SimpleAppCacheSystem::IO_THREAD_ID)
      return io_thread->message_loop();
    if (id == SimpleAppCacheSystem::DB_THREAD_ID)
      return db_thread->message_loop();
    NOTREACHED() << "Invalid AppCacheThreadID value";
    return NULL;
  }
};

TestThreadProvider thread_provider;

}  // namespace

class AppCacheStorageImplTest : public testing::Test {
 public:
  class MockStorageDelegate : public AppCacheStorage::Delegate {
   public:
    explicit MockStorageDelegate(AppCacheStorageImplTest* test)
        : loaded_cache_id_(0), stored_group_success_(false),
          would_exceed_quota_(false), obsoleted_success_(false),
          found_cache_id_(kNoCacheId), found_blocked_by_policy_(false),
          test_(test) {
    }

    void OnCacheLoaded(AppCache* cache, int64 cache_id) {
      loaded_cache_ = cache;
      loaded_cache_id_ = cache_id;
      test_->ScheduleNextTask();
    }

    void OnGroupLoaded(AppCacheGroup* group, const GURL& manifest_url) {
      loaded_group_ = group;
      loaded_manifest_url_ = manifest_url;
      loaded_groups_newest_cache_ = group ? group->newest_complete_cache()
                                          : NULL;
      test_->ScheduleNextTask();
    }

    void OnGroupAndNewestCacheStored(
        AppCacheGroup* group, AppCache* newest_cache, bool success,
        bool would_exceed_quota) {
      stored_group_ = group;
      stored_group_success_ = success;
      would_exceed_quota_ = would_exceed_quota;
      test_->ScheduleNextTask();
    }

    void OnGroupMadeObsolete(AppCacheGroup* group, bool success) {
      obsoleted_group_ = group;
      obsoleted_success_ = success;
      test_->ScheduleNextTask();
    }

    void OnMainResponseFound(const GURL& url, const AppCacheEntry& entry,
                             const GURL& fallback_url,
                             const AppCacheEntry& fallback_entry,
                             int64 cache_id, const GURL& manifest_url,
                             bool was_blocked_by_policy) {
      found_url_ = url;
      found_entry_ = entry;
      found_fallback_url_ = fallback_url;
      found_fallback_entry_ = fallback_entry;
      found_cache_id_ = cache_id;
      found_manifest_url_ = manifest_url;
      found_blocked_by_policy_ = was_blocked_by_policy;
      test_->ScheduleNextTask();
    }

    scoped_refptr<AppCache> loaded_cache_;
    int64 loaded_cache_id_;
    scoped_refptr<AppCacheGroup> loaded_group_;
    GURL loaded_manifest_url_;
    scoped_refptr<AppCache> loaded_groups_newest_cache_;
    scoped_refptr<AppCacheGroup> stored_group_;
    bool stored_group_success_;
    bool would_exceed_quota_;
    scoped_refptr<AppCacheGroup> obsoleted_group_;
    bool obsoleted_success_;
    GURL found_url_;
    AppCacheEntry found_entry_;
    GURL found_fallback_url_;
    AppCacheEntry found_fallback_entry_;
    int64 found_cache_id_;
    GURL found_manifest_url_;
    bool found_blocked_by_policy_;
    AppCacheStorageImplTest* test_;
  };

  class MockAppCachePolicy : public AppCachePolicy {
   public:
    explicit MockAppCachePolicy(AppCacheStorageImplTest* test)
        : can_load_return_value_(true), can_create_return_value_(0),
          callback_(NULL), test_(test) {
    }

    virtual bool CanLoadAppCache(const GURL& manifest_url) {
      requested_manifest_url_ = manifest_url;
      return can_load_return_value_;
    }

    virtual int CanCreateAppCache(const GURL& manifest_url,
                                  net::CompletionCallback* callback) {
      requested_manifest_url_ = manifest_url;
      callback_ = callback;
      if (can_create_return_value_ == net::ERR_IO_PENDING)
        test_->ScheduleNextTask();
      return can_create_return_value_;
    }

    bool can_load_return_value_;
    int can_create_return_value_;
    GURL requested_manifest_url_;
    net::CompletionCallback* callback_;
    AppCacheStorageImplTest* test_;
  };

  // Helper class run a test on our io_thread. The io_thread
  // is spun up once and reused for all tests.
  template <class Method>
  class WrapperTask : public Task {
   public:
    WrapperTask(AppCacheStorageImplTest* test, Method method)
        : test_(test), method_(method) {
    }

    virtual void Run() {
      test_->SetUpTest();

      // Ensure InitTask execution prior to conducting a test.
      test_->FlushDbThreadTasks();

      // We also have to wait for InitTask completion call to be performed
      // on the IO thread prior to running the test. Its guaranteed to be
      // queued by this time.
      MessageLoop::current()->PostTask(FROM_HERE,
          NewRunnableFunction(&RunMethod, test_, method_));
    }

    static void RunMethod(AppCacheStorageImplTest* test, Method method) {
      (test->*method)();
    }

   private:
    AppCacheStorageImplTest* test_;
    Method method_;
  };


  static void SetUpTestCase() {
    io_thread.reset(new base::Thread("AppCacheTest.IOThread"));
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    ASSERT_TRUE(io_thread->StartWithOptions(options));

    db_thread.reset(new base::Thread("AppCacheTest::DBThread"));
    ASSERT_TRUE(db_thread->Start());

    SimpleAppCacheSystem::set_thread_provider(&thread_provider);
  }

  static void TearDownTestCase() {
    SimpleAppCacheSystem::set_thread_provider(NULL);
    io_thread.reset(NULL);
    db_thread.reset(NULL);
  }

  // Test harness --------------------------------------------------

  AppCacheStorageImplTest()
      : ALLOW_THIS_IN_INITIALIZER_LIST(policy_(this)) {
  }

  template <class Method>
  void RunTestOnIOThread(Method method) {
    test_finished_event_ .reset(new base::WaitableEvent(false, false));
    io_thread->message_loop()->PostTask(
        FROM_HERE, new WrapperTask<Method>(this, method));
    test_finished_event_->Wait();
  }

  void SetUpTest() {
    DCHECK(MessageLoop::current() == io_thread->message_loop());
    service_.reset(new AppCacheService);
    service_->Initialize(FilePath(), NULL);
    delegate_.reset(new MockStorageDelegate(this));
  }

  void TearDownTest() {
    DCHECK(MessageLoop::current() == io_thread->message_loop());
    storage()->CancelDelegateCallbacks(delegate());
    group_ = NULL;
    cache_ = NULL;
    cache2_ = NULL;
    delegate_.reset();
    service_.reset();
    FlushDbThreadTasks();
  }

  void TestFinished() {
    // We unwind the stack prior to finishing up to let stack
    // based objects get deleted.
    DCHECK(MessageLoop::current() == io_thread->message_loop());
    MessageLoop::current()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &AppCacheStorageImplTest::TestFinishedUnwound));
  }

  void TestFinishedUnwound() {
    TearDownTest();
    test_finished_event_->Signal();
  }

  void PushNextTask(Task* task) {
    task_stack_.push(task);
  }

  void ScheduleNextTask() {
    DCHECK(MessageLoop::current() == io_thread->message_loop());
    if (task_stack_.empty()) {
      return;
    }
    MessageLoop::current()->PostTask(FROM_HERE, task_stack_.top());
    task_stack_.pop();
  }

  static void SignalEvent(base::WaitableEvent* event) {
    event->Signal();
  }

  void FlushDbThreadTasks() {
    // We pump a task thru the db thread to ensure any tasks previously
    // scheduled on that thread have been performed prior to return.
    base::WaitableEvent event(false, false);
    db_thread->message_loop()->PostTask(FROM_HERE,
        NewRunnableFunction(&AppCacheStorageImplTest::SignalEvent,
                            &event));
    event.Wait();
  }

  // LoadCache_Miss ----------------------------------------------------

  void LoadCache_Miss() {
    // Attempt to load a cache that doesn't exist. Should
    // complete asyncly.
    PushNextTask(NewRunnableMethod(
        this, &AppCacheStorageImplTest::Verify_LoadCache_Miss));

    storage()->LoadCache(111, delegate());
    EXPECT_NE(111, delegate()->loaded_cache_id_);
  }

  void Verify_LoadCache_Miss() {
    EXPECT_EQ(111, delegate()->loaded_cache_id_);
    EXPECT_FALSE(delegate()->loaded_cache_);
    TestFinished();
  }

  // LoadCache_NearHit -------------------------------------------------

  void LoadCache_NearHit() {
    // Attempt to load a cache that is currently in use
    // and does not require loading from storage. This
    // load should complete syncly.

    // Setup some preconditions. Make an 'unstored' cache for
    // us to load. The ctor should put it in the working set.
    int64 cache_id = storage()->NewCacheId();
    scoped_refptr<AppCache> cache(new AppCache(service(), cache_id));

    // Conduct the test.
    storage()->LoadCache(cache_id, delegate());
    EXPECT_EQ(cache_id, delegate()->loaded_cache_id_);
    EXPECT_EQ(cache.get(), delegate()->loaded_cache_.get());
    TestFinished();
  }

  // CreateGroup  --------------------------------------------

  void CreateGroupInEmptyOrigin() {
    // Attempt to load a group that doesn't exist, one should
    // be created for us, but not stored.

    // Since the origin has no groups, the storage class will respond
    // syncly.
    storage()->LoadOrCreateGroup(kManifestUrl, delegate());
    Verify_CreateGroup();
  }

  void CreateGroupInPopulatedOrigin() {
    // Attempt to load a group that doesn't exist, one should
    // be created for us, but not stored.
    PushNextTask(NewRunnableMethod(
       this, &AppCacheStorageImplTest::Verify_CreateGroup));

    // Since the origin has groups, storage class will have to
    // consult the database and completion will be async.
    storage()->origins_with_groups_.insert(kManifestUrl.GetOrigin());

    storage()->LoadOrCreateGroup(kManifestUrl, delegate());
    EXPECT_FALSE(delegate()->loaded_group_.get());
  }

  void Verify_CreateGroup() {
    EXPECT_EQ(kManifestUrl, delegate()->loaded_manifest_url_);
    EXPECT_TRUE(delegate()->loaded_group_.get());
    EXPECT_TRUE(delegate()->loaded_group_->HasOneRef());
    EXPECT_FALSE(delegate()->loaded_group_->newest_complete_cache());

    // Should not have been stored in the database.
    AppCacheDatabase::GroupRecord record;
    EXPECT_FALSE(database()->FindGroup(
        delegate()->loaded_group_->group_id(), &record));

    TestFinished();
  }

  // LoadGroupAndCache_FarHit  --------------------------------------

  void LoadGroupAndCache_FarHit() {
    // Attempt to load a cache that is not currently in use
    // and does require loading from disk. This
    // load should complete asyncly.
    PushNextTask(NewRunnableMethod(
       this, &AppCacheStorageImplTest::Verify_LoadCache_Far_Hit));

    // Setup some preconditions. Create a group and newest cache that
    // appear to be "stored" and "not currently in use".
    MakeCacheAndGroup(kManifestUrl, 1, 1, true);
    group_ = NULL;
    cache_ = NULL;

    // Conduct the cache load test, completes async
    storage()->LoadCache(1, delegate());
  }

  void Verify_LoadCache_Far_Hit() {
    EXPECT_TRUE(delegate()->loaded_cache_);
    EXPECT_TRUE(delegate()->loaded_cache_->HasOneRef());
    EXPECT_EQ(1, delegate()->loaded_cache_id_);

    // The group should also have been loaded.
    EXPECT_TRUE(delegate()->loaded_cache_->owning_group());
    EXPECT_TRUE(delegate()->loaded_cache_->owning_group()->HasOneRef());
    EXPECT_EQ(1, delegate()->loaded_cache_->owning_group()->group_id());

    // Drop things from the working set.
    delegate()->loaded_cache_ = NULL;
    EXPECT_FALSE(delegate()->loaded_group_);

    // Conduct the group load test, also complete asyncly.
    PushNextTask(NewRunnableMethod(
       this, &AppCacheStorageImplTest::Verify_LoadGroup_Far_Hit));

    storage()->LoadOrCreateGroup(kManifestUrl, delegate());
  }

  void Verify_LoadGroup_Far_Hit() {
    EXPECT_TRUE(delegate()->loaded_group_);
    EXPECT_EQ(kManifestUrl, delegate()->loaded_manifest_url_);
    EXPECT_TRUE(delegate()->loaded_group_->newest_complete_cache());
    delegate()->loaded_groups_newest_cache_ = NULL;
    EXPECT_TRUE(delegate()->loaded_group_->HasOneRef());
    TestFinished();
  }

  // StoreNewGroup  --------------------------------------

  void StoreNewGroup() {
    // Store a group and its newest cache. Should complete asyncly.
    PushNextTask(NewRunnableMethod(
       this, &AppCacheStorageImplTest::Verify_StoreNewGroup));

    // Setup some preconditions. Create a group and newest cache that
    // appear to be "unstored".
    group_ = new AppCacheGroup(
        service(), kManifestUrl, storage()->NewGroupId());
    cache_ = new AppCache(service(), storage()->NewCacheId());
    // Hold a ref to the cache simulate the UpdateJob holding that ref,
    // and hold a ref to the group to simulate the CacheHost holding that ref.

    // Conduct the store test.
    storage()->StoreGroupAndNewestCache(group_, cache_, delegate());
    EXPECT_FALSE(delegate()->stored_group_success_);
  }

  void Verify_StoreNewGroup() {
    EXPECT_TRUE(delegate()->stored_group_success_);
    EXPECT_EQ(group_.get(), delegate()->stored_group_.get());
    EXPECT_EQ(cache_.get(), group_->newest_complete_cache());
    EXPECT_TRUE(cache_->is_complete());

    // Should have been stored in the database.
    AppCacheDatabase::GroupRecord group_record;
    AppCacheDatabase::CacheRecord cache_record;
    EXPECT_TRUE(database()->FindGroup(group_->group_id(), &group_record));
    EXPECT_TRUE(database()->FindCache(cache_->cache_id(), &cache_record));
    TestFinished();
  }

  // StoreExistingGroup  --------------------------------------

  void StoreExistingGroup() {
    // Store a group and its newest cache. Should complete asyncly.
    PushNextTask(NewRunnableMethod(
       this, &AppCacheStorageImplTest::Verify_StoreExistingGroup));

    // Setup some preconditions. Create a group and old complete cache
    // that appear to be "stored"
    MakeCacheAndGroup(kManifestUrl, 1, 1, true);

    // And a newest unstored complete cache.
    cache2_ = new AppCache(service(), 2);

    // Conduct the test.
    storage()->StoreGroupAndNewestCache(group_, cache2_, delegate());
    EXPECT_FALSE(delegate()->stored_group_success_);
  }

  void Verify_StoreExistingGroup() {
    EXPECT_TRUE(delegate()->stored_group_success_);
    EXPECT_EQ(group_.get(), delegate()->stored_group_.get());
    EXPECT_EQ(cache2_.get(), group_->newest_complete_cache());
    EXPECT_TRUE(cache2_->is_complete());

    // The new cache should have been stored in the database.
    AppCacheDatabase::GroupRecord group_record;
    AppCacheDatabase::CacheRecord cache_record;
    EXPECT_TRUE(database()->FindGroup(1, &group_record));
    EXPECT_TRUE(database()->FindCache(2, &cache_record));

    // The old cache should have been deleted
    EXPECT_FALSE(database()->FindCache(1, &cache_record));
    TestFinished();
  }

  // StoreExistingGroupExistingCache  -------------------------------

  void StoreExistingGroupExistingCache() {
    // Store a group with updates to its existing newest complete cache.
    // Setup some preconditions. Create a group and a complete cache that
    // appear to be "stored".

    // Setup some preconditions. Create a group and old complete cache
    // that appear to be "stored"
    MakeCacheAndGroup(kManifestUrl, 1, 1, true);

    // Change the cache.
    base::Time now = base::Time::Now();
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::MASTER, 1, 100));
    cache_->set_update_time(now);

    PushNextTask(NewRunnableMethod(
       this, &AppCacheStorageImplTest::Verify_StoreExistingGroupExistingCache,
       now));

    // Conduct the test.
    EXPECT_EQ(cache_, group_->newest_complete_cache());
    storage()->StoreGroupAndNewestCache(group_, cache_, delegate());
    EXPECT_FALSE(delegate()->stored_group_success_);
  }

  void Verify_StoreExistingGroupExistingCache(
      base::Time expected_update_time) {
    EXPECT_TRUE(delegate()->stored_group_success_);
    EXPECT_EQ(cache_, group_->newest_complete_cache());

    AppCacheDatabase::CacheRecord cache_record;
    EXPECT_TRUE(database()->FindCache(1, &cache_record));
    EXPECT_EQ(1, cache_record.cache_id);
    EXPECT_EQ(1, cache_record.group_id);
    EXPECT_FALSE(cache_record.online_wildcard);
    EXPECT_TRUE(expected_update_time == cache_record.update_time);
    EXPECT_EQ(100, cache_record.cache_size);

    std::vector<AppCacheDatabase::EntryRecord> entry_records;
    EXPECT_TRUE(database()->FindEntriesForCache(1, &entry_records));
    EXPECT_EQ(1U, entry_records.size());
    EXPECT_EQ(1 , entry_records[0].cache_id);
    EXPECT_EQ(kEntryUrl, entry_records[0].url);
    EXPECT_EQ(AppCacheEntry::MASTER, entry_records[0].flags);
    EXPECT_EQ(1, entry_records[0].response_id);
    EXPECT_EQ(100, entry_records[0].response_size);

    TestFinished();
  }

  // FailStoreGroup  --------------------------------------

  void FailStoreGroup() {
    // Store a group and its newest cache. Should complete asyncly.
    PushNextTask(NewRunnableMethod(
       this, &AppCacheStorageImplTest::Verify_FailStoreGroup));

    // Setup some preconditions. Create a group and newest cache that
    // appear to be "unstored" and big enough to exceed the 5M limit.
    const int64 kTooBig = 10 * 1024 * 1024;  // 10M
    group_ = new AppCacheGroup(
        service(), kManifestUrl, storage()->NewGroupId());
    cache_ = new AppCache(service(), storage()->NewCacheId());
    cache_->AddEntry(kManifestUrl,
                     AppCacheEntry(AppCacheEntry::MANIFEST, 1, kTooBig));
    // Hold a ref to the cache simulate the UpdateJob holding that ref,
    // and hold a ref to the group to simulate the CacheHost holding that ref.

    // Conduct the store test.
    storage()->StoreGroupAndNewestCache(group_, cache_, delegate());
    EXPECT_FALSE(delegate()->stored_group_success_);  // Expected to be async.
  }

  void Verify_FailStoreGroup() {
    EXPECT_FALSE(delegate()->stored_group_success_);
    EXPECT_TRUE(delegate()->would_exceed_quota_);

    // Should not have been stored in the database.
    AppCacheDatabase::GroupRecord group_record;
    AppCacheDatabase::CacheRecord cache_record;
    EXPECT_FALSE(database()->FindGroup(group_->group_id(), &group_record));
    EXPECT_FALSE(database()->FindCache(cache_->cache_id(), &cache_record));

    TestFinished();
  }

  // MakeGroupObsolete  -------------------------------

  void MakeGroupObsolete() {
    // Make a group obsolete, should complete asyncly.
    PushNextTask(NewRunnableMethod(
       this, &AppCacheStorageImplTest::Verify_MakeGroupObsolete));

    // Setup some preconditions. Create a group and newest cache that
    // appears to be "stored" and "currently in use".
    MakeCacheAndGroup(kManifestUrl, 1, 1, true);
    EXPECT_FALSE(storage()->origins_with_groups_.empty());

    // Also insert some related records.
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 1;
    entry_record.flags = AppCacheEntry::FALLBACK;
    entry_record.response_id = 1;
    entry_record.url = kEntryUrl;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));

    AppCacheDatabase::FallbackNameSpaceRecord fallback_namespace_record;
    fallback_namespace_record.cache_id = 1;
    fallback_namespace_record.fallback_entry_url = kEntryUrl;
    fallback_namespace_record.namespace_url = kFallbackNamespace;
    fallback_namespace_record.origin = kManifestUrl.GetOrigin();
    EXPECT_TRUE(
        database()->InsertFallbackNameSpace(&fallback_namespace_record));

    AppCacheDatabase::OnlineWhiteListRecord online_whitelist_record;
    online_whitelist_record.cache_id = 1;
    online_whitelist_record.namespace_url = kOnlineNamespace;
    EXPECT_TRUE(database()->InsertOnlineWhiteList(&online_whitelist_record));

    // Conduct the test.
    storage()->MakeGroupObsolete(group_, delegate());
    EXPECT_FALSE(group_->is_obsolete());
  }

  void Verify_MakeGroupObsolete() {
    EXPECT_TRUE(delegate()->obsoleted_success_);
    EXPECT_EQ(group_.get(), delegate()->obsoleted_group_.get());
    EXPECT_TRUE(group_->is_obsolete());
    EXPECT_TRUE(storage()->origins_with_groups_.empty());

    // The cache and group have been deleted from the database.
    AppCacheDatabase::GroupRecord group_record;
    AppCacheDatabase::CacheRecord cache_record;
    EXPECT_FALSE(database()->FindGroup(1, &group_record));
    EXPECT_FALSE(database()->FindCache(1, &cache_record));

    // The related records should have been deleted too.
    std::vector<AppCacheDatabase::EntryRecord> entry_records;
    database()->FindEntriesForCache(1, &entry_records);
    EXPECT_TRUE(entry_records.empty());
    std::vector<AppCacheDatabase::FallbackNameSpaceRecord> fallback_records;
    database()->FindFallbackNameSpacesForCache(1, &fallback_records);
    EXPECT_TRUE(fallback_records.empty());
    std::vector<AppCacheDatabase::OnlineWhiteListRecord> whitelist_records;
    database()->FindOnlineWhiteListForCache(1, &whitelist_records);
    EXPECT_TRUE(whitelist_records.empty());

    TestFinished();
  }

  // MarkEntryAsForeign  -------------------------------

  void MarkEntryAsForeign() {
    // Setup some preconditions. Create a cache with an entry
    // in storage and in the working set.
    MakeCacheAndGroup(kManifestUrl, 1, 1, true);
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT));
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 1;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT;
    entry_record.response_id = 0;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));
    EXPECT_FALSE(cache_->GetEntry(kEntryUrl)->IsForeign());

    // Conduct the test.
    storage()->MarkEntryAsForeign(kEntryUrl, 1);

    // The entry in the working set should have been updated syncly.
    EXPECT_TRUE(cache_->GetEntry(kEntryUrl)->IsForeign());
    EXPECT_TRUE(cache_->GetEntry(kEntryUrl)->IsExplicit());

    // And the entry in storage should also be updated, but that
    // happens asyncly on the db thread.
    FlushDbThreadTasks();
    AppCacheDatabase::EntryRecord entry_record2;
    EXPECT_TRUE(database()->FindEntry(1, kEntryUrl, &entry_record2));
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::FOREIGN,
              entry_record2.flags);
    TestFinished();
  }

  // MarkEntryAsForeignWithLoadInProgress  -------------------------------

  void MarkEntryAsForeignWithLoadInProgress() {
    PushNextTask(NewRunnableMethod(this,
       &AppCacheStorageImplTest::Verify_MarkEntryAsForeignWithLoadInProgress));

    // Setup some preconditions. Create a cache with an entry
    // in storage, but not in the working set.
    MakeCacheAndGroup(kManifestUrl, 1, 1, true);
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT));
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 1;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT;
    entry_record.response_id = 0;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));
    EXPECT_FALSE(cache_->GetEntry(kEntryUrl)->IsForeign());
    EXPECT_TRUE(cache_->HasOneRef());
    cache_ = NULL;
    group_ = NULL;

    // Conduct the test, start a cache load, and prior to completion
    // of that load, mark the entry as foreign.
    storage()->LoadCache(1, delegate());
    storage()->MarkEntryAsForeign(kEntryUrl, 1);
  }

  void Verify_MarkEntryAsForeignWithLoadInProgress() {
    EXPECT_EQ(1, delegate()->loaded_cache_id_);
    EXPECT_TRUE(delegate()->loaded_cache_.get());

    // The entry in the working set should have been updated upon load.
    EXPECT_TRUE(delegate()->loaded_cache_->GetEntry(kEntryUrl)->IsForeign());
    EXPECT_TRUE(delegate()->loaded_cache_->GetEntry(kEntryUrl)->IsExplicit());

    // And the entry in storage should also be updated.
    FlushDbThreadTasks();
    AppCacheDatabase::EntryRecord entry_record;
    EXPECT_TRUE(database()->FindEntry(1, kEntryUrl, &entry_record));
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::FOREIGN,
              entry_record.flags);
    TestFinished();
  }

  // FindNoMainResponse  -------------------------------

  void FindNoMainResponse() {
    PushNextTask(NewRunnableMethod(
       this, &AppCacheStorageImplTest::Verify_FindNoMainResponse));

    // Conduct the test.
    storage()->FindResponseForMainRequest(kEntryUrl, delegate());
    EXPECT_NE(kEntryUrl, delegate()->found_url_);
  }

  void Verify_FindNoMainResponse() {
    EXPECT_EQ(kEntryUrl, delegate()->found_url_);
    // If the request was blocked by a policy, the manifest url is still valid.
    EXPECT_TRUE(delegate()->found_manifest_url_.is_empty() ||
                delegate()->found_blocked_by_policy_);
    EXPECT_EQ(kNoCacheId, delegate()->found_cache_id_);
    EXPECT_EQ(kNoResponseId, delegate()->found_entry_.response_id());
    EXPECT_EQ(kNoResponseId, delegate()->found_fallback_entry_.response_id());
    EXPECT_TRUE(delegate()->found_fallback_url_.is_empty());
    EXPECT_EQ(0, delegate()->found_entry_.types());
    EXPECT_EQ(0, delegate()->found_fallback_entry_.types());
    TestFinished();
  }

  // BasicFindMainResponse  -------------------------------

  void BasicFindMainResponseInDatabase() {
    BasicFindMainResponse(true, false);
  }

  void BasicFindMainResponseInWorkingSet() {
    BasicFindMainResponse(false, false);
  }

  void BlockFindMainResponseWithPolicyCheck() {
    BasicFindMainResponse(true, true);
  }

  void BasicFindMainResponse(bool drop_from_working_set,
                             bool block_with_policy_check) {
    PushNextTask(NewRunnableMethod(
       this, &AppCacheStorageImplTest::Verify_BasicFindMainResponse));

    policy_.can_load_return_value_ = !block_with_policy_check;
    service()->set_appcache_policy(&policy_);

    // Setup some preconditions. Create a complete cache with an entry
    // in storage.
    MakeCacheAndGroup(kManifestUrl, 1, 1, true);
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT, 1));
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 1;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT;
    entry_record.response_id = 1;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));

    // Optionally drop the cache/group pair from the working set.
    if (drop_from_working_set) {
      EXPECT_TRUE(cache_->HasOneRef());
      cache_ = NULL;
      EXPECT_TRUE(group_->HasOneRef());
      group_ = NULL;
    }

    // Conduct the test.
    storage()->FindResponseForMainRequest(kEntryUrl, delegate());
    EXPECT_NE(kEntryUrl,  delegate()->found_url_);
  }

  void Verify_BasicFindMainResponse() {
    EXPECT_EQ(kManifestUrl, policy_.requested_manifest_url_);
    if (policy_.can_load_return_value_) {
      EXPECT_EQ(kEntryUrl, delegate()->found_url_);
      EXPECT_EQ(kManifestUrl, delegate()->found_manifest_url_);
      EXPECT_FALSE(delegate()->found_blocked_by_policy_);
      EXPECT_EQ(1, delegate()->found_cache_id_);
      EXPECT_EQ(1, delegate()->found_entry_.response_id());
      EXPECT_TRUE(delegate()->found_entry_.IsExplicit());
      EXPECT_FALSE(delegate()->found_fallback_entry_.has_response_id());
      TestFinished();
    } else {
      Verify_FindNoMainResponse();
    }
  }

  // BasicFindMainFallbackResponse  -------------------------------

  void BasicFindMainFallbackResponseInDatabase() {
    BasicFindMainFallbackResponse(true);
  }

  void BasicFindMainFallbackResponseInWorkingSet() {
    BasicFindMainFallbackResponse(false);
  }

  void BasicFindMainFallbackResponse(bool drop_from_working_set) {
    PushNextTask(NewRunnableMethod(
       this, &AppCacheStorageImplTest::Verify_BasicFindMainFallbackResponse));

    // Setup some preconditions. Create a complete cache with a
    // fallback namespace and entry.
    MakeCacheAndGroup(kManifestUrl, 1, 1, true);
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::FALLBACK, 1));
    cache_->AddEntry(kEntryUrl2, AppCacheEntry(AppCacheEntry::FALLBACK, 2));
    cache_->fallback_namespaces_.push_back(
        FallbackNamespace(kFallbackNamespace2, kEntryUrl2));
    cache_->fallback_namespaces_.push_back(
        FallbackNamespace(kFallbackNamespace, kEntryUrl));
    AppCacheDatabase::CacheRecord cache_record;
    std::vector<AppCacheDatabase::EntryRecord> entries;
    std::vector<AppCacheDatabase::FallbackNameSpaceRecord> fallbacks;
    std::vector<AppCacheDatabase::OnlineWhiteListRecord> whitelists;
    cache_->ToDatabaseRecords(group_,
        &cache_record, &entries, &fallbacks, &whitelists);
    EXPECT_TRUE(database()->InsertEntryRecords(entries));
    EXPECT_TRUE(database()->InsertFallbackNameSpaceRecords(fallbacks));
    EXPECT_TRUE(database()->InsertOnlineWhiteListRecords(whitelists));
    if (drop_from_working_set) {
      EXPECT_TRUE(cache_->HasOneRef());
      cache_ = NULL;
      EXPECT_TRUE(group_->HasOneRef());
      group_ = NULL;
    }

    // Conduct the test. The test url is in both fallback namespace urls,
    // but should match the longer of the two.
    storage()->FindResponseForMainRequest(kFallbackTestUrl, delegate());
    EXPECT_NE(kFallbackTestUrl, delegate()->found_url_);
  }

  void Verify_BasicFindMainFallbackResponse() {
    EXPECT_EQ(kFallbackTestUrl, delegate()->found_url_);
    EXPECT_EQ(kManifestUrl, delegate()->found_manifest_url_);
    EXPECT_FALSE(delegate()->found_blocked_by_policy_);
    EXPECT_EQ(1, delegate()->found_cache_id_);
    EXPECT_FALSE(delegate()->found_entry_.has_response_id());
    EXPECT_EQ(2, delegate()->found_fallback_entry_.response_id());
    EXPECT_EQ(kEntryUrl2, delegate()->found_fallback_url_);
    EXPECT_TRUE(delegate()->found_fallback_entry_.IsFallback());
    TestFinished();
  }

  // FindMainResponseWithMultipleHits  -------------------------------

  void FindMainResponseWithMultipleHits() {
    PushNextTask(NewRunnableMethod(this,
        &AppCacheStorageImplTest::Verify_FindMainResponseWithMultipleHits));

    // Setup some preconditions. Create 2 complete caches with an entry
    // for the same url.

    // The first cache, in the database but not in the working set.
    MakeCacheAndGroup(kManifestUrl, 1, 1, true);
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT, 1));
    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 1;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT;
    entry_record.response_id = 1;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));
    cache_ = NULL;
    group_ = NULL;

    // The second cache, in the database and working set.
    MakeCacheAndGroup(kManifestUrl2, 2, 2, true);
    cache_->AddEntry(kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT, 2));
    entry_record.cache_id = 2;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT;
    entry_record.response_id = 2;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));

    // Conduct the test, we should find the response from the second cache
    // since it's "in use".
    storage()->FindResponseForMainRequest(kEntryUrl, delegate());
    EXPECT_NE(kEntryUrl, delegate()->found_url_);
  }

  void Verify_FindMainResponseWithMultipleHits() {
    EXPECT_EQ(kEntryUrl, delegate()->found_url_);
    EXPECT_EQ(kManifestUrl2, delegate()->found_manifest_url_);
    EXPECT_FALSE(delegate()->found_blocked_by_policy_);
    EXPECT_EQ(2, delegate()->found_cache_id_);
    EXPECT_EQ(2, delegate()->found_entry_.response_id());
    EXPECT_TRUE(delegate()->found_entry_.IsExplicit());
    EXPECT_FALSE(delegate()->found_fallback_entry_.has_response_id());
    TestFinished();
  }

  // FindMainResponseExclusions  -------------------------------

  void FindMainResponseExclusionsInDatabase() {
    FindMainResponseExclusions(true);
  }

  void FindMainResponseExclusionsInWorkingSet() {
    FindMainResponseExclusions(false);
  }

  void FindMainResponseExclusions(bool drop_from_working_set) {
    // Setup some preconditions. Create a complete cache with a
    // foreign entry, an online namespace, and a second online
    // namespace nested within a fallback namespace.
    MakeCacheAndGroup(kManifestUrl, 1, 1, true);
    cache_->AddEntry(kEntryUrl,
        AppCacheEntry(AppCacheEntry::EXPLICIT | AppCacheEntry::FOREIGN, 1));
    cache_->online_whitelist_namespaces_.push_back(kOnlineNamespace);
    cache_->AddEntry(kEntryUrl2, AppCacheEntry(AppCacheEntry::FALLBACK, 2));
    cache_->fallback_namespaces_.push_back(
        FallbackNamespace(kFallbackNamespace, kEntryUrl2));
    cache_->online_whitelist_namespaces_.push_back(kOnlineNamespace);
    cache_->online_whitelist_namespaces_.push_back(
        kOnlineNamespaceWithinFallback);

    AppCacheDatabase::EntryRecord entry_record;
    entry_record.cache_id = 1;
    entry_record.url = kEntryUrl;
    entry_record.flags = AppCacheEntry::EXPLICIT | AppCacheEntry::FOREIGN;
    entry_record.response_id = 1;
    EXPECT_TRUE(database()->InsertEntry(&entry_record));
    AppCacheDatabase::OnlineWhiteListRecord whitelist_record;
    whitelist_record.cache_id = 1;
    whitelist_record.namespace_url = kOnlineNamespace;
    EXPECT_TRUE(database()->InsertOnlineWhiteList(&whitelist_record));
    AppCacheDatabase::FallbackNameSpaceRecord fallback_namespace_record;
    fallback_namespace_record.cache_id = 1;
    fallback_namespace_record.fallback_entry_url = kEntryUrl2;
    fallback_namespace_record.namespace_url = kFallbackNamespace;
    fallback_namespace_record.origin = kManifestUrl.GetOrigin();
    EXPECT_TRUE(
        database()->InsertFallbackNameSpace(&fallback_namespace_record));
    whitelist_record.cache_id = 1;
    whitelist_record.namespace_url = kOnlineNamespaceWithinFallback;
    EXPECT_TRUE(database()->InsertOnlineWhiteList(&whitelist_record));
    if (drop_from_working_set) {
      cache_ = NULL;
      group_ = NULL;
    }

    // We should not find anything for the foreign entry.
    PushNextTask(NewRunnableMethod(
        this, &AppCacheStorageImplTest::Verify_ExclusionNotFound,
        kEntryUrl, 1));
    storage()->FindResponseForMainRequest(kEntryUrl, delegate());
  }

  void Verify_ExclusionNotFound(GURL expected_url, int phase) {
    EXPECT_EQ(expected_url, delegate()->found_url_);
    EXPECT_TRUE(delegate()->found_manifest_url_.is_empty());
    EXPECT_FALSE(delegate()->found_blocked_by_policy_);
    EXPECT_EQ(kNoCacheId, delegate()->found_cache_id_);
    EXPECT_EQ(kNoResponseId, delegate()->found_entry_.response_id());
    EXPECT_EQ(kNoResponseId, delegate()->found_fallback_entry_.response_id());
    EXPECT_TRUE(delegate()->found_fallback_url_.is_empty());
    EXPECT_EQ(0, delegate()->found_entry_.types());
    EXPECT_EQ(0, delegate()->found_fallback_entry_.types());

    if (phase == 1) {
      // We should not find anything for the online namespace.
      PushNextTask(NewRunnableMethod(this,
          &AppCacheStorageImplTest::Verify_ExclusionNotFound,
          kOnlineNamespace, 2));
      storage()->FindResponseForMainRequest(kOnlineNamespace, delegate());
      return;
    }
    if (phase == 2) {
      // We should not find anything for the online namespace nested within
      // the fallback namespace.
      PushNextTask(NewRunnableMethod(this,
          &AppCacheStorageImplTest::Verify_ExclusionNotFound,
          kOnlineNamespaceWithinFallback, 3));
      storage()->FindResponseForMainRequest(
          kOnlineNamespaceWithinFallback, delegate());
      return;
    }

    TestFinished();
  }

  // Test case helpers --------------------------------------------------

  AppCacheService* service() {
    return service_.get();
  }

  AppCacheStorageImpl* storage() {
    return static_cast<AppCacheStorageImpl*>(service()->storage());
  }

  AppCacheDatabase* database() {
    return storage()->database_;
  }

  MockStorageDelegate* delegate() {
    return delegate_.get();
  }

  void MakeCacheAndGroup(
      const GURL& manifest_url, int64 group_id, int64 cache_id,
      bool add_to_database) {
    group_ = new AppCacheGroup(service(), manifest_url, group_id);
    cache_ = new AppCache(service(), cache_id);
    cache_->set_complete(true);
    group_->AddCache(cache_);
    if (add_to_database) {
      AppCacheDatabase::GroupRecord group_record;
      group_record.group_id = group_id;
      group_record.manifest_url = manifest_url;
      group_record.origin = manifest_url.GetOrigin();
      EXPECT_TRUE(database()->InsertGroup(&group_record));
      AppCacheDatabase::CacheRecord cache_record;
      cache_record.cache_id = cache_id;
      cache_record.group_id = group_id;
      cache_record.online_wildcard = false;
      cache_record.update_time = kZeroTime;
      EXPECT_TRUE(database()->InsertCache(&cache_record));
      storage()->origins_with_groups_.insert(manifest_url.GetOrigin());
    }
  }

  // Data members --------------------------------------------------

  scoped_ptr<base::WaitableEvent> test_finished_event_;
  std::stack<Task*> task_stack_;
  MockAppCachePolicy policy_;
  scoped_ptr<AppCacheService> service_;
  scoped_ptr<MockStorageDelegate> delegate_;
  scoped_refptr<AppCacheGroup> group_;
  scoped_refptr<AppCache> cache_;
  scoped_refptr<AppCache> cache2_;
};


TEST_F(AppCacheStorageImplTest, LoadCache_Miss) {
  RunTestOnIOThread(&AppCacheStorageImplTest::LoadCache_Miss);
}

TEST_F(AppCacheStorageImplTest, LoadCache_NearHit) {
  RunTestOnIOThread(&AppCacheStorageImplTest::LoadCache_NearHit);
}

TEST_F(AppCacheStorageImplTest, CreateGroupInEmptyOrigin) {
  RunTestOnIOThread(&AppCacheStorageImplTest::CreateGroupInEmptyOrigin);
}

TEST_F(AppCacheStorageImplTest, CreateGroupInPopulatedOrigin) {
  RunTestOnIOThread(&AppCacheStorageImplTest::CreateGroupInPopulatedOrigin);
}

TEST_F(AppCacheStorageImplTest, LoadGroupAndCache_FarHit) {
  RunTestOnIOThread(&AppCacheStorageImplTest::LoadGroupAndCache_FarHit);
}

TEST_F(AppCacheStorageImplTest, StoreNewGroup) {
  RunTestOnIOThread(&AppCacheStorageImplTest::StoreNewGroup);
}

TEST_F(AppCacheStorageImplTest, StoreExistingGroup) {
  RunTestOnIOThread(&AppCacheStorageImplTest::StoreExistingGroup);
}

TEST_F(AppCacheStorageImplTest, StoreExistingGroupExistingCache) {
  RunTestOnIOThread(&AppCacheStorageImplTest::StoreExistingGroupExistingCache);
}

TEST_F(AppCacheStorageImplTest, FailStoreGroup) {
  RunTestOnIOThread(&AppCacheStorageImplTest::FailStoreGroup);
}

TEST_F(AppCacheStorageImplTest, MakeGroupObsolete) {
  RunTestOnIOThread(&AppCacheStorageImplTest::MakeGroupObsolete);
}

TEST_F(AppCacheStorageImplTest, MarkEntryAsForeign) {
  RunTestOnIOThread(&AppCacheStorageImplTest::MarkEntryAsForeign);
}

TEST_F(AppCacheStorageImplTest, MarkEntryAsForeignWithLoadInProgress) {
  RunTestOnIOThread(
      &AppCacheStorageImplTest::MarkEntryAsForeignWithLoadInProgress);
}

TEST_F(AppCacheStorageImplTest, FindNoMainResponse) {
  RunTestOnIOThread(&AppCacheStorageImplTest::FindNoMainResponse);
}

TEST_F(AppCacheStorageImplTest, BasicFindMainResponseInDatabase) {
  RunTestOnIOThread(
      &AppCacheStorageImplTest::BasicFindMainResponseInDatabase);
}

TEST_F(AppCacheStorageImplTest, BasicFindMainResponseInWorkingSet) {
  RunTestOnIOThread(
      &AppCacheStorageImplTest::BasicFindMainResponseInWorkingSet);
}

TEST_F(AppCacheStorageImplTest, BlockFindMainResponseWithPolicyCheck) {
  RunTestOnIOThread(
      &AppCacheStorageImplTest::BlockFindMainResponseWithPolicyCheck);
}

TEST_F(AppCacheStorageImplTest, BasicFindMainFallbackResponseInDatabase) {
  RunTestOnIOThread(
      &AppCacheStorageImplTest::BasicFindMainFallbackResponseInDatabase);
}

TEST_F(AppCacheStorageImplTest, BasicFindMainFallbackResponseInWorkingSet) {
  RunTestOnIOThread(
      &AppCacheStorageImplTest::BasicFindMainFallbackResponseInWorkingSet);
}

TEST_F(AppCacheStorageImplTest, FindMainResponseWithMultipleHits) {
  RunTestOnIOThread(
      &AppCacheStorageImplTest::FindMainResponseWithMultipleHits);
}

TEST_F(AppCacheStorageImplTest, FindMainResponseExclusionsInDatabase) {
  RunTestOnIOThread(
      &AppCacheStorageImplTest::FindMainResponseExclusionsInDatabase);
}

TEST_F(AppCacheStorageImplTest, FindMainResponseExclusionsInWorkingSet) {
  RunTestOnIOThread(
      &AppCacheStorageImplTest::FindMainResponseExclusionsInWorkingSet);
}

// That's all folks!

}  // namespace appcache

// AppCacheStorageImplTest is expected to always live longer than the
// runnable methods.  This lets us call NewRunnableMethod on its instances.
DISABLE_RUNNABLE_METHOD_REFCOUNT(appcache::AppCacheStorageImplTest);
