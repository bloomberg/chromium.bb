// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_handle.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"

namespace content {

ServiceWorkerMainResourceHandle::ServiceWorkerMainResourceHandle(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    ServiceWorkerAccessedCallback on_service_worker_accessed)
    : service_worker_accessed_callback_(std::move(on_service_worker_accessed)),
      context_wrapper_(std::move(context_wrapper)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerMainResourceHandle::~ServiceWorkerMainResourceHandle() = default;

void ServiceWorkerMainResourceHandle::OnCreatedContainerHost(
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(container_info->host_remote.is_valid() &&
         container_info->client_receiver.is_valid());

  container_info_ = std::move(container_info);
}

void ServiceWorkerMainResourceHandle::OnBeginNavigationCommit(
    const GlobalRenderFrameHostId& rfh_id,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr* out_container_info,
    ukm::SourceId document_ukm_source_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We may have failed to pre-create the container host.
  if (!container_info_)
    return;
  *out_container_info = std::move(container_info_);

  if (container_host_) {
    container_host_->OnBeginNavigationCommit(
        rfh_id, cross_origin_embedder_policy, std::move(coep_reporter),
        document_ukm_source_id);
  }
}

void ServiceWorkerMainResourceHandle::OnEndNavigationCommit() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (container_host_)
    container_host_->OnEndNavigationCommit();
}

void ServiceWorkerMainResourceHandle::OnBeginWorkerCommit(
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    ukm::SourceId worker_ukm_source_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (container_host_) {
    container_host_->CompleteWebWorkerPreparation(cross_origin_embedder_policy,
                                                  worker_ukm_source_id);
  }
}

}  // namespace content
