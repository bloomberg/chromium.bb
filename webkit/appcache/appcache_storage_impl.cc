// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_storage_impl.h"

#include "app/sql/connection.h"
#include "app/sql/transaction.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_database.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_thread.h"

namespace appcache {

static const char kAppCacheDatabaseName[] = "Index";
static const char kDiskCacheDirectoryName[] = "Cache";
static const int kMaxDiskCacheSize = 10 * 1024 * 1024;

// DatabaseTask -----------------------------------------

class AppCacheStorageImpl::DatabaseTask
    : public base::RefCountedThreadSafe<DatabaseTask> {
 public:
  explicit DatabaseTask(AppCacheStorageImpl* storage)
      : storage_(storage), database_(storage->database_) {}

  virtual ~DatabaseTask() {}

  void AddDelegate(DelegateReference* delegate_reference) {
    delegates_.push_back(delegate_reference);
  }

  // Schedules a task to be Run() on the DB thread. Tasks
  // are run in the order in which they are scheduled.
  void Schedule();

  // Called on the DB thread.
  virtual void Run() = 0;

  // Called on the IO thread after Run() has completed.
  virtual void RunCompleted() {}

  // Once scheduled a task cannot be cancelled, but the
  // call to RunCompleted may be. This method should only be
  // called on the IO thread. This is used by AppCacheStorageImpl
  // to cancel the completion calls when AppCacheStorageImpl is
  // destructed. This method may be overriden to release or delete
  // additional data associated with the task that is not DB thread
  // safe. If overriden, this base class method must be called from
  // within the override.
  virtual void CancelCompletion();

 protected:
  AppCacheStorageImpl* storage_;
  AppCacheDatabase* database_;
  DelegateReferenceVector delegates_;

 private:
  void CallRun();
  void CallRunCompleted();
};

void AppCacheStorageImpl::DatabaseTask::Schedule() {
  DCHECK(storage_);
  DCHECK(AppCacheThread::CurrentlyOn(AppCacheThread::io()));
  if (AppCacheThread::PostTask(AppCacheThread::db(), FROM_HERE,
          NewRunnableMethod(this, &DatabaseTask::CallRun))) {
    storage_->scheduled_database_tasks_.push_back(this);
  } else {
    NOTREACHED() << "The database thread is not running.";
  }
}

void AppCacheStorageImpl::DatabaseTask::CancelCompletion() {
  DCHECK(AppCacheThread::CurrentlyOn(AppCacheThread::io()));
  delegates_.clear();
  storage_ = NULL;
}

void AppCacheStorageImpl::DatabaseTask::CallRun() {
  DCHECK(AppCacheThread::CurrentlyOn(AppCacheThread::db()));
  Run();
  AppCacheThread::PostTask(AppCacheThread::io(), FROM_HERE,
      NewRunnableMethod(this, &DatabaseTask::CallRunCompleted));
}

void AppCacheStorageImpl::DatabaseTask::CallRunCompleted() {
  if (storage_) {
    DCHECK(AppCacheThread::CurrentlyOn(AppCacheThread::io()));
    DCHECK(storage_->scheduled_database_tasks_.front() == this);
    storage_->scheduled_database_tasks_.pop_front();
    RunCompleted();
  }
}

// InitTask -------

class AppCacheStorageImpl::InitTask : public DatabaseTask {
 public:
  explicit InitTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage), last_group_id_(0),
        last_cache_id_(0), last_response_id_(0) {}

  virtual void Run();
  virtual void RunCompleted();

  int64 last_group_id_;
  int64 last_cache_id_;
  int64 last_response_id_;
  std::set<GURL> origins_with_groups_;
};

void AppCacheStorageImpl::InitTask::Run() {
  database_->FindLastStorageIds(
      &last_group_id_, &last_cache_id_, &last_response_id_);
  database_->FindOriginsWithGroups(&origins_with_groups_);
}

void AppCacheStorageImpl::InitTask::RunCompleted() {
  storage_->last_group_id_ = last_group_id_;
  storage_->last_cache_id_ = last_cache_id_;
  storage_->last_response_id_ = last_response_id_;
  storage_->origins_with_groups_.swap(origins_with_groups_);
}

// CloseConnectionTask -------

class AppCacheStorageImpl::CloseConnectionTask : public DatabaseTask {
 public:
  explicit CloseConnectionTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage) {}

  virtual void Run() { database_->CloseConnection(); }
};

// StoreOrLoadTask -------

class AppCacheStorageImpl::StoreOrLoadTask : public DatabaseTask {
 protected:
  explicit StoreOrLoadTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage) {}

  bool FindRelatedCacheRecords(int64 cache_id);
  void CreateCacheAndGroupFromRecords(
      scoped_refptr<AppCache>* cache, scoped_refptr<AppCacheGroup>* group);

  AppCacheDatabase::GroupRecord group_record_;
  AppCacheDatabase::CacheRecord cache_record_;
  std::vector<AppCacheDatabase::EntryRecord> entry_records_;
  std::vector<AppCacheDatabase::FallbackNameSpaceRecord>
      fallback_namespace_records_;
  std::vector<AppCacheDatabase::OnlineWhiteListRecord>
      online_whitelist_records_;
};

bool AppCacheStorageImpl::StoreOrLoadTask::FindRelatedCacheRecords(
    int64 cache_id) {
  return database_->FindEntriesForCache(cache_id, &entry_records_) &&
         database_->FindFallbackNameSpacesForCache(
             cache_id, &fallback_namespace_records_) &&
         database_->FindOnlineWhiteListForCache(
             cache_id, &online_whitelist_records_);
}

void AppCacheStorageImpl::StoreOrLoadTask::CreateCacheAndGroupFromRecords(
    scoped_refptr<AppCache>* cache, scoped_refptr<AppCacheGroup>* group) {
  DCHECK(storage_ && cache && group);

  (*cache) = new AppCache(storage_->service_, cache_record_.cache_id);
  cache->get()->InitializeWithDatabaseRecords(
      cache_record_, entry_records_, fallback_namespace_records_,
      online_whitelist_records_);
  cache->get()->set_complete(true);

  (*group) = storage_->working_set_.GetGroup(group_record_.manifest_url);
  if (group->get()) {
    DCHECK(group_record_.group_id == group->get()->group_id());
    group->get()->AddCache(cache->get());
  } else {
    (*group) = new AppCacheGroup(
        storage_->service_, group_record_.manifest_url,
        group_record_.group_id);
    group->get()->AddCache(cache->get());
  }
  DCHECK(group->get()->newest_complete_cache() == cache->get());

  // We have to update foriegn entries if MarkEntryAsForeignTasks
  // are in flight.
  std::vector<GURL> urls;
  storage_->GetPendingForeignMarkingsForCache(cache->get()->cache_id(), &urls);
  for (std::vector<GURL>::iterator iter = urls.begin();
       iter != urls.end(); ++iter) {
    DCHECK(cache->get()->GetEntry(*iter));
    cache->get()->GetEntry(*iter)->add_types(AppCacheEntry::FOREIGN);
  }
}

// CacheLoadTask -------

class AppCacheStorageImpl::CacheLoadTask : public StoreOrLoadTask {
 public:
  CacheLoadTask(int64 cache_id, AppCacheStorageImpl* storage)
      : StoreOrLoadTask(storage), cache_id_(cache_id),
        success_(false) {}

  virtual void Run();
  virtual void RunCompleted();

  int64 cache_id_;
  bool success_;
};

void AppCacheStorageImpl::CacheLoadTask::Run() {
  success_ =
      database_->FindCache(cache_id_, &cache_record_) &&
      database_->FindGroup(cache_record_.group_id, &group_record_) &&
      FindRelatedCacheRecords(cache_id_);
}

void AppCacheStorageImpl::CacheLoadTask::RunCompleted() {
  storage_->pending_cache_loads_.erase(cache_id_);
  scoped_refptr<AppCache> cache;
  scoped_refptr<AppCacheGroup> group;
  if (success_) {
    DCHECK(cache_record_.cache_id == cache_id_);
    DCHECK(!storage_->working_set_.GetCache(cache_record_.cache_id));
    CreateCacheAndGroupFromRecords(&cache, &group);
  }
  FOR_EACH_DELEGATE(delegates_, OnCacheLoaded(cache, cache_id_));
}

// GroupLoadTask -------

class AppCacheStorageImpl::GroupLoadTask : public StoreOrLoadTask {
 public:
  GroupLoadTask(GURL manifest_url, AppCacheStorageImpl* storage)
      : StoreOrLoadTask(storage), manifest_url_(manifest_url),
        success_(false) {}

  virtual void Run();
  virtual void RunCompleted();

  GURL manifest_url_;
  bool success_;
};

void AppCacheStorageImpl::GroupLoadTask::Run() {
  success_ =
      database_->FindGroupForManifestUrl(manifest_url_, &group_record_) &&
      database_->FindCacheForGroup(group_record_.group_id, &cache_record_) &&
      FindRelatedCacheRecords(cache_record_.cache_id);
}

void AppCacheStorageImpl::GroupLoadTask::RunCompleted() {
  storage_->pending_group_loads_.erase(manifest_url_);
  scoped_refptr<AppCacheGroup> group;
  scoped_refptr<AppCache> cache;
  if (success_) {
    DCHECK(group_record_.manifest_url == manifest_url_);
    DCHECK(!storage_->working_set_.GetGroup(manifest_url_));
    DCHECK(!storage_->working_set_.GetCache(cache_record_.cache_id));
    CreateCacheAndGroupFromRecords(&cache, &group);
  } else {
    group = new AppCacheGroup(
        storage_->service_, manifest_url_,
        storage_->NewGroupId());
  }
  FOR_EACH_DELEGATE(delegates_, OnGroupLoaded(group, manifest_url_));
}

// StoreGroupAndCacheTask -------

class AppCacheStorageImpl::StoreGroupAndCacheTask : public StoreOrLoadTask {
 public:
  StoreGroupAndCacheTask(AppCacheStorageImpl* storage, AppCacheGroup* group,
                         AppCache* newest_cache);

  virtual void Run();
  virtual void RunCompleted();
  virtual void CancelCompletion();

  scoped_refptr<AppCacheGroup> group_;
  scoped_refptr<AppCache> cache_;
  bool success_;
};

AppCacheStorageImpl::StoreGroupAndCacheTask::StoreGroupAndCacheTask(
    AppCacheStorageImpl* storage, AppCacheGroup* group, AppCache* newest_cache)
    : StoreOrLoadTask(storage), group_(group), cache_(newest_cache),
      success_(false) {
  group_record_.group_id = group->group_id();
  group_record_.manifest_url = group->manifest_url();
  group_record_.origin = group_record_.manifest_url.GetOrigin();
  newest_cache->ToDatabaseRecords(
      group,
      &cache_record_, &entry_records_, &fallback_namespace_records_,
      &online_whitelist_records_);
}

void AppCacheStorageImpl::StoreGroupAndCacheTask::Run() {
  DCHECK(!success_);
  sql::Connection* connection = database_->db_connection();
  if (!connection)
    return;

  sql::Transaction transaction(connection);
  if (!transaction.Begin())
    return;

  AppCacheDatabase::GroupRecord existing_group;
  success_ = database_->FindGroup(group_record_.group_id, &existing_group);
  if (!success_) {
    success_ = database_->InsertGroup(&group_record_);
  } else {
    DCHECK(group_record_.group_id == existing_group.group_id);
    DCHECK(group_record_.manifest_url == existing_group.manifest_url);
    DCHECK(group_record_.origin == existing_group.origin);

    AppCacheDatabase::CacheRecord cache;
    if (database_->FindCacheForGroup(group_record_.group_id, &cache)) {
      success_ =
          database_->DeleteCache(cache.cache_id) &&
          database_->DeleteEntriesForCache(cache.cache_id) &&
          database_->DeleteFallbackNameSpacesForCache(cache.cache_id) &&
          database_->DeleteOnlineWhiteListForCache(cache.cache_id);
      // TODO(michaeln): schedule to purge unused responses from the disk cache
    } else {
      NOTREACHED() << "A existing group without a cache is unexpected";
    }
  }

  success_ =
      success_ &&
      database_->InsertCache(&cache_record_) &&
      database_->InsertEntryRecords(entry_records_) &&
      database_->InsertFallbackNameSpaceRecords(fallback_namespace_records_)&&
      database_->InsertOnlineWhiteListRecords(online_whitelist_records_) &&
      transaction.Commit();
}

void AppCacheStorageImpl::StoreGroupAndCacheTask::RunCompleted() {
  if (success_) {
    storage_->origins_with_groups_.insert(group_->manifest_url().GetOrigin());
    if (cache_ != group_->newest_complete_cache())
      group_->AddCache(cache_);
  }
  FOR_EACH_DELEGATE(delegates_, OnGroupAndNewestCacheStored(group_, success_));
  group_ = NULL;
  cache_ = NULL;
}

void AppCacheStorageImpl::StoreGroupAndCacheTask::CancelCompletion() {
  // Overriden to safely drop our reference to the group and cache
  // which are not thread safe refcounted.
  DatabaseTask::CancelCompletion();
  group_ = NULL;
  cache_ = NULL;
}

// FindMainResponseTask -------

class AppCacheStorageImpl::FindMainResponseTask : public DatabaseTask {
 public:
  FindMainResponseTask(AppCacheStorageImpl* storage, const GURL& url,
                       const AppCacheWorkingSet::GroupMap* groups_in_use)
      : DatabaseTask(storage), url_(url), cache_id_(kNoCacheId) {
    if (groups_in_use) {
      for (AppCacheWorkingSet::GroupMap::const_iterator it =
               groups_in_use->begin();
           it != groups_in_use->end(); ++it) {
        AppCacheGroup* group = it->second;
        AppCache* cache = group->newest_complete_cache();
        if (group->is_obsolete() || !cache)
          continue;
        cache_ids_in_use_.insert(cache->cache_id());
      }
    }
  }

  virtual void Run();
  virtual void RunCompleted();

  GURL url_;
  std::set<int64> cache_ids_in_use_;
  AppCacheEntry entry_;
  AppCacheEntry fallback_entry_;
  int64 cache_id_;
  GURL manifest_url_;
};

namespace {

bool SortByLength(
    const AppCacheDatabase::FallbackNameSpaceRecord& lhs,
    const AppCacheDatabase::FallbackNameSpaceRecord& rhs) {
  return lhs.namespace_url.spec().length() > rhs.namespace_url.spec().length();
}

}

void AppCacheStorageImpl::FindMainResponseTask::Run() {
  // We have a bias for hits from caches that are in use.

  // TODO(michaeln): The heuristics around choosing amoungst
  // multiple candidates is under specified, and just plain
  // not fully understood. Refine these over time. In particular,
  // * prefer candidates from newer caches
  // * take into account the cache associated with the document
  //   that initiated the navigation
  // * take into account the cache associated with the document
  //   currently residing in the frame being navigated

  // First look for an exact match. We don't worry about whether
  // the containing cache is in-use in this loop because the
  // storage class's FindResponseForMainRequest method does that
  // as a pre-optimization.
  std::vector<AppCacheDatabase::EntryRecord> entries;
  if (database_->FindEntriesForUrl(url_, &entries) && !entries.empty()) {
    std::vector<AppCacheDatabase::EntryRecord>::iterator iter;
    for (iter = entries.begin(); iter < entries.end(); ++iter) {
      if (iter->flags & AppCacheEntry::FOREIGN)
        continue;

      AppCacheDatabase::GroupRecord group_record;
      if (!database_->FindGroupForCache(iter->cache_id, &group_record)) {
        NOTREACHED() << "A cache without a group is not expected.";
        continue;
      }
      entry_ = AppCacheEntry(iter->flags, iter->response_id);
      cache_id_ = iter->cache_id;
      manifest_url_ = group_record.manifest_url;
      return;
    }
  }

  // No exact matches, look at the fallback namespaces for this origin.
  std::vector<AppCacheDatabase::FallbackNameSpaceRecord> fallbacks;
  if (!database_->FindFallbackNameSpacesForOrigin(url_.GetOrigin(), &fallbacks)
      || fallbacks.empty()) {
    return;
  }

  // Sort by namespace url string length, longest to shortest,
  // since longer matches trump when matching a url to a namespace.
  std::sort(fallbacks.begin(), fallbacks.end(), SortByLength);

  bool has_candidate = false;
  GURL candidate_fallback_namespace;
  std::vector<AppCacheDatabase::FallbackNameSpaceRecord>::iterator iter;
  for (iter = fallbacks.begin(); iter < fallbacks.end(); ++iter) {
    if (has_candidate &&
        (candidate_fallback_namespace.spec().length() >
         iter->namespace_url.spec().length())) {
      break;  // Stop iterating since longer namespace prefix matches win.
    }

    if (StartsWithASCII(url_.spec(), iter->namespace_url.spec(), true)) {
      bool is_cache_in_use = cache_ids_in_use_.find(iter->cache_id) !=
                             cache_ids_in_use_.end();

      bool take_new_candidate = !has_candidate || is_cache_in_use;

      AppCacheDatabase::EntryRecord entry_record;
      if (take_new_candidate &&
          database_->FindEntry(iter->cache_id, iter->fallback_entry_url,
                               &entry_record)) {
        AppCacheDatabase::GroupRecord group_record;
        if (!database_->FindGroupForCache(iter->cache_id, &group_record)) {
          NOTREACHED() << "A cache without a group is not expected.";
          continue;
        }
        cache_id_ = iter->cache_id;
        manifest_url_ = group_record.manifest_url;
        fallback_entry_ = AppCacheEntry(
            entry_record.flags, entry_record.response_id);
        if (is_cache_in_use)
          break;  // Stop iterating since we favor hits from in-use caches.
        candidate_fallback_namespace = iter->namespace_url;
        has_candidate = true;
      }
    }
  }
}

void AppCacheStorageImpl::FindMainResponseTask::RunCompleted() {
  FOR_EACH_DELEGATE(delegates_,
      OnMainResponseFound(url_, entry_, fallback_entry_,
                          cache_id_, manifest_url_));
}

// MarkEntryAsForeignTask -------

class AppCacheStorageImpl::MarkEntryAsForeignTask : public DatabaseTask {
 public:
  MarkEntryAsForeignTask(
      AppCacheStorageImpl* storage, const GURL& url, int64 cache_id)
      : DatabaseTask(storage), cache_id_(cache_id), entry_url_(url) {}

  virtual void Run();
  virtual void RunCompleted();

  int64 cache_id_;
  GURL entry_url_;
};

void AppCacheStorageImpl::MarkEntryAsForeignTask::Run() {
  database_->AddEntryFlags(entry_url_, cache_id_, AppCacheEntry::FOREIGN);
}

void AppCacheStorageImpl::MarkEntryAsForeignTask::RunCompleted() {
  DCHECK(storage_->pending_foreign_markings_.front().first == entry_url_ &&
         storage_->pending_foreign_markings_.front().second == cache_id_);
  storage_->pending_foreign_markings_.pop_front();
}

// MakeGroupObsoleteTask -------

class AppCacheStorageImpl::MakeGroupObsoleteTask : public DatabaseTask {
 public:
  MakeGroupObsoleteTask(AppCacheStorageImpl* storage, AppCacheGroup* group);

  virtual void Run();
  virtual void RunCompleted();
  virtual void CancelCompletion();

  scoped_refptr<AppCacheGroup> group_;
  int64 group_id_;
  bool success_;
  std::set<GURL> origins_with_groups_;
};

AppCacheStorageImpl::MakeGroupObsoleteTask::MakeGroupObsoleteTask(
    AppCacheStorageImpl* storage, AppCacheGroup* group)
    : DatabaseTask(storage), group_(group), group_id_(group->group_id()),
      success_(false) {
}

void AppCacheStorageImpl::MakeGroupObsoleteTask::Run() {
  DCHECK(!success_);
  sql::Connection* connection = database_->db_connection();
  if (!connection)
    return;

  sql::Transaction transaction(connection);
  if (!transaction.Begin())
    return;

  AppCacheDatabase::GroupRecord group_record;
  if (!database_->FindGroup(group_id_, &group_record)) {
    // This group doesn't exists in the database, nothing todo here.
    success_ = true;
    return;
  }

  AppCacheDatabase::CacheRecord cache_record;
  if (database_->FindCacheForGroup(group_id_, &cache_record)) {
    success_ =
        database_->DeleteGroup(group_id_) &&
        database_->DeleteCache(cache_record.cache_id) &&
        database_->DeleteEntriesForCache(cache_record.cache_id) &&
        database_->DeleteFallbackNameSpacesForCache(cache_record.cache_id) &&
        database_->DeleteOnlineWhiteListForCache(cache_record.cache_id);
  } else {
    NOTREACHED() << "A existing group without a cache is unexpected";
    success_ = database_->DeleteGroup(group_id_);
  }

  success_ = success_ &&
             database_->FindOriginsWithGroups(&origins_with_groups_) &&
             transaction.Commit();

  // TODO(michaeln): schedule to purge unused responses from the disk cache
}

void AppCacheStorageImpl::MakeGroupObsoleteTask::RunCompleted() {
  if (success_) {
    storage_->origins_with_groups_.swap(origins_with_groups_);
    group_->set_obsolete(true);

    // Also remove from the working set, caches for an 'obsolete' group
    // may linger in use, but the group itself cannot be looked up by
    // 'manifest_url' in the working set any longer.
    storage_->working_set()->RemoveGroup(group_);
  }
  FOR_EACH_DELEGATE(delegates_, OnGroupMadeObsolete(group_, success_));
  group_ = NULL;
}

void AppCacheStorageImpl::MakeGroupObsoleteTask::CancelCompletion() {
  // Overriden to safely drop our reference to the group
  // which is not thread safe refcounted.
  DatabaseTask::CancelCompletion();
  group_ = NULL;
}


// AppCacheStorageImpl ---------------------------------------------------

AppCacheStorageImpl::AppCacheStorageImpl(AppCacheService* service)
    : AppCacheStorage(service), is_incognito_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

AppCacheStorageImpl::~AppCacheStorageImpl() {
  STLDeleteElements(&pending_simple_tasks_);

  std::for_each(scheduled_database_tasks_.begin(),
                scheduled_database_tasks_.end(),
                std::mem_fun(&DatabaseTask::CancelCompletion));

  if (database_)
    AppCacheThread::DeleteSoon(AppCacheThread::db(), FROM_HERE, database_);
}

void AppCacheStorageImpl::Initialize(const FilePath& cache_directory) {
  // TODO(michaeln): until purging of responses is addressed in some way,
  // always use incognito mode which doesn't put anything to disk.
  // Uncomment the following line when responses are dealt with.
  // cache_directory_ = cache_directory;
  is_incognito_ = cache_directory_.empty();

  FilePath db_file_path;
  if (!is_incognito_)
    db_file_path = cache_directory.AppendASCII(kAppCacheDatabaseName);
  database_ = new AppCacheDatabase(db_file_path);

  scoped_refptr<InitTask> task = new InitTask(this);
  task->Schedule();
}

void AppCacheStorageImpl::LoadCache(int64 id, Delegate* delegate) {
  DCHECK(delegate);
  AppCache* cache = working_set_.GetCache(id);
  if (cache) {
    delegate->OnCacheLoaded(cache, id);
    return;
  }
  scoped_refptr<CacheLoadTask> task = GetPendingCacheLoadTask(id);
  if (task) {
    task->AddDelegate(GetOrCreateDelegateReference(delegate));
    return;
  }
  task = new CacheLoadTask(id, this);
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->Schedule();
  pending_cache_loads_[id] = task;
}

void AppCacheStorageImpl::LoadOrCreateGroup(
    const GURL& manifest_url, Delegate* delegate) {
  DCHECK(delegate);
  AppCacheGroup* group = working_set_.GetGroup(manifest_url);
  if (group) {
    delegate->OnGroupLoaded(group, manifest_url);
    return;
  }

  scoped_refptr<GroupLoadTask> task = GetPendingGroupLoadTask(manifest_url);
  if (task) {
    task->AddDelegate(GetOrCreateDelegateReference(delegate));
    return;
  }

  if (origins_with_groups_.find(manifest_url.GetOrigin()) ==
      origins_with_groups_.end()) {
    // No need to query the database, return NULL immediately.
    scoped_refptr<AppCacheGroup> group = new AppCacheGroup(
        service_, manifest_url, NewGroupId());
    delegate->OnGroupLoaded(group, manifest_url);
    return;
  }

  task = new GroupLoadTask(manifest_url, this);
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->Schedule();
  pending_group_loads_[manifest_url] = task.get();
}

void AppCacheStorageImpl::StoreGroupAndNewestCache(
    AppCacheGroup* group, AppCache* newest_cache, Delegate* delegate) {
  // TODO(michaeln): distinguish between a simple update of an existing
  // cache that just adds new master entry(s), and the insertion of a
  // whole new cache. The StoreGroupAndCacheTask as written will handle
  // the simple update case in a very heavy weight way (delete all and
  // the reinsert all over again).
  DCHECK(group && delegate);
  DCHECK(newest_cache && newest_cache->is_complete());
  scoped_refptr<StoreGroupAndCacheTask> task =
      new StoreGroupAndCacheTask(this, group, newest_cache);
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->Schedule();
}

void AppCacheStorageImpl::FindResponseForMainRequest(
    const GURL& url, Delegate* delegate) {
  DCHECK(delegate);

  const GURL* url_ptr = &url;
  GURL url_no_ref;
  if (url.has_ref()) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    url_no_ref = url.ReplaceComponents(replacements);
    url_ptr = &url_no_ref;
  }

  const GURL origin = url.GetOrigin();

  // First look in our working set for a direct hit without having to query
  // the database.
  const AppCacheWorkingSet::GroupMap* groups_in_use =
      working_set()->GetGroupsInOrigin(origin);
  if (groups_in_use) {
    for (AppCacheWorkingSet::GroupMap::const_iterator it =
            groups_in_use->begin();
         it != groups_in_use->end(); ++it) {
      AppCacheGroup* group = it->second;
      AppCache* cache = group->newest_complete_cache();
      if (group->is_obsolete() || !cache)
        continue;

      AppCacheEntry* entry = cache->GetEntry(*url_ptr);
      if (entry && !entry->IsForeign()) {
        ScheduleSimpleTask(method_factory_.NewRunnableMethod(
            &AppCacheStorageImpl::DeliverShortCircuitedFindMainResponse,
            url, *entry, make_scoped_refptr(group), make_scoped_refptr(cache),
            make_scoped_refptr(GetOrCreateDelegateReference(delegate))));
        return;
      }
    }
  }

  if (IsInitTaskComplete() &&
      origins_with_groups_.find(origin) == origins_with_groups_.end()) {
    // No need to query the database, return async'ly but without going thru
    // the DB thread.
    scoped_refptr<AppCacheGroup> no_group;
    scoped_refptr<AppCache> no_cache;
    ScheduleSimpleTask(method_factory_.NewRunnableMethod(
        &AppCacheStorageImpl::DeliverShortCircuitedFindMainResponse,
        url, AppCacheEntry(), no_group, no_cache,
        make_scoped_refptr(GetOrCreateDelegateReference(delegate))));
    return;
  }

  // We have to query the database, schedule a database task to do so.
  scoped_refptr<FindMainResponseTask> task =
      new FindMainResponseTask(this, *url_ptr, groups_in_use);
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->Schedule();
}

void AppCacheStorageImpl::DeliverShortCircuitedFindMainResponse(
    const GURL& url, AppCacheEntry found_entry,
    scoped_refptr<AppCacheGroup> group, scoped_refptr<AppCache> cache,
    scoped_refptr<DelegateReference> delegate_ref) {
  if (delegate_ref->delegate) {
    delegate_ref->delegate->OnMainResponseFound(
        url, found_entry, AppCacheEntry(),
        cache.get() ? cache->cache_id() : kNoCacheId,
        group.get() ? group->manifest_url() : GURL::EmptyGURL());
  }
}

void AppCacheStorageImpl::FindResponseForSubRequest(
    AppCache* cache, const GURL& url,
    AppCacheEntry* found_entry, AppCacheEntry* found_fallback_entry,
    bool* found_network_namespace) {
  DCHECK(cache && cache->is_complete());
  GURL fallback_namespace_not_used;
  cache->FindResponseForRequest(
      url, found_entry, found_fallback_entry,
      &fallback_namespace_not_used, found_network_namespace);
}

void AppCacheStorageImpl::MarkEntryAsForeign(
    const GURL& entry_url, int64 cache_id) {
  AppCache* cache = working_set_.GetCache(cache_id);
  if (cache) {
    AppCacheEntry* entry = cache->GetEntry(entry_url);
    DCHECK(entry);
    if (entry)
      entry->add_types(AppCacheEntry::FOREIGN);
  }
  scoped_refptr<MarkEntryAsForeignTask> task =
      new MarkEntryAsForeignTask(this, entry_url, cache_id);
  task->Schedule();
  pending_foreign_markings_.push_back(std::make_pair(entry_url, cache_id));
}

void AppCacheStorageImpl::MakeGroupObsolete(
    AppCacheGroup* group, Delegate* delegate) {
  DCHECK(group && delegate);
  scoped_refptr<MakeGroupObsoleteTask> task =
      new MakeGroupObsoleteTask(this, group);
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->Schedule();
}

AppCacheResponseReader* AppCacheStorageImpl::CreateResponseReader(
    const GURL& manifest_url, int64 response_id) {
  return new AppCacheResponseReader(response_id, disk_cache());
}

AppCacheResponseWriter* AppCacheStorageImpl::CreateResponseWriter(
    const GURL& manifest_url) {
  return new AppCacheResponseWriter(NewResponseId(), disk_cache());
}

void AppCacheStorageImpl::DoomResponses(
    const GURL& manifest_url, const std::vector<int64>& response_ids) {
  for (std::vector<int64>::const_iterator it = response_ids.begin();
       it != response_ids.end(); ++it) {
    disk_cache()->DoomEntry(Int64ToString(*it));
  }
}

AppCacheStorageImpl::CacheLoadTask*
AppCacheStorageImpl::GetPendingCacheLoadTask(int64 cache_id) {
  PendingCacheLoads::iterator found = pending_cache_loads_.find(cache_id);
  if (found != pending_cache_loads_.end())
    return found->second;
  return NULL;
}

AppCacheStorageImpl::GroupLoadTask*
AppCacheStorageImpl::GetPendingGroupLoadTask(const GURL& manifest_url) {
  PendingGroupLoads::iterator found = pending_group_loads_.find(manifest_url);
  if (found != pending_group_loads_.end())
    return found->second;
  return NULL;
}

void AppCacheStorageImpl::GetPendingForeignMarkingsForCache(
    int64 cache_id, std::vector<GURL>* urls) {
  PendingForeignMarkings::iterator iter = pending_foreign_markings_.begin();
  while (iter != pending_foreign_markings_.end()) {
    if (iter->second == cache_id)
      urls->push_back(iter->first);
    ++iter;
  }
}

void AppCacheStorageImpl::ScheduleSimpleTask(Task* task) {
  pending_simple_tasks_.push_back(task);
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &AppCacheStorageImpl::RunOnePendingSimpleTask));
}

void AppCacheStorageImpl::RunOnePendingSimpleTask() {
  DCHECK(!pending_simple_tasks_.empty());
  Task* task = pending_simple_tasks_.front();
  pending_simple_tasks_.pop_front();
  task->Run();
  delete task;
}

disk_cache::Backend* AppCacheStorageImpl::disk_cache() {
  if (!disk_cache_.get()) {
    if (is_incognito_) {
      disk_cache_.reset(
          disk_cache::CreateInMemoryCacheBackend(kMaxDiskCacheSize));
    } else {
      // TODO(michaeln): create a disk backed backend
      disk_cache_.reset(
          disk_cache::CreateInMemoryCacheBackend(kMaxDiskCacheSize));
    }
  }
  return disk_cache_.get();
}

}  // namespace appcache
