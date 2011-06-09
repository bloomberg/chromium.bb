// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/proxy_var_cache.h"

#include <limits>
#include <map>

namespace ppapi_proxy {

ProxyVarCache& ProxyVarCache::GetInstance() {
  // TODO(dspringer): When PPB_Var_Deprecated support is pulled out of the
  // proxy, uncomment these two lines and remove the lines after it that create
  // the cache on the heap.  The cache leaks a scriptable object, which causes
  // a crash on exit because the shared_ptr dtors get called; the object in
  // question is an ObjectProxyVar, which tried to call through its
  // PPB_Class_Deprecated structure, which has already been freed by this point.
  // When the deprecated scripting is removed, this crash should disappear.
  // static ProxyVarCache cache_singleton;
  // return cache_singleton;
  static ProxyVarCache* cache_singleton = NULL;
  if (cache_singleton == NULL)
    cache_singleton = new ProxyVarCache();
  return *cache_singleton;
}

void ProxyVarCache::RetainSharedProxyVar(const SharedProxyVar& proxy_var) {
  // This implements "insert if absent, retain if present".
  std::pair<ProxyVarDictionary::iterator,
            bool> insert_result =
      proxy_var_cache_.insert(
          std::pair<int64_t, SharedProxyVar>(proxy_var->id(), proxy_var));
  if (!insert_result.second) {
    // Object already exists, bump the retain count.
    insert_result.first->second->Retain();
  }
}

void ProxyVarCache::RetainProxyVar(const PP_Var& var) {
  if (!IsCachedType(var))
    return;
  ProxyVarDictionary::iterator iter = proxy_var_cache_.find(var.value.as_id);
  if (iter != proxy_var_cache_.end()) {
    iter->second->Retain();
  }
}

void ProxyVarCache::ReleaseProxyVar(const PP_Var& var) {
  if (!IsCachedType(var))
    return;
  ProxyVarDictionary::iterator iter = proxy_var_cache_.find(var.value.as_id);
  if (iter != proxy_var_cache_.end()) {
    if (iter->second->Release()) {
      proxy_var_cache_.erase(iter);
    }
  }
}

SharedProxyVar ProxyVarCache::SharedProxyVarForVar(PP_Var pp_var) const {
  SharedProxyVar proxy_var;
  ProxyVarDictionary::const_iterator iter =
      proxy_var_cache_.find(pp_var.value.as_id);
  if (iter != proxy_var_cache_.end()) {
    proxy_var = iter->second;
  }
  return proxy_var;
}

}  // namespace ppapi_proxy
