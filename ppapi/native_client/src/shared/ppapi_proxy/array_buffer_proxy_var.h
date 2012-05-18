// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_ARRAY_BUFFER_PROXY_VAR_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_ARRAY_BUFFER_PROXY_VAR_H_

#include "native_client/src/include/nacl_memory.h"
#include "native_client/src/shared/ppapi_proxy/proxy_var.h"

#include <vector>

namespace ppapi_proxy {

// Subclass of ProxyVar that handles ArrayBuffer objects.
class ArrayBufferProxyVar : public ProxyVar {
 public:
  explicit ArrayBufferProxyVar(uint32_t size_in_bytes)
      : ProxyVar(PP_VARTYPE_ARRAY_BUFFER), buffer_(size_in_bytes, 0) {}

  void* buffer() { return buffer_.empty() ? NULL : &buffer_[0]; }
  uint32_t buffer_length() const { return buffer_.size(); }

  // Convenience function to do type checking and down-casting. This returns a
  // scoped_refptr<>, so you don't have to down-cast the raw pointer.
  static scoped_refptr<ArrayBufferProxyVar> CastFromProxyVar(
      SharedProxyVar proxy_var) {
    if (proxy_var == NULL ||
        proxy_var->pp_var_type() != PP_VARTYPE_ARRAY_BUFFER) {
      scoped_refptr<ArrayBufferProxyVar> null_ptr;
      return null_ptr;
    }
    return scoped_refptr<ArrayBufferProxyVar>(
        static_cast<ArrayBufferProxyVar*>(proxy_var.get()));
  }

 protected:
  virtual ~ArrayBufferProxyVar() {}

 private:
  std::vector<uint8_t> buffer_;
};

typedef scoped_refptr<ArrayBufferProxyVar> SharedArrayBufferProxyVar;

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_ARRAY_BUFFER_PROXY_VAR_H_
