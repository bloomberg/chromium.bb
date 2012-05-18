// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PROXY_VAR_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PROXY_VAR_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_memory.h"
#include "native_client/src/include/ref_counted.h"
#include "ppapi/c/pp_var.h"

namespace ppapi_proxy {

// Complex PP_Var types (such as strings) have a copy of the variant's contents
// cached by the proxy.  This is done so that PP_Vars can be reference counted,
// and their contents accessed "locally" by NaCl modules without having to
// perform a complete round trip to the browser for each such operation.
//
// Note: this class is intended to be sub-classed to handle specific content
// types such as strings, dictionaries, or arrays.
class ProxyVar : public nacl::RefCountedThreadSafe<ProxyVar> {
 public:
  // The type of this cached object.  Simple types (int, bool, etc.) are not
  // cached.
  PP_VarType pp_var_type() const { return pp_var_type_; }

  // The assigned unique id associated with this object.  Use this as the id
  // for the corresponding PP_Var.
  int64_t id() const { return id_; }

 protected:
  // Initialize this instance to represent a PP_Var of type |pp_var_type|.
  // Generates a unique id for this instance, and sets the reference count to 1.
  // Subclasses should implement ctors that handle specific content data.
  explicit ProxyVar(PP_VarType pp_var_type);

  virtual ~ProxyVar() {}

 private:
  friend class nacl::RefCountedThreadSafe<ProxyVar>;

  PP_VarType pp_var_type_;
  int64_t id_;

  // A counter for unique ids.
  static int64_t unique_var_id;

  ProxyVar();  // Not implemented - do not use.
  NACL_DISALLOW_COPY_AND_ASSIGN(ProxyVar);
};

typedef scoped_refptr<ProxyVar> SharedProxyVar;

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PROXY_VAR_H_
