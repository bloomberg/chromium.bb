// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_BACKEND_IMPL_H_
#define WEBKIT_APPCACHE_APPCACHE_BACKEND_IMPL_H_

#include "base/hash_tables.h"
#include "webkit/appcache/appcache_host.h"

namespace appcache {

class AppCacheService;

class AppCacheBackendImpl {
 public:
  AppCacheBackendImpl() : service_(NULL), frontend_(NULL), process_id_(0) {}
  ~AppCacheBackendImpl();

  void Initialize(AppCacheService* service,
                  AppCacheFrontend* frontend,
                  int process_id);

  int process_id() const { return process_id_; }

  // Methods to support the AppCacheBackend interface. A false return
  // value indicates an invalid host_id and that no action was taken
  // by the backend impl.
  bool RegisterHost(int host_id);
  bool UnregisterHost(int host_id);
  bool SelectCache(int host_id,
                   const GURL& document_url,
                   const int64 cache_document_was_loaded_from,
                   const GURL& manifest_url);
  bool MarkAsForeignEntry(int host_id, const GURL& document_url,
                          int64 cache_document_was_loaded_from);
  bool GetStatusWithCallback(int host_id, GetStatusCallback* callback,
                             void* callback_param);
  bool StartUpdateWithCallback(int host_id, StartUpdateCallback* callback,
                               void* callback_param);
  bool SwapCacheWithCallback(int host_id, SwapCacheCallback* callback,
                             void* callback_param);

  // Returns a pointer to a registered host. The backend retains ownership.
  AppCacheHost* GetHost(int host_id) {
    HostMap::iterator it = hosts_.find(host_id);
    return (it != hosts_.end()) ? (it->second) : NULL;
  }

  typedef base::hash_map<int, AppCacheHost*> HostMap;
  const HostMap& hosts() { return hosts_; }

 private:
  AppCacheService* service_;
  AppCacheFrontend* frontend_;
  int process_id_;
  HostMap hosts_;
};

}  // namespace

#endif  // WEBKIT_APPCACHE_APPCACHE_BACKEND_IMPL_H_
