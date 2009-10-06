// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_GROUP_H_
#define WEBKIT_APPCACHE_APPCACHE_GROUP_H_

#include <vector>

#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace appcache {

class AppCache;
class AppCacheHost;
class AppCacheService;
class AppCacheUpdateJob;

// Collection of application caches identified by the same manifest URL.
// A group exists as long as it is in use by a host or is being updated.
class AppCacheGroup : public base::RefCounted<AppCacheGroup> {
 public:

  class Observer {
    public:
      // Called just after an appcache update has completed.
      virtual void OnUpdateComplete(AppCacheGroup* group) = 0;
      virtual ~Observer() { }
  };

  enum UpdateStatus {
    IDLE,
    CHECKING,
    DOWNLOADING,
  };

  AppCacheGroup(AppCacheService* service, const GURL& manifest_url);
  ~AppCacheGroup();

  // Adds/removes an observer, the AppCacheGroup does not take
  // ownership of the observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const GURL& manifest_url() { return manifest_url_; }

  bool is_obsolete() { return is_obsolete_; }
  void set_obsolete(bool value) { is_obsolete_ = value; }

  AppCache* newest_complete_cache() { return newest_complete_cache_; }

  void AddCache(AppCache* complete_cache);

  // Returns false if cache cannot be removed. The newest complete cache
  // cannot be removed as long as the group is still in use.
  bool RemoveCache(AppCache* cache);

  bool HasCache() { return newest_complete_cache_ || !old_caches_.empty(); }

  UpdateStatus update_status() { return update_status_; }

  // Starts an update via update() javascript API.
  void StartUpdate() {
    StartUpdateWithHost(NULL);
  }

  // Starts an update for a doc loaded from an application cache.
  void StartUpdateWithHost(AppCacheHost* host)  {
    StartUpdateWithNewMasterEntry(host, GURL::EmptyGURL());
  }

  // Starts an update for a doc loaded using HTTP GET or equivalent with
  // an <html> tag manifest attribute value that matches this group's
  // manifest url.
  void StartUpdateWithNewMasterEntry(AppCacheHost* host,
                                     const GURL& new_master_resource);

 private:
  friend class AppCacheUpdateJob;
  friend class AppCacheUpdateJobTest;

  typedef std::vector<scoped_refptr<AppCache> > Caches;

  AppCacheUpdateJob* update_job() { return update_job_; }
  void SetUpdateStatus(UpdateStatus status);

  const Caches& old_caches() { return old_caches_; }

  GURL manifest_url_;
  UpdateStatus update_status_;
  bool is_obsolete_;

  // Old complete app caches.
  Caches old_caches_;

  // Newest cache in this group to be complete, aka relevant cache.
  scoped_refptr<AppCache> newest_complete_cache_;

  // Current update job for this group, if any.
  AppCacheUpdateJob* update_job_;

  // Central service object.
  AppCacheService* service_;

  // List of objects observing this group.
  ObserverList<Observer> observers_;

  FRIEND_TEST(AppCacheGroupTest, StartUpdate);
  FRIEND_TEST(AppCacheUpdateJobTest, AlreadyChecking);
  FRIEND_TEST(AppCacheUpdateJobTest, AlreadyDownloading);
  DISALLOW_COPY_AND_ASSIGN(AppCacheGroup);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_GROUP_H_
