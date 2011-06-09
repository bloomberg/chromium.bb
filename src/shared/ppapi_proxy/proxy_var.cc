// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/proxy_var.h"

#include <limits>

namespace ppapi_proxy {

ProxyVar::ProxyVar(PP_VarType pp_var_type)
    : pp_var_type_(pp_var_type), ref_count_(1) {
  // Roll id of to 1, because an id of 0 is not valid.
  // TODO(dspringer): make this thread-safe.
  if (unique_var_id_ > std::numeric_limits<int64_t>::max() - 1)
    unique_var_id_ = 0;
  id_ = ++unique_var_id_;
}

void ProxyVar::Retain() {
  // TODO(dspringer): make this thread-safe.
  ++ref_count_;
}

bool ProxyVar::Release() {
  // TODO(dspringer): make this thread-safe.
  if (ref_count_ > 0)
    --ref_count_;
  return ref_count_ == 0;
}

int64_t ProxyVar::unique_var_id_ = 0;

}  // namespace ppapi_proxy
