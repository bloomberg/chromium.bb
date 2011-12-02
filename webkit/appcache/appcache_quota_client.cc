// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_quota_client.h"

#include <algorithm>
#include <map>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "webkit/appcache/appcache_service.h"

using quota::QuotaClient;

namespace {
quota::QuotaStatusCode NetErrorCodeToQuotaStatus(int code) {
  if (code == net::OK)
    return quota::kQuotaStatusOk;
  else if (code == net::ERR_ABORTED)
    return quota::kQuotaErrorAbort;
  else
    return quota::kQuotaStatusUnknown;
}

void RunFront(appcache::AppCacheQuotaClient::RequestQueue* queue) {
  base::Closure request = queue->front();
  queue->pop_front();
  request.Run();
}
}  // namespace

namespace appcache {

AppCacheQuotaClient::AppCacheQuotaClient(AppCacheService* service)
    : ALLOW_THIS_IN_INITIALIZER_LIST(service_delete_callback_(
          base::Bind(&AppCacheQuotaClient::DidDeleteAppCachesForOrigin,
                     base::Unretained(this)))),
      service_(service),
      appcache_is_ready_(false),
      quota_manager_is_destroyed_(false) {
}

AppCacheQuotaClient::~AppCacheQuotaClient() {
  DCHECK(pending_batch_requests_.empty());
  DCHECK(pending_serial_requests_.empty());
  DCHECK(current_delete_request_callback_.is_null());
}

QuotaClient::ID AppCacheQuotaClient::id() const {
  return kAppcache;
}

void AppCacheQuotaClient::OnQuotaManagerDestroyed() {
  DeletePendingRequests();
  if (!current_delete_request_callback_.is_null()) {
    current_delete_request_callback_.Reset();
    service_delete_callback_.Cancel();
  }

  quota_manager_is_destroyed_ = true;
  if (!service_)
    delete this;
}

void AppCacheQuotaClient::GetOriginUsage(
    const GURL& origin,
    quota::StorageType type,
    const GetUsageCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(!quota_manager_is_destroyed_);

  if (!service_) {
    callback.Run(0);
    return;
  }

  if (!appcache_is_ready_) {
    pending_batch_requests_.push_back(
        base::Bind(&AppCacheQuotaClient::GetOriginUsage,
                   base::Unretained(this), origin, type, callback));
    return;
  }

  if (type == quota::kStorageTypePersistent) {
    callback.Run(0);
    return;
  }

  const AppCacheStorage::UsageMap* map = GetUsageMap();
  AppCacheStorage::UsageMap::const_iterator found = map->find(origin);
  if (found == map->end()) {
    callback.Run(0);
    return;
  }
  callback.Run(found->second);
}

void AppCacheQuotaClient::GetOriginsForType(
    quota::StorageType type,
    const GetOriginsCallback& callback) {
  GetOriginsHelper(type, std::string(), callback);
}

void AppCacheQuotaClient::GetOriginsForHost(
    quota::StorageType type,
    const std::string& host,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());
  if (host.empty()) {
    callback.Run(std::set<GURL>(), type);
    return;
  }
  GetOriginsHelper(type, host, callback);
}

void AppCacheQuotaClient::DeleteOriginData(const GURL& origin,
                                           quota::StorageType type,
                                           const DeletionCallback& callback) {
  DCHECK(!quota_manager_is_destroyed_);

  if (!service_) {
    callback.Run(quota::kQuotaErrorAbort);
    return;
  }

  if (!appcache_is_ready_ || !current_delete_request_callback_.is_null()) {
    pending_serial_requests_.push_back(
        base::Bind(&AppCacheQuotaClient::DeleteOriginData,
                   base::Unretained(this), origin, type, callback));
    return;
  }

  current_delete_request_callback_ = callback;
  if (type == quota::kStorageTypePersistent) {
    DidDeleteAppCachesForOrigin(net::OK);
    return;
  }

  service_->DeleteAppCachesForOrigin(
      origin, service_delete_callback_.callback());
}

void AppCacheQuotaClient::DidDeleteAppCachesForOrigin(int rv) {
  DCHECK(service_);
  if (quota_manager_is_destroyed_)
    return;

  // Finish the request by calling our callers callback.
  current_delete_request_callback_.Run(NetErrorCodeToQuotaStatus(rv));
  current_delete_request_callback_.Reset();
  if (pending_serial_requests_.empty())
    return;

  // Start the next in the queue.
  RunFront(&pending_serial_requests_);
}

void AppCacheQuotaClient::GetOriginsHelper(
    quota::StorageType type,
    const std::string& opt_host,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(!quota_manager_is_destroyed_);

  if (!service_) {
    callback.Run(std::set<GURL>(), type);
    return;
  }

  if (!appcache_is_ready_) {
    pending_batch_requests_.push_back(
        base::Bind(&AppCacheQuotaClient::GetOriginsHelper,
                   base::Unretained(this), type, opt_host, callback));
    return;
  }

  if (type == quota::kStorageTypePersistent) {
    callback.Run(std::set<GURL>(), type);
    return;
  }

  const AppCacheStorage::UsageMap* map = GetUsageMap();
  std::set<GURL> origins;
  for (AppCacheStorage::UsageMap::const_iterator iter = map->begin();
       iter != map->end(); ++iter) {
    if (opt_host.empty() || iter->first.host() == opt_host)
      origins.insert(iter->first);
  }
  callback.Run(origins, type);
}

void AppCacheQuotaClient::ProcessPendingRequests() {
  DCHECK(appcache_is_ready_);
  while (!pending_batch_requests_.empty())
    RunFront(&pending_batch_requests_);

  if (!pending_serial_requests_.empty())
    RunFront(&pending_serial_requests_);
}

void AppCacheQuotaClient::DeletePendingRequests() {
  pending_batch_requests_.clear();
  pending_serial_requests_.clear();
}

const AppCacheStorage::UsageMap* AppCacheQuotaClient::GetUsageMap() {
  DCHECK(service_);
  return service_->storage()->usage_map();
}

void AppCacheQuotaClient::NotifyAppCacheReady() {
  appcache_is_ready_ = true;
  ProcessPendingRequests();
}

void AppCacheQuotaClient::NotifyAppCacheDestroyed() {
  service_ = NULL;
  while (!pending_batch_requests_.empty())
    RunFront(&pending_batch_requests_);

  while (!pending_serial_requests_.empty())
    RunFront(&pending_serial_requests_);

  if (!current_delete_request_callback_.is_null()) {
    current_delete_request_callback_.Run(quota::kQuotaErrorAbort);
    current_delete_request_callback_.Reset();
    service_delete_callback_.Cancel();
  }

  if (quota_manager_is_destroyed_)
    delete this;
}

}  // namespace appcache
