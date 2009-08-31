// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_backend_impl.h"

#include "base/logging.h"
#include "webkit/appcache/web_application_cache_host_impl.h"

namespace appcache {

AppCacheBackendImpl::~AppCacheBackendImpl() {
  // TODO(michaeln): if (service_) service_->UnregisterFrontend(frontend_);
}

void AppCacheBackendImpl::Initialize(AppCacheService* service,
                                     AppCacheFrontend* frontend) {
  DCHECK(!service_ && !frontend_ && frontend);  // DCHECK(service)
  service_ = service;
  frontend_ = frontend;
  // TODO(michaeln): service_->RegisterFrontend(frontend_);
}

void AppCacheBackendImpl::RegisterHost(int host_id) {
  // TODO(michaeln): plumb to service
}

void AppCacheBackendImpl::UnregisterHost(int host_id) {
  // TODO(michaeln): plumb to service
}

void AppCacheBackendImpl::SelectCache(
    int host_id,
    const GURL& document_url,
    const int64 cache_document_was_loaded_from,
    const GURL& manifest_url) {
  // TODO(michaeln): plumb to service
  frontend_->OnCacheSelected(host_id, kNoCacheId, UNCACHED);
}

void AppCacheBackendImpl::MarkAsForeignEntry(
    int host_id,
    const GURL& document_url,
    int64 cache_document_was_loaded_from) {
  // TODO(michaeln): plumb to service
}

Status AppCacheBackendImpl::GetStatus(int host_id) {
  // TODO(michaeln): plumb to service
  return UNCACHED;
}

bool AppCacheBackendImpl::StartUpdate(int host_id) {
  // TODO(michaeln): plumb to service
  return false;
}

bool AppCacheBackendImpl::SwapCache(int host_id) {
  // TODO(michaeln): plumb to service
  return false;
}

}  // namespace appcache
