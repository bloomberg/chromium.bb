// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_context.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_metrics.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_registration_notifier.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/background_fetch/background_fetch_scheduler.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace content {

using FailureReason = blink::mojom::BackgroundFetchFailureReason;

BackgroundFetchContext::BackgroundFetchContext(
    BrowserContext* browser_context,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<content::CacheStorageContextImpl>&
        cache_storage_context,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : browser_context_(browser_context),
      service_worker_context_(service_worker_context),
      registration_notifier_(
          std::make_unique<BackgroundFetchRegistrationNotifier>()),
      delegate_proxy_(browser_context_->GetBackgroundFetchDelegate()),
      weak_factory_(this) {
  // Although this lives only on the IO thread, it is constructed on UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(service_worker_context_);

  data_manager_ = std::make_unique<BackgroundFetchDataManager>(
      browser_context_, service_worker_context, cache_storage_context,
      std::move(quota_manager_proxy));
  scheduler_ = std::make_unique<BackgroundFetchScheduler>(
      data_manager_.get(), registration_notifier_.get(), &delegate_proxy_,
      service_worker_context_);
}

BackgroundFetchContext::~BackgroundFetchContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->RemoveObserver(scheduler_.get());
  data_manager_->RemoveObserver(scheduler_.get());
}

void BackgroundFetchContext::InitializeOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_->AddObserver(scheduler_.get());

  data_manager_->AddObserver(scheduler_.get());
  data_manager_->InitializeOnIOThread();
  data_manager_->GetInitializationData(
      base::BindOnce(&BackgroundFetchContext::DidGetInitializationData,
                     weak_factory_.GetWeakPtr()));
}

void BackgroundFetchContext::DidGetInitializationData(
    blink::mojom::BackgroundFetchError error,
    std::vector<background_fetch::BackgroundFetchInitializationData>
        initialization_data) {
  if (error != blink::mojom::BackgroundFetchError::NONE)
    return;

  background_fetch::RecordRegistrationsOnStartup(initialization_data.size());

  for (auto& data : initialization_data) {
    for (auto& observer : data_manager_->observers()) {
      observer.OnRegistrationLoadedAtStartup(
          data.registration_id, data.registration, data.options, data.icon,
          data.num_completed_requests, data.num_requests,
          data.active_fetch_requests);
    }
  }
}

void BackgroundFetchContext::GetRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->GetRegistration(
      service_worker_registration_id, origin, developer_id,
      base::BindOnce(&BackgroundFetchContext::DidGetRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundFetchContext::GetDeveloperIdsForServiceWorker(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->GetDeveloperIdsForServiceWorker(service_worker_registration_id,
                                                 origin, std::move(callback));
}

void BackgroundFetchContext::DidGetRegistration(
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback,
    blink::mojom::BackgroundFetchError error,
    const BackgroundFetchRegistration& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != blink::mojom::BackgroundFetchError::NONE) {
    std::move(callback).Run(error,
                            base::nullopt /* BackgroundFetchRegistration */);
    return;
  }

  BackgroundFetchRegistration updated_registration(registration);
  for (auto& observer : data_manager_->observers())
    observer.OnRegistrationQueried(&updated_registration);

  std::move(callback).Run(error, updated_registration);
}

void BackgroundFetchContext::StartFetch(
    const BackgroundFetchRegistrationId& registration_id,
    std::vector<blink::mojom::FetchAPIRequestPtr> requests,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    RenderFrameHost* render_frame_host,
    blink::mojom::BackgroundFetchService::FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |registration_id| should be unique even if developer id has been
  // duplicated, because the caller of this function generates a new unique_id
  // every time, which is what BackgroundFetchRegistrationId's comparison
  // operator uses.
  DCHECK_EQ(0u, fetch_callbacks_.count(registration_id));
  fetch_callbacks_[registration_id] = std::move(callback);
  int frame_tree_node_id =
      render_frame_host ? render_frame_host->GetFrameTreeNodeId() : 0;

  GetPermissionForOrigin(
      registration_id.origin(), render_frame_host,
      base::BindOnce(&BackgroundFetchContext::DidGetPermission,
                     weak_factory_.GetWeakPtr(), registration_id,
                     std::move(requests), options, icon, std::move(ukm_data),
                     frame_tree_node_id));
}

void BackgroundFetchContext::GetPermissionForOrigin(
    const url::Origin& origin,
    RenderFrameHost* render_frame_host,
    GetPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ResourceRequestInfo::WebContentsGetter wc_getter = base::NullCallback();

  // Permissions need to go through the DownloadRequestLimiter if the fetch
  // is started from a top-level frame.
  if (render_frame_host && !render_frame_host->GetParent()) {
    wc_getter = base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                                    render_frame_host->GetFrameTreeNodeId());
  }

  delegate_proxy_.GetPermissionForOrigin(origin, std::move(wc_getter),
                                         std::move(callback));
}

void BackgroundFetchContext::DidGetPermission(
    const BackgroundFetchRegistrationId& registration_id,
    std::vector<blink::mojom::FetchAPIRequestPtr> requests,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    int frame_tree_node_id,
    BackgroundFetchPermission permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&background_fetch::RecordBackgroundFetchUkmEvent,
                     registration_id.origin(), requests.size(), options, icon,
                     std::move(ukm_data), frame_tree_node_id, permission));

  if (permission != BackgroundFetchPermission::BLOCKED) {
    data_manager_->CreateRegistration(
        registration_id, std::move(requests), options, icon,
        permission == BackgroundFetchPermission::ASK /* start_paused */,
        base::BindOnce(&BackgroundFetchContext::DidCreateRegistration,
                       weak_factory_.GetWeakPtr(), registration_id));
    return;
  }

  // No permission, the fetch should be rejected.
  std::move(fetch_callbacks_[registration_id])
      .Run(blink::mojom::BackgroundFetchError::PERMISSION_DENIED,
           base::nullopt);
  fetch_callbacks_.erase(registration_id);
}

void BackgroundFetchContext::GetIconDisplaySize(
    blink::mojom::BackgroundFetchService::GetIconDisplaySizeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  delegate_proxy_.GetIconDisplaySize(std::move(callback));
}

void BackgroundFetchContext::DidCreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchError error,
    const BackgroundFetchRegistration& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = fetch_callbacks_.find(registration_id);

  // The fetch might have been abandoned already if the Service Worker was
  // unregistered or corrupted while registration was in progress.
  if (iter == fetch_callbacks_.end())
    return;

  if (error == blink::mojom::BackgroundFetchError::NONE)
    std::move(iter->second).Run(error, registration);
  else
    std::move(iter->second).Run(error, base::nullopt /* registration */);

  fetch_callbacks_.erase(registration_id);
}

void BackgroundFetchContext::AddRegistrationObserver(
    const std::string& unique_id,
    blink::mojom::BackgroundFetchRegistrationObserverPtr observer) {
  registration_notifier_->AddObserver(unique_id, std::move(observer));
}

void BackgroundFetchContext::UpdateUI(
    const BackgroundFetchRegistrationId& registration_id,
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon,
    blink::mojom::BackgroundFetchService::UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  delegate_proxy_.UpdateUI(registration_id.unique_id(), title, icon,
                           std::move(callback));
}

void BackgroundFetchContext::Abort(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchService::AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scheduler_->Abort(registration_id, FailureReason::CANCELLED_BY_DEVELOPER,
                    std::move(callback));
}

void BackgroundFetchContext::MatchRequests(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
    blink::mojom::BackgroundFetchService::MatchRequestsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->MatchRequests(
      registration_id, std::move(match_params),
      base::BindOnce(&BackgroundFetchContext::DidGetMatchingRequests,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundFetchContext::DidGetMatchingRequests(
    blink::mojom::BackgroundFetchService::MatchRequestsCallback callback,
    blink::mojom::BackgroundFetchError error,
    std::vector<BackgroundFetchSettledFetch> settled_fetches) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != blink::mojom::BackgroundFetchError::NONE)
    DCHECK(settled_fetches.empty());

  std::move(callback).Run(std::move(settled_fetches));
}

void BackgroundFetchContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BackgroundFetchContext::ShutdownOnIO, this));
}

void BackgroundFetchContext::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->ShutdownOnIO();
}

void BackgroundFetchContext::SetDataManagerForTesting(
    std::unique_ptr<BackgroundFetchDataManager> data_manager) {
  DCHECK(data_manager);
  data_manager_ = std::move(data_manager);
  scheduler_ = std::make_unique<BackgroundFetchScheduler>(
      data_manager_.get(), registration_notifier_.get(), &delegate_proxy_,
      service_worker_context_);
}

}  // namespace content
