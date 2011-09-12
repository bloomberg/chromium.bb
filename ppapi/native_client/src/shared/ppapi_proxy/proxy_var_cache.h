// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PROXY_VAR_CACHE_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PROXY_VAR_CACHE_H_

#include <map>

#include "native_client/src/include/nacl_memory.h"
#include "native_client/src/shared/ppapi_proxy/proxy_var.h"
#include "ppapi/c/pp_var.h"

namespace ppapi_proxy {

// This class manages the proxy-local cache of ProxyVars. The base factory
// method generates a unique id that gets used as the PP_Var's id, this id is
// associated with the variant's content.  The factory also inserts the new
// instance into the proxy-local cache.
// Note: There is one proxy var cache per NaCl module instance.
class ProxyVarCache {
 public:
  // Return the global proxy var cache.  Always returns a valid (possibly
  // empty) cache.
  static ProxyVarCache& GetInstance();

  // If |proxy_var| already exists in the cache, then increment its reference
  // count.  Otherwise, add it to the cache.  This method does not check the
  // validity of |proxy_var|, assuming that it's not possible to make a
  // SharedProxyVar unless you use one of the cacheable subclasses.
  // TODO(dspringer): Should the contents of the proxy_var in the cache be
  // replaced with |proxy_var|?  Should there be some kind of assert if an
  // object with the same id exists in the cache but the contents are different?
  void RetainSharedProxyVar(const SharedProxyVar& proxy_var);

  // Find the proxy var associated with |id| and increment its ref count.  Does
  // nothing if no such object exists.  This only operates on vars that are
  // cached (that is, IsCachedType() returns |true|).  Any other var type is
  // not cached, and this function does nothing.
  void RetainProxyVar(const PP_Var& var);

  // Release the cached object associated with |id|.  If the reference count
  // of the object falls to 0, it gets removed from the cache.  This only
  // operates on vars that are cached (that is, IsCachedType() returns |true|).
  // Any other var type is ignored, and this function does nothing.
  void ReleaseProxyVar(const PP_Var& var);

  // Find the object in the cache associated with |pp_var|.
  SharedProxyVar SharedProxyVarForVar(PP_Var pp_var) const;

 private:
  // Return whether or not a var type is cached.
  // Note to implementers: be sure to add to this function when adding new
  // cached types.
  // TODO(dspringer): When all the complex var types are handled, this
  // test can turn into something like var.type >= PP_VARTYPE_STRING.
  bool IsCachedType(const PP_Var& var) {
    return var.type == PP_VARTYPE_STRING || var.type == PP_VARTYPE_OBJECT;
  }

  // The cache of these objects.  The value is a shared pointer so that
  // instances of subclasses can be inserted.
  typedef std::map<int64_t, SharedProxyVar> ProxyVarDictionary;
  ProxyVarDictionary proxy_var_cache_;

  static ProxyVarCache* cache_singleton;

  ProxyVarCache() {}
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PROXY_VAR_CACHE_H_
