// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJECT_PROXY_VAR_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJECT_PROXY_VAR_H_

#include "native_client/src/shared/ppapi_proxy/proxy_var.h"

#include <utility>

#include "native_client/src/third_party/ppapi/c/dev/ppb_var_deprecated.h"
#include "native_client/src/third_party/ppapi/c/dev/ppp_class_deprecated.h"

namespace ppapi_proxy {

// Subclass of ProxyVar that handles (deprecated) scriptable objects.  A
// scriptable object is represented by a pair consisting of the object class
// structure and the object data.  This class takes over ownership of the
// pointers used in the <class, object> pair.  The memory is freed in the dtor.
class ObjectProxyVar : public ProxyVar {
 public:
  typedef std::pair<const PPP_Class_Deprecated*, void*> ObjectPair;

  // It is possible for |contents.first| to be NULL.  In this case, it's
  // assumed that |contents.second| was allocated on the heap using malloc().
  // If |contents.first| is non-NULL, then it's assumed to point to a valid
  // PPP_Class_Deprecated struct that lives at least as long as this object
  // instance lives.
  explicit ObjectProxyVar(const ObjectPair& contents)
      : ProxyVar(PP_VARTYPE_OBJECT), contents_(contents) {}

  // Free the memory used in the <class, object> pair.  If |contents().first|
  // is NULL, then assume that the object was created on the heap using
  // malloc().
  virtual ~ObjectProxyVar() {
    if (contents().first == NULL) {
      free(contents().second);
    } else {
      contents().first->Deallocate(contents().second);
    }
  }

  // Convenience function to do type checking and down-casting. This returns a
  // shared_ptr<>, so you don't have to down-cast the raw pointer.
  static std::tr1::shared_ptr<ObjectProxyVar> CastFromProxyVar(
      const SharedProxyVar& proxy_var) {
    if (proxy_var == NULL || proxy_var->pp_var_type() != PP_VARTYPE_OBJECT) {
      std::tr1::shared_ptr<ObjectProxyVar> object_var;
      return object_var;
    }
    return std::tr1::static_pointer_cast<ObjectProxyVar>(proxy_var);
  }

  const ObjectPair& contents() const { return contents_; }

 private:
  ObjectPair contents_;

  ObjectProxyVar();  // Not implemented, do not use.
};

typedef std::tr1::shared_ptr<ObjectProxyVar> SharedObjectProxyVar;

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJECT_PROXY_VAR_H_
