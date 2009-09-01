// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_backend_impl.h"

#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/web_application_cache_host_impl.h"

namespace appcache {

AppCacheBackendImpl::~AppCacheBackendImpl() {
  if (service_)
    service_->UnregisterBackend(this);
}

void AppCacheBackendImpl::Initialize(AppCacheService* service,
                                     AppCacheFrontend* frontend,
                                     int process_id) {
  DCHECK(!service_ && !frontend_ && frontend && service);
  service_ = service;
  frontend_ = frontend;
  process_id_ = process_id;
  service_->RegisterBackend(this);
}

void AppCacheBackendImpl::RegisterHost(int id) {
  DCHECK(hosts_.find(id) == hosts_.end());
  hosts_.insert(HostMap::value_type(id, AppCacheHost(id, frontend_)));
}

void AppCacheBackendImpl::UnregisterHost(int id) {
  hosts_.erase(id);
}

void AppCacheBackendImpl::SelectCache(
    int host_id,
    const GURL& document_url,
    const int64 cache_document_was_loaded_from,
    const GURL& manifest_url) {
  // TODO(michaeln): write me
  frontend_->OnCacheSelected(host_id, kNoCacheId, UNCACHED);
}

void AppCacheBackendImpl::MarkAsForeignEntry(
    int host_id,
    const GURL& document_url,
    int64 cache_document_was_loaded_from) {
  // TODO(michaeln): write me
}

void AppCacheBackendImpl::GetStatusWithCallback(
    int host_id, GetStatusCallback* callback, void* callback_param) {
  // TODO(michaeln): write me
  callback->Run(UNCACHED, callback_param);
}

void AppCacheBackendImpl::StartUpdateWithCallback(
    int host_id, StartUpdateCallback* callback, void* callback_param) {
  // TODO(michaeln): write me
  callback->Run(false, callback_param);
}

void AppCacheBackendImpl::SwapCacheWithCallback(
    int host_id, SwapCacheCallback* callback, void* callback_param) {
  // TODO(michaeln): write me
  callback->Run(false, callback_param);
}

}  // namespace appcache
