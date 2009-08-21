// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_BACKEND_IMPL_H_
#define WEBKIT_APPCACHE_APPCACHE_BACKEND_IMPL_H_

#include "webkit/appcache/appcache_interfaces.h"

namespace appcache {

class AppCacheService;

class AppCacheBackendImpl : public AppCacheBackend {
 public:
  AppCacheBackendImpl() : service_(NULL), frontend_(NULL) {}
  ~AppCacheBackendImpl();

  void Initialize(AppCacheService* service,
                  AppCacheFrontend* frontend);

  virtual void RegisterHost(int host_id);
  virtual void UnregisterHost(int host_id);
  virtual void SelectCache(int host_id,
                           const GURL& document_url,
                           const int64 cache_document_was_loaded_from,
                           const GURL& manifest_url);
  virtual void MarkAsForeignEntry(int host_id, const GURL& document_url,
                                  int64 cache_document_was_loaded_from);
  virtual Status GetStatus(int host_id);
  virtual bool StartUpdate(int host_id);
  virtual bool SwapCache(int host_id);

 private:
  AppCacheService* service_;
  AppCacheFrontend* frontend_;
};

}  // namespace

#endif  // WEBKIT_APPCACHE_APPCACHE_BACKEND_IMPL_H_
