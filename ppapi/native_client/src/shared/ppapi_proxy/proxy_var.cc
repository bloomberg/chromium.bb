// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/proxy_var.h"

#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/untrusted/pthread/pthread.h"

#include <limits>

namespace ppapi_proxy {

namespace {

pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

}  // namespace

int64_t ProxyVar::unique_var_id = 0;

ProxyVar::ProxyVar(PP_VarType pp_var_type) : pp_var_type_(pp_var_type) {
  // Roll id of INT64_MAX to 1, because an id of 0 is not valid.
  nacl::ScopedPthreadMutexLock ml(&mu);
  if (unique_var_id > std::numeric_limits<int64_t>::max() - 1)
    unique_var_id = 0;
  id_ = ++unique_var_id;
}

}  // namespace ppapi_proxy
