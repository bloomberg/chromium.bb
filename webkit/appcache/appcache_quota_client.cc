// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_quota_client.h"

#include <algorithm>
#include <map>
#include <set>

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
}  // namespace

namespace appcache {

AppCacheQuotaClient::AppCacheQuotaClient(AppCacheService* service)
    : ALLOW_THIS_IN_INITIALIZER_LIST(service_delete_callback_(
          new net::CancelableOldCompletionCallback<AppCacheQuotaClient>(
              this, &AppCacheQuotaClient::DidDeleteAppCachesForOrigin))),
      service_(service), appcache_is_ready_(false),
      quota_manager_is_destroyed_(false) {
}

AppCacheQuotaClient::~AppCacheQuotaClient() {
  DCHECK(pending_usage_requests_.empty());
  DCHECK(pending_origins_requests_.empty());
  DCHECK(pending_delete_requests_.empty());
  DCHECK(current_delete_request_callback_.is_null());
}

QuotaClient::ID AppCacheQuotaClient::id() const {
  return kAppcache;
}

void AppCacheQuotaClient::OnQuotaManagerDestroyed() {
  DeletePendingRequests();
  if (!current_delete_request_callback_.is_null()) {
    current_delete_request_callback_.Reset();
    service_delete_callback_.release()->Cancel();
  } else {
    service_delete_callback_ = NULL;
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
    pending_usage_requests_.push_back(UsageRequest());
    pending_usage_requests_.back().origin = origin;
    pending_usage_requests_.back().type = type;
    pending_usage_requests_.back().callback = callback;
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
    pending_delete_requests_.push_back(DeleteRequest());
    pending_delete_requests_.back().origin = origin;
    pending_delete_requests_.back().type = type;
    pending_delete_requests_.back().callback = callback;
    return;
  }

  current_delete_request_callback_ = callback;
  if (type == quota::kStorageTypePersistent) {
    DidDeleteAppCachesForOrigin(net::OK);
    return;
  }
  service_->DeleteAppCachesForOrigin(origin, service_delete_callback_);
}

AppCacheQuotaClient::UsageRequest::UsageRequest() {}
AppCacheQuotaClient::UsageRequest::~UsageRequest() {}

AppCacheQuotaClient::OriginsRequest::OriginsRequest() {}
AppCacheQuotaClient::OriginsRequest::~OriginsRequest() {}

AppCacheQuotaClient::DeleteRequest::DeleteRequest() {}
AppCacheQuotaClient::DeleteRequest::~DeleteRequest() {}

void AppCacheQuotaClient::DidDeleteAppCachesForOrigin(int rv) {
  DCHECK(service_);
  if (quota_manager_is_destroyed_)
    return;

  // Finish the request by calling our callers callback.
  current_delete_request_callback_.Run(NetErrorCodeToQuotaStatus(rv));
  current_delete_request_callback_.Reset();
  if (pending_delete_requests_.empty())
    return;

  // Start the next in the queue.
  DeleteRequest& next_request = pending_delete_requests_.front();
  current_delete_request_callback_ = next_request.callback;
  service_->DeleteAppCachesForOrigin(next_request.origin,
                                     service_delete_callback_);
  pending_delete_requests_.pop_front();
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
    pending_origins_requests_.push_back(OriginsRequest());
    pending_origins_requests_.back().opt_host = opt_host;
    pending_origins_requests_.back().type = type;
    pending_origins_requests_.back().callback = callback;
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
  while (!pending_usage_requests_.empty()) {
    UsageRequest& request = pending_usage_requests_.front();
    GetOriginUsage(request.origin, request.type, request.callback);
    pending_usage_requests_.pop_front();
  }
  while (!pending_origins_requests_.empty()) {
    OriginsRequest& request = pending_origins_requests_.front();
    GetOriginsHelper(request.type, request.opt_host, request.callback);
    pending_origins_requests_.pop_front();
  }
  if (!pending_delete_requests_.empty()) {
    // Just start the first delete, others will follow upon completion.
    DeleteRequest request = pending_delete_requests_.front();
    pending_delete_requests_.pop_front();
    DeleteOriginData(request.origin, request.type, request.callback);
  }
}

void AppCacheQuotaClient::AbortPendingRequests() {
  while (!pending_usage_requests_.empty()) {
    pending_usage_requests_.front().callback.Run(0);
    pending_usage_requests_.pop_front();
  }
  while (!pending_origins_requests_.empty()) {
    pending_origins_requests_.front().callback.Run(std::set<GURL>(),
        pending_origins_requests_.front().type);
    pending_origins_requests_.pop_front();
  }
  while (!pending_delete_requests_.empty()) {
    pending_delete_requests_.front().callback.Run(quota::kQuotaErrorAbort);
    pending_delete_requests_.pop_front();
  }
}

void AppCacheQuotaClient::DeletePendingRequests() {
  while (!pending_usage_requests_.empty()) {
    pending_usage_requests_.pop_front();
  }
  while (!pending_origins_requests_.empty()) {
    pending_origins_requests_.pop_front();
  }
  while (!pending_delete_requests_.empty()) {
    pending_delete_requests_.pop_front();
  }
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
  AbortPendingRequests();
  if (!current_delete_request_callback_.is_null()) {
    current_delete_request_callback_.Run(quota::kQuotaErrorAbort);
    current_delete_request_callback_.Reset();
    service_delete_callback_.release()->Cancel();
  } else {
    service_delete_callback_ = NULL;
  }
  if (quota_manager_is_destroyed_)
    delete this;
}

}  // namespace appcache
