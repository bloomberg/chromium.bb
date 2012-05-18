// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_STRING_PROXY_VAR_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_STRING_PROXY_VAR_H_

#include "native_client/src/include/nacl_memory.h"
#include "native_client/src/shared/ppapi_proxy/proxy_var.h"

#include <string>

namespace ppapi_proxy {

// Subclass of ProxyVar that handles string objects.
class StringProxyVar : public ProxyVar {
 public:
  explicit StringProxyVar(const std::string& contents)
      : ProxyVar(PP_VARTYPE_STRING), contents_(contents) {}

  StringProxyVar(const char* data, size_t len)
      : ProxyVar(PP_VARTYPE_STRING), contents_(data, len) {}

  const std::string& contents() const { return contents_; }

  // Convenience function to do type checking and down-casting. This returns a
  // scoped_refptr<>, so you don't have to down-cast the raw pointer.
  static scoped_refptr<StringProxyVar> CastFromProxyVar(
      SharedProxyVar proxy_var) {
    if (proxy_var == NULL || proxy_var->pp_var_type() != PP_VARTYPE_STRING) {
      scoped_refptr<StringProxyVar> string_var;
      return string_var;
    }
    return scoped_refptr<StringProxyVar>(
        static_cast<StringProxyVar*>(proxy_var.get()));
  }

 protected:
  virtual ~StringProxyVar() {}

 private:
  std::string contents_;
};

typedef scoped_refptr<StringProxyVar> SharedStringProxyVar;

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_STRING_PROXY_VAR_H_
