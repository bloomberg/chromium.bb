// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_HOST_H_
#define WEBKIT_APPCACHE_APPCACHE_HOST_H_

#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/appcache_storage.h"

class URLRequest;

namespace appcache {

class AppCache;
class AppCacheFrontend;
class AppCacheRequestHandler;

typedef Callback2<Status, void*>::Type GetStatusCallback;
typedef Callback2<bool, void*>::Type StartUpdateCallback;
typedef Callback2<bool, void*>::Type SwapCacheCallback;

// Server-side representation of an application cache host.
class AppCacheHost : public AppCacheStorage::Delegate,
                     public AppCacheGroup::UpdateObserver {
 public:

  class Observer {
   public:
    // Called just after the cache selection algorithm completes.
    virtual void OnCacheSelectionComplete(AppCacheHost* host) = 0;

    // Called just prior to the instance being deleted.
    virtual void OnDestructionImminent(AppCacheHost* host) = 0;

    virtual ~Observer() {}
  };

  AppCacheHost(int host_id, AppCacheFrontend* frontend,
               AppCacheService* service);
  ~AppCacheHost();

  // Adds/removes an observer, the AppCacheHost does not take
  // ownership of the observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Support for cache selection and scriptable method calls.
  void SelectCache(const GURL& document_url,
                   const int64 cache_document_was_loaded_from,
                   const GURL& manifest_url);
  void MarkAsForeignEntry(const GURL& document_url,
                          int64 cache_document_was_loaded_from);
  void GetStatusWithCallback(GetStatusCallback* callback,
                             void* callback_param);
  void StartUpdateWithCallback(StartUpdateCallback* callback,
                               void* callback_param);
  void SwapCacheWithCallback(SwapCacheCallback* callback,
                             void* callback_param);

  // Support for loading resources out of the appcache.
  // Returns NULL if the host is not associated with a complete cache.
  AppCacheRequestHandler* CreateRequestHandler(URLRequest* request,
                                               bool is_main_request);

  // Establishes an association between this host and a cache. 'cache' may be
  // NULL to break any existing association. Associations are established
  // either thru the cache selection algorithm implemented (in this class),
  // or by the update algorithm (see AppCacheUpdateJob).
  void AssociateCache(AppCache* cache);

  // Adds a reference to the newest complete cache in a group, unless it's the
  // same as the cache that is currently associated with the host.
  void SetSwappableCache(AppCacheGroup* group);

  // Used to ensure that a loaded appcache survives a frame navigation.
  void LoadMainResourceCache(int64 cache_id);

  // Used by the update job to keep track of which hosts are associated
  // with which pending master entries.
  const GURL& pending_master_entry_url() const {
    return new_master_entry_url_;
  }

  int host_id() const { return host_id_; }
  AppCacheService* service() const { return service_; }
  AppCacheFrontend* frontend() const { return frontend_; }
  AppCache* associated_cache() const { return associated_cache_.get(); }

  bool is_selection_pending() const {
    return pending_selected_cache_id_ != kNoCacheId ||
           !pending_selected_manifest_url_.is_empty();
  }

 private:
  Status GetStatus();
  void LoadSelectedCache(int64 cache_id);
  void LoadOrCreateGroup(const GURL& manifest_url);

  // AppCacheStorage::Delegate impl
  virtual void OnCacheLoaded(AppCache* cache, int64 cache_id);
  virtual void OnGroupLoaded(AppCacheGroup* group,
                             const GURL& manifest_url);

  void FinishCacheSelection(AppCache* cache, AppCacheGroup* group);
  void DoPendingGetStatus();
  void DoPendingStartUpdate();
  void DoPendingSwapCache();

  void ObserveGroupBeingUpdated(AppCacheGroup* group);

  // AppCacheGroup::UpdateObserver method
  virtual void OnUpdateComplete(AppCacheGroup* group);

  // Identifies the corresponding appcache host in the child process.
  int host_id_;

  // The cache associated with this host, if any.
  scoped_refptr<AppCache> associated_cache_;

  // Hold a reference to the newest complete cache (if associated cache is
  // not the newest) to keep the newest cache in existence while the app cache
  // group is in use. The newest complete cache may have no associated hosts
  // holding any references to it and would otherwise be deleted prematurely.
  scoped_refptr<AppCache> swappable_cache_;

  // Keep a reference to the group being updated until the update completes.
  scoped_refptr<AppCacheGroup> group_being_updated_;

  // Similarly, keep a reference to the newest cache of the group until the
  // update completes. When adding a new master entry to a cache that is not
  // in use in any other host, this reference keeps the cache in  memory.
  scoped_refptr<AppCache> newest_cache_of_group_being_updated_;

  // Keep a reference to the cache of the main resource so it survives frame
  // navigations.
  scoped_refptr<AppCache> main_resource_cache_;
  int64 pending_main_resource_cache_id_;

  // Cache loading is async, if we're loading a specific cache or group
  // for the purposes of cache selection, one or the other of these will
  // indicate which cache or group is being loaded.
  int64 pending_selected_cache_id_;
  GURL pending_selected_manifest_url_;

  // A new master entry to be added to the cache, may be empty.
  GURL new_master_entry_url_;

  // The frontend proxy to deliver notifications to the child process.
  AppCacheFrontend* frontend_;

  // Our central service object.
  AppCacheService* service_;

  // Since these are synchronous scriptable api calls in the client,
  // there can only be one type of callback pending.
  // Also, we have to wait until we have a cache selection prior
  // to responding to these calls, as cache selection involves
  // async loading of a cache or a group from storage.
  GetStatusCallback* pending_get_status_callback_;
  StartUpdateCallback* pending_start_update_callback_;
  SwapCacheCallback* pending_swap_cache_callback_;
  void* pending_callback_param_;

  // List of objects observing us.
  ObserverList<Observer> observers_;

  friend class AppCacheRequestHandlerTest;
  friend class AppCacheUpdateJobTest;
  FRIEND_TEST(AppCacheTest, CleanupUnusedCache);
  FRIEND_TEST(AppCacheGroupTest, CleanupUnusedGroup);
  FRIEND_TEST(AppCacheHostTest, Basic);
  FRIEND_TEST(AppCacheHostTest, SelectNoCache);
  FRIEND_TEST(AppCacheHostTest, ForeignEntry);
  FRIEND_TEST(AppCacheHostTest, FailedCacheLoad);
  FRIEND_TEST(AppCacheHostTest, FailedGroupLoad);
  FRIEND_TEST(AppCacheHostTest, SetSwappableCache);
  FRIEND_TEST(AppCacheGroupTest, QueueUpdate);
  DISALLOW_COPY_AND_ASSIGN(AppCacheHost);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_HOST_H_
