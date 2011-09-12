// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/proxy_var_cache.h"

#include <limits>
#include <map>

#include "native_client/src/include/ref_counted.h"
#include "native_client/src/untrusted/pthread/pthread.h"

namespace ppapi_proxy {

namespace {

pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

}  // namespace

ProxyVarCache* ProxyVarCache::cache_singleton = NULL;

ProxyVarCache& ProxyVarCache::GetInstance() {
  // When the deprecated scripting is removed, this crash should disappear.
  // static ProxyVarCache cache_singleton;
  // return cache_singleton;
  pthread_mutex_lock(&mu);
  if (cache_singleton == NULL)
    cache_singleton = new ProxyVarCache();
  pthread_mutex_unlock(&mu);
  return *cache_singleton;
}

void ProxyVarCache::RetainSharedProxyVar(const SharedProxyVar& proxy_var) {
  pthread_mutex_lock(&mu);
  // This implements "insert if absent, retain if present".
  std::pair<ProxyVarDictionary::iterator, bool> insert_result =
      proxy_var_cache_.insert(
          std::pair<int64_t, SharedProxyVar>(proxy_var->id(), proxy_var));
  if (!insert_result.second) {
    // Object already exists, bump the retain count.
    insert_result.first->second->AddRef();
  }
  pthread_mutex_unlock(&mu);
}

void ProxyVarCache::RetainProxyVar(const PP_Var& var) {
  if (!IsCachedType(var))
    return;
  pthread_mutex_lock(&mu);
  ProxyVarDictionary::iterator iter = proxy_var_cache_.find(var.value.as_id);
  if (iter != proxy_var_cache_.end()) {
    iter->second->AddRef();
  }
  pthread_mutex_unlock(&mu);
}

void ProxyVarCache::ReleaseProxyVar(const PP_Var& var) {
  if (!IsCachedType(var))
    return;
  pthread_mutex_lock(&mu);
  ProxyVarDictionary::iterator iter = proxy_var_cache_.find(var.value.as_id);
  if (iter != proxy_var_cache_.end()) {
    // Decrement the reference count by one, as requested.
    iter->second->Release();
    // All var reference count updating happens while mu is held, so the
    // potential race between HasOneRef and AddRef/Release cannot materialize.
    if (iter->second->HasOneRef()) {
      // If there is only one reference, it must be from the dictionary.
      // Delete the dictionary entry.
      proxy_var_cache_.erase(iter);
    }
  }
  pthread_mutex_unlock(&mu);
}

SharedProxyVar ProxyVarCache::SharedProxyVarForVar(PP_Var pp_var) const {
  SharedProxyVar proxy_var;
  pthread_mutex_lock(&mu);
  ProxyVarDictionary::const_iterator iter =
      proxy_var_cache_.find(pp_var.value.as_id);
  if (iter != proxy_var_cache_.end()) {
    proxy_var = iter->second;
  }
  pthread_mutex_unlock(&mu);
  return proxy_var;
}

}  // namespace ppapi_proxy
