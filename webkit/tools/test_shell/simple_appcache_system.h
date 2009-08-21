// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_SIMPLE_APPCACHE_SYSTEM_H_
#define WEBKIT_TOOLS_TEST_SHELL_SIMPLE_APPCACHE_SYSTEM_H_

#include "webkit/appcache/appcache_backend_impl.h"
#include "webkit/appcache/appcache_frontend_impl.h"

class SimpleAppCacheSystem {
 public:
  void Initialize() {
    backend_impl_.Initialize(NULL, &frontend_impl_);
  }

  appcache::AppCacheBackend* backend() { return &backend_impl_; }
  appcache::AppCacheFrontend* frontend() { return &frontend_impl_; }

 private:
  appcache::AppCacheBackendImpl backend_impl_;
  appcache::AppCacheFrontendImpl frontend_impl_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_SIMPLE_APPCACHE_SYSTEM_H_
