// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_GROUP_H_
#define WEBKIT_APPCACHE_APPCACHE_GROUP_H_

#include <vector>

#include "base/ref_counted.h"
#include "googleurl/src/gurl.h"

namespace appcache {

class AppCache;
class AppCacheService;

// Collection of application caches identified by the same manifest URL.
// A group exists as long as it is in use by a host or is being updated.
class AppCacheGroup : public base::RefCounted<AppCacheGroup> {
 public:

  enum UpdateStatus {
    IDLE,
    CHECKING,
    DOWNLOADING,
  };

  AppCacheGroup(AppCacheService* service, const GURL& manifest_url);
  ~AppCacheGroup();

  const GURL& manifest_url() { return manifest_url_; }

  UpdateStatus update_status() { return update_status_; }
  void set_update_status(UpdateStatus status) { update_status_ = status; }

  bool is_obsolete() { return is_obsolete_; }
  void set_obsolete(bool value) { is_obsolete_ = value; }

  AppCache* newest_complete_cache() { return newest_complete_cache_; }

  void AddCache(AppCache* complete_cache);

  // Returns false if cache cannot be removed. The newest complete cache
  // cannot be removed as long as the group is still in use.
  bool RemoveCache(AppCache* cache);

 private:
  GURL manifest_url_;
  UpdateStatus update_status_;
  bool is_obsolete_;

  // old complete app caches
  typedef std::vector<scoped_refptr<AppCache> > Caches;
  Caches old_caches_;

  // newest cache in this group to be complete, aka relevant cache
  scoped_refptr<AppCache> newest_complete_cache_;

  // to notify service when group is no longer needed
  AppCacheService* service_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_GROUP_H_
