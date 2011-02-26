// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_storage_impl.h"

#include "app/sql/connection.h"
#include "app/sql/transaction.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_database.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_histograms.h"
#include "webkit/appcache/appcache_policy.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/appcache_thread.h"
#include "webkit/quota/special_storage_policy.h"

namespace {
// Helper with no return value for use with NewRunnableFunction.
void DeleteDirectory(const FilePath& path) {
  file_util::Delete(path, true);
}
}

namespace appcache {

static const int kMaxDiskCacheSize = 250 * 1024 * 1024;
static const int kMaxMemDiskCacheSize = 10 * 1024 * 1024;
static const FilePath::CharType kDiskCacheDirectoryName[] =
    FILE_PATH_LITERAL("Cache");

// DatabaseTask -----------------------------------------

class AppCacheStorageImpl::DatabaseTask
    : public base::RefCountedThreadSafe<DatabaseTask> {
 public:
  explicit DatabaseTask(AppCacheStorageImpl* storage)
      : storage_(storage), database_(storage->database_) {}

  virtual ~DatabaseTask() {}

  void AddDelegate(DelegateReference* delegate_reference) {
    delegates_.push_back(make_scoped_refptr(delegate_reference));
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
  void CallDisableStorage();
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
  if (!database_->is_disabled()) {
    Run();
    if (database_->is_disabled()) {
      AppCacheThread::PostTask(AppCacheThread::io(), FROM_HERE,
          NewRunnableMethod(this, &DatabaseTask::CallDisableStorage));
    }
  }
  AppCacheThread::PostTask(AppCacheThread::io(), FROM_HERE,
      NewRunnableMethod(this, &DatabaseTask::CallRunCompleted));
}

void AppCacheStorageImpl::DatabaseTask::CallRunCompleted() {
  if (storage_) {
    DCHECK(AppCacheThread::CurrentlyOn(AppCacheThread::io()));
    DCHECK(storage_->scheduled_database_tasks_.front() == this);
    storage_->scheduled_database_tasks_.pop_front();
    RunCompleted();
    delegates_.clear();
  }
}

void AppCacheStorageImpl::DatabaseTask::CallDisableStorage() {
  if (storage_) {
    DCHECK(AppCacheThread::CurrentlyOn(AppCacheThread::io()));
    storage_->Disable();
  }
}

// InitTask -------

class AppCacheStorageImpl::InitTask : public DatabaseTask {
 public:
  explicit InitTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage), last_group_id_(0),
        last_cache_id_(0), last_response_id_(0),
        last_deletable_response_rowid_(0) {}

  virtual void Run();
  virtual void RunCompleted();

  int64 last_group_id_;
  int64 last_cache_id_;
  int64 last_response_id_;
  int64 last_deletable_response_rowid_;
  std::set<GURL> origins_with_groups_;
};

void AppCacheStorageImpl::InitTask::Run() {
  database_->FindLastStorageIds(
      &last_group_id_, &last_cache_id_, &last_response_id_,
      &last_deletable_response_rowid_);
  database_->FindOriginsWithGroups(&origins_with_groups_);
}

void AppCacheStorageImpl::InitTask::RunCompleted() {
  storage_->last_group_id_ = last_group_id_;
  storage_->last_cache_id_ = last_cache_id_;
  storage_->last_response_id_ = last_response_id_;
  storage_->last_deletable_response_rowid_ = last_deletable_response_rowid_;

  if (!storage_->is_disabled()) {
    storage_->origins_with_groups_.swap(origins_with_groups_);

    const int kDelayMillis = 5 * 60 * 1000;  // Five minutes.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
      storage_->method_factory_.NewRunnableMethod(
          &AppCacheStorageImpl::DelayedStartDeletingUnusedResponses),
      kDelayMillis);
  }
}

// CloseConnectionTask -------

class AppCacheStorageImpl::CloseConnectionTask : public DatabaseTask {
 public:
  explicit CloseConnectionTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage) {}

  virtual void Run() { database_->CloseConnection(); }
};

// DisableDatabaseTask -------

class AppCacheStorageImpl::DisableDatabaseTask : public DatabaseTask {
 public:
  explicit DisableDatabaseTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage) {}

  virtual void Run() { database_->Disable(); }
};

// GetAllInfoTask -------

class AppCacheStorageImpl::GetAllInfoTask : public DatabaseTask {
 public:
  explicit GetAllInfoTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage),
        info_collection_(new AppCacheInfoCollection()) {
  }

  virtual void Run();
  virtual void RunCompleted();

  scoped_refptr<AppCacheInfoCollection> info_collection_;
};

void AppCacheStorageImpl::GetAllInfoTask::Run() {
  std::set<GURL> origins;
  database_->FindOriginsWithGroups(&origins);
  for (std::set<GURL>::const_iterator origin = origins.begin();
       origin != origins.end(); ++origin) {
    AppCacheInfoVector& infos =
        info_collection_->infos_by_origin[*origin];
    std::vector<AppCacheDatabase::GroupRecord> groups;
    database_->FindGroupsForOrigin(*origin, &groups);
    for (std::vector<AppCacheDatabase::GroupRecord>::const_iterator
         group = groups.begin();
         group != groups.end(); ++group) {
      AppCacheDatabase::CacheRecord cache_record;
      database_->FindCacheForGroup(group->group_id, &cache_record);
      AppCacheInfo info;
      info.manifest_url = group->manifest_url;
      info.creation_time = group->creation_time;
      info.size = cache_record.cache_size;
      info.last_access_time = group->last_access_time;
      info.last_update_time = cache_record.update_time;
      info.cache_id = cache_record.cache_id;
      info.is_complete = true;
      infos.push_back(info);
    }
  }
}

void AppCacheStorageImpl::GetAllInfoTask::RunCompleted() {
  DCHECK(delegates_.size() == 1);
  FOR_EACH_DELEGATE(delegates_, OnAllInfo(info_collection_));
}

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
    group->get()->set_creation_time(group_record_.creation_time);
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

  // TODO(michaeln): Maybe verify that the responses we expect to exist
  // do actually exist in the disk_cache (and if not then what?)
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

  if (success_)
    database_->UpdateGroupLastAccessTime(group_record_.group_id,
                                         base::Time::Now());
}

void AppCacheStorageImpl::CacheLoadTask::RunCompleted() {
  storage_->pending_cache_loads_.erase(cache_id_);
  scoped_refptr<AppCache> cache;
  scoped_refptr<AppCacheGroup> group;
  if (success_ && !storage_->is_disabled()) {
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

  if (success_)
    database_->UpdateGroupLastAccessTime(group_record_.group_id,
                                         base::Time::Now());
}

void AppCacheStorageImpl::GroupLoadTask::RunCompleted() {
  storage_->pending_group_loads_.erase(manifest_url_);
  scoped_refptr<AppCacheGroup> group;
  scoped_refptr<AppCache> cache;
  if (!storage_->is_disabled()) {
    if (success_) {
      DCHECK(group_record_.manifest_url == manifest_url_);
      DCHECK(!storage_->working_set_.GetGroup(manifest_url_));
      DCHECK(!storage_->working_set_.GetCache(cache_record_.cache_id));
      CreateCacheAndGroupFromRecords(&cache, &group);
    } else {
      group = new AppCacheGroup(
          storage_->service_, manifest_url_, storage_->NewGroupId());
    }
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
  bool would_exceed_quota_;
  int64 quota_override_;
  std::vector<int64> newly_deletable_response_ids_;
};

AppCacheStorageImpl::StoreGroupAndCacheTask::StoreGroupAndCacheTask(
    AppCacheStorageImpl* storage, AppCacheGroup* group, AppCache* newest_cache)
    : StoreOrLoadTask(storage), group_(group), cache_(newest_cache),
      success_(false), would_exceed_quota_(false),
      quota_override_(-1) {
  group_record_.group_id = group->group_id();
  group_record_.manifest_url = group->manifest_url();
  group_record_.origin = group_record_.manifest_url.GetOrigin();
  newest_cache->ToDatabaseRecords(
      group,
      &cache_record_, &entry_records_, &fallback_namespace_records_,
      &online_whitelist_records_);

  if (storage->service()->special_storage_policy() &&
      storage->service()->special_storage_policy()->IsStorageUnlimited(
          group_record_.origin)) {
    quota_override_ = kint64max;
  }
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
    group_record_.creation_time = base::Time::Now();
    group_record_.last_access_time = base::Time::Now();
    success_ = database_->InsertGroup(&group_record_);
  } else {
    DCHECK(group_record_.group_id == existing_group.group_id);
    DCHECK(group_record_.manifest_url == existing_group.manifest_url);
    DCHECK(group_record_.origin == existing_group.origin);

    database_->UpdateGroupLastAccessTime(group_record_.group_id,
                                         base::Time::Now());

    AppCacheDatabase::CacheRecord cache;
    if (database_->FindCacheForGroup(group_record_.group_id, &cache)) {
      // Get the set of response ids in the old cache.
      std::set<int64> existing_response_ids;
      database_->FindResponseIdsForCacheAsSet(cache.cache_id,
                                              &existing_response_ids);

      // Remove those that remain in the new cache.
      std::vector<AppCacheDatabase::EntryRecord>::const_iterator entry_iter =
          entry_records_.begin();
      while (entry_iter != entry_records_.end()) {
        existing_response_ids.erase(entry_iter->response_id);
        ++entry_iter;
      }

      // The rest are deletable.
      std::set<int64>::const_iterator id_iter = existing_response_ids.begin();
      while (id_iter != existing_response_ids.end()) {
        newly_deletable_response_ids_.push_back(*id_iter);
        ++id_iter;
      }

      success_ =
          database_->DeleteCache(cache.cache_id) &&
          database_->DeleteEntriesForCache(cache.cache_id) &&
          database_->DeleteFallbackNameSpacesForCache(cache.cache_id) &&
          database_->DeleteOnlineWhiteListForCache(cache.cache_id) &&
          database_->InsertDeletableResponseIds(newly_deletable_response_ids_);
    } else {
      NOTREACHED() << "A existing group without a cache is unexpected";
    }
  }

  success_ =
      success_ &&
      database_->InsertCache(&cache_record_) &&
      database_->InsertEntryRecords(entry_records_) &&
      database_->InsertFallbackNameSpaceRecords(fallback_namespace_records_)&&
      database_->InsertOnlineWhiteListRecords(online_whitelist_records_);

  if (!success_)
    return;

  int64 quota = (quota_override_ >= 0) ?
      quota_override_ :
      database_->GetOriginQuota(group_record_.origin);

  if (database_->GetOriginUsage(group_record_.origin) > quota) {
    would_exceed_quota_ = true;
    success_ = false;
    return;
  }

  success_ = transaction.Commit();
}

void AppCacheStorageImpl::StoreGroupAndCacheTask::RunCompleted() {
  if (success_) {
    // TODO(kkanetkar): Add to creation time when that's enabled.
    storage_->origins_with_groups_.insert(group_->manifest_url().GetOrigin());
    if (cache_ != group_->newest_complete_cache()) {
      cache_->set_complete(true);
      group_->AddCache(cache_);
    }
    group_->AddNewlyDeletableResponseIds(&newly_deletable_response_ids_);
  }
  FOR_EACH_DELEGATE(delegates_,
                    OnGroupAndNewestCacheStored(group_, cache_, success_,
                                                would_exceed_quota_));
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
  GURL fallback_url_;
  int64 cache_id_;
  GURL manifest_url_;
};

// Helpers for FindMainResponseTask::Run()
namespace {
bool SortByLength(
    const AppCacheDatabase::FallbackNameSpaceRecord& lhs,
    const AppCacheDatabase::FallbackNameSpaceRecord& rhs) {
  return lhs.namespace_url.spec().length() > rhs.namespace_url.spec().length();
}

class NetworkNamespaceHelper {
 public:
  explicit NetworkNamespaceHelper(AppCacheDatabase* database)
      : database_(database) {
  }

  bool IsInNetworkNamespace(const GURL& url, int64 cache_id) {
    static const std::vector<GURL> kEmptyVector;
    typedef std::pair<WhiteListMap::iterator, bool> InsertResult;
    InsertResult result = namespaces_map_.insert(
        WhiteListMap::value_type(cache_id, kEmptyVector));
    if (result.second)
      GetOnlineWhiteListForCache(cache_id, &result.first->second);
    return AppCache::IsInNetworkNamespace(url, result.first->second);
  }

 private:
  void GetOnlineWhiteListForCache(
      int64 cache_id, std::vector<GURL>* urls) {
    DCHECK(urls && urls->empty());
    typedef std::vector<AppCacheDatabase::OnlineWhiteListRecord>
        WhiteListVector;
    WhiteListVector records;
    if (!database_->FindOnlineWhiteListForCache(cache_id, &records))
      return;
    WhiteListVector::const_iterator iter = records.begin();
    while (iter != records.end()) {
      urls->push_back(iter->namespace_url);
      ++iter;
    }
  }

  // Key is cache id
  typedef std::map<int64, std::vector<GURL> > WhiteListMap;
  WhiteListMap namespaces_map_;
  AppCacheDatabase* database_;
};
}  // namespace

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
  NetworkNamespaceHelper network_namespace_helper(database_);
  for (iter = fallbacks.begin(); iter < fallbacks.end(); ++iter) {
    // Skip this fallback namespace if the requested url falls into a network
    // namespace of the containing appcache.
    if (network_namespace_helper.IsInNetworkNamespace(url_, iter->cache_id))
      continue;

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
        if (entry_record.flags & AppCacheEntry::FOREIGN)
          continue;
        AppCacheDatabase::GroupRecord group_record;
        if (!database_->FindGroupForCache(iter->cache_id, &group_record)) {
          NOTREACHED() << "A cache without a group is not expected.";
          continue;
        }
        cache_id_ = iter->cache_id;
        fallback_url_ = iter->fallback_entry_url;
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
  storage_->CheckPolicyAndCallOnMainResponseFound(
      &delegates_, url_, entry_, fallback_url_, fallback_entry_,
      cache_id_, manifest_url_);
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
  std::vector<int64> newly_deletable_response_ids_;
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
    database_->FindResponseIdsForCacheAsVector(cache_record.cache_id,
                                               &newly_deletable_response_ids_);
    success_ =
        database_->DeleteGroup(group_id_) &&
        database_->DeleteCache(cache_record.cache_id) &&
        database_->DeleteEntriesForCache(cache_record.cache_id) &&
        database_->DeleteFallbackNameSpacesForCache(cache_record.cache_id) &&
        database_->DeleteOnlineWhiteListForCache(cache_record.cache_id) &&
        database_->InsertDeletableResponseIds(newly_deletable_response_ids_);
  } else {
    NOTREACHED() << "A existing group without a cache is unexpected";
    success_ = database_->DeleteGroup(group_id_);
  }

  success_ = success_ &&
             database_->FindOriginsWithGroups(&origins_with_groups_) &&
             transaction.Commit();
}

void AppCacheStorageImpl::MakeGroupObsoleteTask::RunCompleted() {
  if (success_) {
    group_->set_obsolete(true);
    if (!storage_->is_disabled()) {
      storage_->origins_with_groups_.swap(origins_with_groups_);
      group_->AddNewlyDeletableResponseIds(&newly_deletable_response_ids_);

      // Also remove from the working set, caches for an 'obsolete' group
      // may linger in use, but the group itself cannot be looked up by
      // 'manifest_url' in the working set any longer.
      storage_->working_set()->RemoveGroup(group_);
    }
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

// GetDeletableResponseIdsTask -------

class AppCacheStorageImpl::GetDeletableResponseIdsTask : public DatabaseTask {
 public:
  GetDeletableResponseIdsTask(AppCacheStorageImpl* storage, int64 max_rowid)
      : DatabaseTask(storage), max_rowid_(max_rowid) {}

  virtual void Run();
  virtual void RunCompleted();

  int64 max_rowid_;
  std::vector<int64> response_ids_;
};

void AppCacheStorageImpl::GetDeletableResponseIdsTask::Run() {
  const int kSqlLimit = 1000;
  database_->GetDeletableResponseIds(&response_ids_, max_rowid_, kSqlLimit);
}

void AppCacheStorageImpl::GetDeletableResponseIdsTask::RunCompleted() {
  if (!response_ids_.empty())
    storage_->StartDeletingResponses(response_ids_);
}

// InsertDeletableResponseIdsTask -------

class AppCacheStorageImpl::InsertDeletableResponseIdsTask
    : public DatabaseTask {
 public:
  explicit InsertDeletableResponseIdsTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage) {}
  virtual void Run();
  std::vector<int64> response_ids_;
};

void AppCacheStorageImpl::InsertDeletableResponseIdsTask::Run() {
  database_->InsertDeletableResponseIds(response_ids_);
}

// DeleteDeletableResponseIdsTask -------

class AppCacheStorageImpl::DeleteDeletableResponseIdsTask
    : public DatabaseTask {
 public:
  explicit DeleteDeletableResponseIdsTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage) {}
  virtual void Run();
  std::vector<int64> response_ids_;
};

void AppCacheStorageImpl::DeleteDeletableResponseIdsTask::Run() {
  database_->DeleteDeletableResponseIds(response_ids_);
}

// UpdateGroupLastAccessTimeTask -------

class AppCacheStorageImpl::UpdateGroupLastAccessTimeTask
    : public DatabaseTask {
 public:
  UpdateGroupLastAccessTimeTask(
      AppCacheStorageImpl* storage, int64 group_id, base::Time time)
      : DatabaseTask(storage), group_id_(group_id), last_access_time_(time) {}

  virtual void Run();

  int64 group_id_;
  base::Time last_access_time_;
};

void AppCacheStorageImpl::UpdateGroupLastAccessTimeTask::Run() {
  database_->UpdateGroupLastAccessTime(group_id_, last_access_time_);
}


// AppCacheStorageImpl ---------------------------------------------------

AppCacheStorageImpl::AppCacheStorageImpl(AppCacheService* service)
    : AppCacheStorage(service), is_incognito_(false),
      is_response_deletion_scheduled_(false),
      did_start_deleting_responses_(false),
      last_deletable_response_rowid_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(doom_callback_(
          this, &AppCacheStorageImpl::OnDeletedOneResponse)),
      ALLOW_THIS_IN_INITIALIZER_LIST(init_callback_(
          this, &AppCacheStorageImpl::OnDiskCacheInitialized)),
      database_(NULL), is_disabled_(false),
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

void AppCacheStorageImpl::Initialize(const FilePath& cache_directory,
                                     base::MessageLoopProxy* cache_thread) {
  cache_directory_ = cache_directory;
  cache_thread_ = cache_thread;
  is_incognito_ = cache_directory_.empty();

  FilePath db_file_path;
  if (!is_incognito_)
    db_file_path = cache_directory_.Append(kAppCacheDatabaseName);
  database_ = new AppCacheDatabase(db_file_path);

  scoped_refptr<InitTask> task(new InitTask(this));
  task->Schedule();
}

void AppCacheStorageImpl::Disable() {
  if (is_disabled_)
    return;
  VLOG(1) << "Disabling appcache storage.";
  is_disabled_ = true;
  origins_with_groups_.clear();
  working_set()->Disable();
  if (disk_cache_.get())
    disk_cache_->Disable();
  scoped_refptr<DisableDatabaseTask> task(new DisableDatabaseTask(this));
  task->Schedule();
}

void AppCacheStorageImpl::GetAllInfo(Delegate* delegate) {
  DCHECK(delegate);
  scoped_refptr<GetAllInfoTask> task(new GetAllInfoTask(this));
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->Schedule();
}

void AppCacheStorageImpl::LoadCache(int64 id, Delegate* delegate) {
  DCHECK(delegate);
  if (is_disabled_) {
    delegate->OnCacheLoaded(NULL, id);
    return;
  }

  AppCache* cache = working_set_.GetCache(id);
  if (cache) {
    delegate->OnCacheLoaded(cache, id);
    if (cache->owning_group()) {
      scoped_refptr<DatabaseTask> update_task(
          new UpdateGroupLastAccessTimeTask(
              this, cache->owning_group()->group_id(), base::Time::Now()));
      update_task->Schedule();
    }
    return;
  }
  scoped_refptr<CacheLoadTask> task(GetPendingCacheLoadTask(id));
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
  if (is_disabled_) {
    delegate->OnGroupLoaded(NULL, manifest_url);
    return;
  }

  AppCacheGroup* group = working_set_.GetGroup(manifest_url);
  if (group) {
    delegate->OnGroupLoaded(group, manifest_url);
    scoped_refptr<DatabaseTask> update_task(
        new UpdateGroupLastAccessTimeTask(
            this, group->group_id(), base::Time::Now()));
    update_task->Schedule();
    return;
  }

  scoped_refptr<GroupLoadTask> task(GetPendingGroupLoadTask(manifest_url));
  if (task) {
    task->AddDelegate(GetOrCreateDelegateReference(delegate));
    return;
  }

  if (origins_with_groups_.find(manifest_url.GetOrigin()) ==
      origins_with_groups_.end()) {
    // No need to query the database, return a new group immediately.
    scoped_refptr<AppCacheGroup> group(new AppCacheGroup(
        service_, manifest_url, NewGroupId()));
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
  DCHECK(group && delegate && newest_cache);
  scoped_refptr<StoreGroupAndCacheTask> task(
      new StoreGroupAndCacheTask(this, group, newest_cache));
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
  scoped_refptr<FindMainResponseTask> task(
      new FindMainResponseTask(this, *url_ptr, groups_in_use));
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->Schedule();
}

void AppCacheStorageImpl::DeliverShortCircuitedFindMainResponse(
    const GURL& url, AppCacheEntry found_entry,
    scoped_refptr<AppCacheGroup> group, scoped_refptr<AppCache> cache,
    scoped_refptr<DelegateReference> delegate_ref) {
  if (delegate_ref->delegate) {
    DelegateReferenceVector delegates(1, delegate_ref);
    CheckPolicyAndCallOnMainResponseFound(
        &delegates, url, found_entry,
        GURL(), AppCacheEntry(),
        cache.get() ? cache->cache_id() : kNoCacheId,
        group.get() ? group->manifest_url() : GURL());
  }
}

void AppCacheStorageImpl::CheckPolicyAndCallOnMainResponseFound(
    DelegateReferenceVector* delegates,
    const GURL& url, const AppCacheEntry& entry,
    const GURL& fallback_url, const AppCacheEntry& fallback_entry,
    int64 cache_id, const GURL& manifest_url) {
  if (!manifest_url.is_empty()) {
    // Check the policy prior to returning a main resource from the appcache.
    AppCachePolicy* policy = service()->appcache_policy();
    if (policy && !policy->CanLoadAppCache(manifest_url)) {
      FOR_EACH_DELEGATE(
          (*delegates),
          OnMainResponseFound(url, AppCacheEntry(),
                              GURL(), AppCacheEntry(),
                              kNoCacheId, manifest_url, true));
      return;
    }
  }

  FOR_EACH_DELEGATE(
      (*delegates),
      OnMainResponseFound(url, entry,
                          fallback_url, fallback_entry,
                          cache_id, manifest_url, false));
}

void AppCacheStorageImpl::FindResponseForSubRequest(
    AppCache* cache, const GURL& url,
    AppCacheEntry* found_entry, AppCacheEntry* found_fallback_entry,
    bool* found_network_namespace) {
  DCHECK(cache && cache->is_complete());

  // When a group is forcibly deleted, all subresource loads for pages
  // using caches in the group will result in a synthesized network errors.
  // Forcible deletion is not a function that is covered by the HTML5 spec.
  if (cache->owning_group()->is_being_deleted()) {
    *found_entry = AppCacheEntry();
    *found_fallback_entry = AppCacheEntry();
    *found_network_namespace = false;
    return;
  }

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
  scoped_refptr<MarkEntryAsForeignTask> task(
      new MarkEntryAsForeignTask(this, entry_url, cache_id));
  task->Schedule();
  pending_foreign_markings_.push_back(std::make_pair(entry_url, cache_id));
}

void AppCacheStorageImpl::MakeGroupObsolete(
    AppCacheGroup* group, Delegate* delegate) {
  DCHECK(group && delegate);
  scoped_refptr<MakeGroupObsoleteTask> task(
      new MakeGroupObsoleteTask(this, group));
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
  if (response_ids.empty())
    return;

  // Start deleting them from the disk cache lazily.
  StartDeletingResponses(response_ids);

  // Also schedule a database task to record these ids in the
  // deletable responses table.
  // TODO(michaeln): There is a race here. If the browser crashes
  // prior to committing these rows to the database and prior to us
  // having deleted them from the disk cache, we'll never delete them.
  scoped_refptr<InsertDeletableResponseIdsTask> task(
      new InsertDeletableResponseIdsTask(this));
  task->response_ids_ = response_ids;
  task->Schedule();
}

void AppCacheStorageImpl::DeleteResponses(
    const GURL& manifest_url, const std::vector<int64>& response_ids) {
  if (response_ids.empty())
    return;
  StartDeletingResponses(response_ids);
}

void AppCacheStorageImpl::PurgeMemory() {
  scoped_refptr<CloseConnectionTask> task(new CloseConnectionTask(this));
  task->Schedule();
}

void AppCacheStorageImpl::DelayedStartDeletingUnusedResponses() {
  // Only if we haven't already begun.
  if (!did_start_deleting_responses_) {
    scoped_refptr<GetDeletableResponseIdsTask> task(
        new GetDeletableResponseIdsTask(this, last_deletable_response_rowid_));
    task->Schedule();
  }
}

void AppCacheStorageImpl::StartDeletingResponses(
    const std::vector<int64>& response_ids) {
  DCHECK(!response_ids.empty());
  did_start_deleting_responses_ = true;
  deletable_response_ids_.insert(
      deletable_response_ids_.end(),
      response_ids.begin(), response_ids.end());
  if (!is_response_deletion_scheduled_)
    ScheduleDeleteOneResponse();
}

void AppCacheStorageImpl::ScheduleDeleteOneResponse() {
  DCHECK(!is_response_deletion_scheduled_);
  const int kDelayMillis = 10;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &AppCacheStorageImpl::DeleteOneResponse),
      kDelayMillis);
  is_response_deletion_scheduled_ = true;
}

void AppCacheStorageImpl::DeleteOneResponse() {
  DCHECK(is_response_deletion_scheduled_);
  DCHECK(!deletable_response_ids_.empty());

  if (!disk_cache()) {
    DCHECK(is_disabled_);
    deletable_response_ids_.clear();
    deleted_response_ids_.clear();
    is_response_deletion_scheduled_ = false;
    return;
  }

  int64 id = deletable_response_ids_.front();
  int rv = disk_cache_->DoomEntry(id, &doom_callback_);
  if (rv != net::ERR_IO_PENDING)
    OnDeletedOneResponse(rv);
}

void AppCacheStorageImpl::OnDeletedOneResponse(int rv) {
  is_response_deletion_scheduled_ = false;
  if (is_disabled_)
    return;

  int64 id = deletable_response_ids_.front();
  deletable_response_ids_.pop_front();
  if (rv != net::ERR_ABORTED)
    deleted_response_ids_.push_back(id);

  const size_t kBatchSize = 50U;
  if (deleted_response_ids_.size() >= kBatchSize ||
      deletable_response_ids_.empty()) {
    scoped_refptr<DeleteDeletableResponseIdsTask> task(
        new DeleteDeletableResponseIdsTask(this));
    task->response_ids_.swap(deleted_response_ids_);
    task->Schedule();
  }

  if (deletable_response_ids_.empty()) {
    scoped_refptr<GetDeletableResponseIdsTask> task(
        new GetDeletableResponseIdsTask(this, last_deletable_response_rowid_));
    task->Schedule();
    return;
  }

  ScheduleDeleteOneResponse();
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

AppCacheDiskCache* AppCacheStorageImpl::disk_cache() {
  DCHECK(IsInitTaskComplete());

  if (is_disabled_)
    return NULL;

  if (!disk_cache_.get()) {
    int rv = net::OK;
    disk_cache_.reset(new AppCacheDiskCache);
    if (is_incognito_) {
      rv = disk_cache_->InitWithMemBackend(
          kMaxMemDiskCacheSize, &init_callback_);
    } else {
      rv = disk_cache_->InitWithDiskBackend(
          cache_directory_.Append(kDiskCacheDirectoryName),
          kMaxDiskCacheSize, false, cache_thread_, &init_callback_);
    }

    // We should not keep this reference around.
    cache_thread_ = NULL;

    if (rv != net::ERR_IO_PENDING)
      OnDiskCacheInitialized(rv);
  }
  return disk_cache_.get();
}

void AppCacheStorageImpl::OnDiskCacheInitialized(int rv) {
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to open the appcache diskcache.";
    AppCacheHistograms::CountInitResult(AppCacheHistograms::DISK_CACHE_ERROR);

    // We're unable to open the disk cache, this is a fatal error that we can't
    // really recover from. We handle it by disabling the appcache for this
    // browser session and deleting the directory on disk. The next browser
    // session should start with a clean slate.
    Disable();
    if (!is_incognito_) {
      VLOG(1) << "Deleting existing appcache data and starting over.";
      AppCacheThread::PostTask(AppCacheThread::db(), FROM_HERE,
          NewRunnableFunction(DeleteDirectory, cache_directory_));
    }
  }
}

}  // namespace appcache
