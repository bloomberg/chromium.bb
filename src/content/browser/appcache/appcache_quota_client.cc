// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_quota_client.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

using blink::mojom::StorageType;

namespace content {
namespace {
blink::mojom::QuotaStatusCode NetErrorCodeToQuotaStatus(int code) {
  if (code == net::OK)
    return blink::mojom::QuotaStatusCode::kOk;
  else if (code == net::ERR_ABORTED)
    return blink::mojom::QuotaStatusCode::kErrorAbort;
  else
    return blink::mojom::QuotaStatusCode::kUnknown;
}

void RunFront(content::AppCacheQuotaClient::RequestQueue* queue) {
  base::OnceClosure request = std::move(queue->front());
  queue->pop_front();
  std::move(request).Run();
}

void RunDeleteOnIO(const base::Location& from_here,
                   net::CompletionRepeatingCallback callback,
                   int result) {
  base::PostTask(from_here, {BrowserThread::IO},
                 base::BindOnce(std::move(callback), result));
}
}  // namespace

AppCacheQuotaClient::AppCacheQuotaClient(
    base::WeakPtr<AppCacheServiceImpl> service)
    : service_(std::move(service)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AppCacheQuotaClient::~AppCacheQuotaClient() {
  DCHECK(pending_batch_requests_.empty());
  DCHECK(pending_serial_requests_.empty());
  DCHECK(current_delete_request_callback_.is_null());
}

storage::QuotaClientType AppCacheQuotaClient::type() const {
  return storage::QuotaClientType::kAppcache;
}

void AppCacheQuotaClient::OnQuotaManagerDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DeletePendingRequests();
  if (!current_delete_request_callback_.is_null()) {
    current_delete_request_callback_.Reset();
    GetServiceDeleteCallback()->Cancel();
  }
}

void AppCacheQuotaClient::GetOriginUsage(const url::Origin& origin,
                                         StorageType type,
                                         GetUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  if (service_is_destroyed_) {
    std::move(callback).Run(0);
    return;
  }

  if (!appcache_is_ready_) {
    pending_batch_requests_.push_back(base::BindOnce(
        &AppCacheQuotaClient::GetOriginUsage, base::RetainedRef(this), origin,
        type, std::move(callback)));
    return;
  }

  if (type != StorageType::kTemporary) {
    std::move(callback).Run(0);
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          [](base::WeakPtr<AppCacheServiceImpl> service,
             const url::Origin& origin) -> int64_t {
            if (!service)
              return 0;

            const std::map<url::Origin, int64_t>& map =
                service->storage()->usage_map();
            auto it = map.find(origin);
            if (it == map.end())
              return 0;

            return it->second;
          },
          service_, origin),
      std::move(callback));
}

void AppCacheQuotaClient::GetOriginsForType(StorageType type,
                                            GetOriginsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetOriginsHelper(type, std::string(), std::move(callback));
}

void AppCacheQuotaClient::GetOriginsForHost(StorageType type,
                                            const std::string& host,
                                            GetOriginsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  if (host.empty()) {
    std::move(callback).Run(std::set<url::Origin>());
    return;
  }
  GetOriginsHelper(type, host, std::move(callback));
}

void AppCacheQuotaClient::DeleteOriginData(const url::Origin& origin,
                                           StorageType type,
                                           DeletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (service_is_destroyed_) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorAbort);
    return;
  }

  if (!appcache_is_ready_ || !current_delete_request_callback_.is_null()) {
    pending_serial_requests_.push_back(base::BindOnce(
        &AppCacheQuotaClient::DeleteOriginData, base::RetainedRef(this), origin,
        type, std::move(callback)));
    return;
  }

  current_delete_request_callback_ = std::move(callback);
  if (type != StorageType::kTemporary) {
    DidDeleteAppCachesForOrigin(net::OK);
    return;
  }

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&AppCacheServiceImpl::DeleteAppCachesForOrigin, service_,
                     origin,
                     base::BindOnce(&RunDeleteOnIO, FROM_HERE,
                                    GetServiceDeleteCallback()->callback())));
}

void AppCacheQuotaClient::PerformStorageCleanup(blink::mojom::StorageType type,
                                                base::OnceClosure callback) {
  std::move(callback).Run();
}

bool AppCacheQuotaClient::DoesSupport(StorageType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return type == StorageType::kTemporary;
}

void AppCacheQuotaClient::DidDeleteAppCachesForOrigin(int rv) {
  // Finish the request by calling our callers callback.
  std::move(current_delete_request_callback_)
      .Run(NetErrorCodeToQuotaStatus(rv));
  if (pending_serial_requests_.empty())
    return;

  // Start the next in the queue.
  RunFront(&pending_serial_requests_);
}

void AppCacheQuotaClient::GetOriginsHelper(StorageType type,
                                           const std::string& opt_host,
                                           GetOriginsCallback callback) {
  DCHECK(!callback.is_null());

  if (service_is_destroyed_) {
    std::move(callback).Run(std::set<url::Origin>());
    return;
  }

  if (!appcache_is_ready_) {
    pending_batch_requests_.push_back(base::BindOnce(
        &AppCacheQuotaClient::GetOriginsHelper, base::RetainedRef(this), type,
        opt_host, std::move(callback)));
    return;
  }

  if (type != StorageType::kTemporary) {
    std::move(callback).Run(std::set<url::Origin>());
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          [](base::WeakPtr<AppCacheServiceImpl> service,
             const std::string& opt_host) {
            std::set<url::Origin> origins;
            if (!service)
              return origins;

            for (const auto& pair : service->storage()->usage_map()) {
              if (opt_host.empty() || pair.first.host() == opt_host)
                origins.insert(pair.first);
            }
            return origins;
          },
          service_, opt_host),
      std::move(callback));
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

net::CancelableCompletionRepeatingCallback*
AppCacheQuotaClient::GetServiceDeleteCallback() {
  // Lazily created due to base::CancelableCallback's threading restrictions,
  // there is no way to detach from the thread created on.
  if (!service_delete_callback_) {
    service_delete_callback_ =
        std::make_unique<net::CancelableCompletionRepeatingCallback>(
            base::BindRepeating(
                &AppCacheQuotaClient::DidDeleteAppCachesForOrigin,
                base::RetainedRef(this)));
  }
  return service_delete_callback_.get();
}

void AppCacheQuotaClient::NotifyAppCacheReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Can reoccur during reinitialization.
  if (!appcache_is_ready_) {
    appcache_is_ready_ = true;
    ProcessPendingRequests();
  }
}

void AppCacheQuotaClient::NotifyAppCacheDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  service_ = nullptr;
  service_is_destroyed_ = true;
  while (!pending_batch_requests_.empty())
    RunFront(&pending_batch_requests_);

  while (!pending_serial_requests_.empty())
    RunFront(&pending_serial_requests_);

  if (!current_delete_request_callback_.is_null()) {
    std::move(current_delete_request_callback_)
        .Run(blink::mojom::QuotaStatusCode::kErrorAbort);
    GetServiceDeleteCallback()->Cancel();
  }

  if (service_delete_callback_)
    service_delete_callback_.reset();
}

}  // namespace content
