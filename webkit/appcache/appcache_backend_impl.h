// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_BACKEND_IMPL_H_
#define WEBKIT_APPCACHE_APPCACHE_BACKEND_IMPL_H_

#include <map>

#include "base/logging.h"
#include "base/task.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_interfaces.h"

namespace appcache {

class AppCacheService;

typedef Callback2<Status, void*>::Type GetStatusCallback;
typedef Callback2<bool, void*>::Type StartUpdateCallback;
typedef Callback2<bool, void*>::Type SwapCacheCallback;

class AppCacheBackendImpl : public AppCacheBackend {
 public:
  AppCacheBackendImpl() : service_(NULL), frontend_(NULL), process_id_(0) {}
  ~AppCacheBackendImpl();

  void Initialize(AppCacheService* service,
                  AppCacheFrontend* frontend,
                  int process_id);

  int process_id() const { return process_id_; }

  // Returns a pointer to a registered host. The backend retains ownership.
  AppCacheHost* GetHost(int host_id) {
    HostMap::iterator it = hosts_.find(host_id);
    return (it != hosts_.end()) ? &(it->second) : NULL;
  }

  typedef std::map<int, AppCacheHost> HostMap;
  const HostMap& hosts() { return hosts_; }

  // AppCacheBackend methods
  virtual void RegisterHost(int host_id);
  virtual void UnregisterHost(int host_id);
  virtual void SelectCache(int host_id,
                           const GURL& document_url,
                           const int64 cache_document_was_loaded_from,
                           const GURL& manifest_url);
  virtual void MarkAsForeignEntry(int host_id, const GURL& document_url,
                                  int64 cache_document_was_loaded_from);

  // We don't use the sync variants in the backend, would block the IO thread.
  virtual Status GetStatus(int host_id) { NOTREACHED(); return UNCACHED; }
  virtual bool StartUpdate(int host_id) { NOTREACHED(); return false; }
  virtual bool SwapCache(int host_id) { NOTREACHED(); return false; }

  // Async variants of the sync methods defined in the backend interface.
  void GetStatusWithCallback(int host_id, GetStatusCallback* callback,
                             void* callback_param);
  void StartUpdateWithCallback(int host_id, StartUpdateCallback* callback,
                               void* callback_param);
  void SwapCacheWithCallback(int host_id, SwapCacheCallback* callback,
                             void* callback_param);

 private:
  AppCacheService* service_;
  AppCacheFrontend* frontend_;
  int process_id_;
  HostMap hosts_;
};

}  // namespace

#endif  // WEBKIT_APPCACHE_APPCACHE_BACKEND_IMPL_H_
