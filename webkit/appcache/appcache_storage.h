// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_STORAGE_H_
#define WEBKIT_APPCACHE_APPCACHE_STORAGE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "webkit/appcache/appcache_working_set.h"

class GURL;

namespace appcache {

class AppCache;
class AppCacheGroup;
class AppCacheResponseInfo;
class AppCacheResponseReader;
class AppCacheResponseWriter;
class AppCacheService;

class AppCacheStorage {
 public:

  class Delegate {
   public:
    virtual ~Delegate() {}

    // If a load fails the 'cache' will be NULL.
    virtual void OnCacheLoaded(AppCache* cache, int64 cache_id) {}

    // If a load fails the 'group' will be NULL.
    virtual void OnGroupLoaded(
        AppCacheGroup* group, const GURL& manifest_url) {}

    // If successfully stored 'success' will be true.
    virtual void OnGroupAndNewestCacheStored(
        AppCacheGroup* group, bool success) {}

    // If the update fails, success will be false.
    virtual void OnGroupMarkedAsObsolete(GURL& manifest_url, bool success) {}

    // If a load fails the 'response_info' will be NULL.
    virtual void OnResponseInfoLoaded(
        AppCacheResponseInfo* response_info, int64 response_id) {}

    // If no response is found, response_id will be kNoResponseId.
    // If a response is found, the cache id and manifest url of the
    // containing cache and group are also returned.
    virtual void OnMainResponseFound(
        const GURL& url, int64 response_id, bool is_fallback,
        int64 cache_id, const GURL& mainfest_url) {}
  };

  explicit AppCacheStorage(AppCacheService* service)
      : last_cache_id_(kUnitializedId), last_group_id_(kUnitializedId),
        last_entry_id_(kUnitializedId), last_response_id_(kUnitializedId),
        service_(service)  {}
  virtual ~AppCacheStorage() {}

  // Schedules a cache to be loaded from storage. Upon load completion
  // the delegate will be called back. If the cache already resides in
  // memory, the delegate will be called back immediately without returning
  // to the message loop. If the load fails, the delegate will be called
  // back with a NULL cache pointer.
  virtual void LoadCache(int64 id, Delegate* delegate) = 0;

  // Schedules a group and its newest cache, if any, to be loaded from storage.
  // Upon load completion the delegate will be called back. If the group
  // and newest cache already reside in memory, the delegate will be called
  // back immediately without returning to the message loop. If the load fails,
  // the delegate will be called back with a NULL group pointer.
  virtual void LoadOrCreateGroup(
      const GURL& manifest_url, Delegate* delegate) = 0;

  // Schedules response info to be loaded from storage.
  // Upon load completion the delegate will be called back. If the data
  // already resides in memory, the delegate will be called back
  // immediately without returning to the message loop. If the load fails,
  // the delegate will be called back with a NULL pointer.
  virtual void LoadResponseInfo(
      const GURL& manifest_url, int64 response_id, Delegate* delegate) = 0;

  // Schedules a group and its newest complete cache to be initially stored or
  // incrementally updated with new changes. Upon completion the delegate
  // will be called back. A group without a newest cache cannot be stored.
  // It's a programming error to call this method with such a group. A
  // side effect of storing a new newest cache is the removal of the group's
  // old caches and responses from persistent storage (although they may still
  // linger in the in-memory working set until no longer needed).
  virtual void StoreGroupAndNewestCache(
      AppCacheGroup* group, Delegate* delegate) = 0;

  // Schedules a query to identify a response for a main request. Upon
  // completion the delegate will be called back.
  virtual void FindResponseForMainRequest(
      const GURL& url, Delegate* delegate) = 0;

  // Immediately updates in-memory storage, if the cache is in memory,
  // and schedules a task to update persistent storage. If the cache is
  // already scheduled to be loaded, upon loading completion the entry
  // will be marked. There is no delegate completion callback.
  virtual void MarkEntryAsForeign(const GURL& entry_url, int64 cache_id) = 0;

  // Schedules a task to update persistent storage and doom the group and all
  // related caches and responses for deletion. Upon completion the in-memory
  // instance is marked as obsolete and the delegate callback is called.
  virtual void MarkGroupAsObsolete(
      AppCacheGroup* group, Delegate* delegate) = 0;

  // Cancels all pending callbacks for the delegate. The delegate callbacks
  // will not be invoked after, however any scheduled operations will still
  // take place. The callbacks for subsequently scheduled operations are
  // unaffected.
  virtual void CancelDelegateCallbacks(Delegate* delegate) = 0;

  // Creates a reader to read a response from storage.
  virtual AppCacheResponseReader* CreateResponseReader(
      const GURL& manifest_url, int64 response_id) = 0;

  // Creates a writer to write a new response to storage. This call
  // establishes a new response id.
  virtual AppCacheResponseWriter* CreateResponseWriter(
      const GURL& manifest_url) = 0;

  // Schedules the deletion of many responses.
  virtual void DoomResponses(
      const GURL& manifest_url, const std::vector<int64>& response_ids) = 0;

  // Generates unique storage ids for different object types.
  int64 NewCacheId() {
    DCHECK(last_cache_id_ != kUnitializedId);
    return ++last_cache_id_;
  }
  int64 NewGroupId() {
    DCHECK(last_group_id_ != kUnitializedId);
    return ++last_group_id_;
  }
  int64 NewEntryId() {
    DCHECK(last_entry_id_ != kUnitializedId);
    return ++last_entry_id_;
  }

  // The working set of object instances currently in memory.
  AppCacheWorkingSet* working_set() { return &working_set_; }

  // Simple ptr back to the service object that owns us.
  AppCacheService* service() { return service_; }

 protected:
  // Should only be called when creating a new response writer.
  int64 NewResponseId() {
    DCHECK(last_response_id_ != kUnitializedId);
    return ++last_response_id_;
  }

  // The last storage id used for different object types.
  int64 last_cache_id_;
  int64 last_group_id_;
  int64 last_entry_id_;
  int64 last_response_id_;

  AppCacheWorkingSet working_set_;
  AppCacheService* service_;

  // The set of last ids must be retrieved from storage prior to being used.
  static const int64 kUnitializedId = -1;

  DISALLOW_COPY_AND_ASSIGN(AppCacheStorage);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_STORAGE_H_

