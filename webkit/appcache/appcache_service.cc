// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_service.h"

#include "base/logging.h"
#include "webkit/appcache/appcache_backend_impl.h"
#include "webkit/appcache/appcache_storage_impl.h"

namespace appcache {

AppCacheService::AppCacheService()
    : request_context_(NULL) {
}

AppCacheService::~AppCacheService() {
  DCHECK(backends_.empty());
}

void AppCacheService::Initialize(const FilePath& cache_directory) {
  DCHECK(!storage_.get());
  AppCacheStorageImpl* storage = new AppCacheStorageImpl(this);
  storage->Initialize(cache_directory);
  storage_.reset(storage);
}

void AppCacheService::RegisterBackend(
    AppCacheBackendImpl* backend_impl) {
  DCHECK(backends_.find(backend_impl->process_id()) == backends_.end());
  backends_.insert(
      BackendMap::value_type(backend_impl->process_id(), backend_impl));
}

void AppCacheService::UnregisterBackend(
    AppCacheBackendImpl* backend_impl) {
  backends_.erase(backend_impl->process_id());
}

}  // namespace appcache
