// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_MOCK_APPCACHE_STORAGE_H_
#define WEBKIT_APPCACHE_MOCK_APPCACHE_STORAGE_H_

#include "webkit/appcache/appcache_storage.h"

namespace appcache {

// For use in unit tests.
// Note: This class is also being used to bootstrap our development efforts.
// We can get layout tests up and running, and back fill with real storage
// somewhat in parallel.
class MockAppCacheStorage : public AppCacheStorage {
 public:
  explicit MockAppCacheStorage(AppCacheService* service);
  virtual void LoadCache(int64 id, Delegate* delegate);
  virtual void LoadOrCreateGroup(const GURL& manifest_url, Delegate* delegate);
  virtual void LoadResponseInfo(
      const GURL& manifest_url, int64 response_id, Delegate* delegate);
  virtual void StoreGroupAndNewestCache(
      AppCacheGroup* group, Delegate* delegate);
  virtual void FindResponseForMainRequest(const GURL& url, Delegate* delegate);
  virtual void CancelDelegateCallbacks(Delegate* delegate);
  virtual void MarkEntryAsForeign(const GURL& entry_url, int64 cache_id);
  virtual void MarkGroupAsObsolete(AppCacheGroup* group, Delegate* delegate);
  virtual AppCacheResponseReader* CreateResponseReader(
      const GURL& manifest_url, int64 response_id);
  virtual AppCacheResponseWriter* CreateResponseWriter(const GURL& origin);
  virtual void DoomResponses(
      const GURL& manifest_url, const std::vector<int64>& response_ids);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_MOCK_APPCACHE_STORAGE_H_
