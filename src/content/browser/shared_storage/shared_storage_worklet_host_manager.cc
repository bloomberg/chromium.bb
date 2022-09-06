// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_render_thread_worklet_driver.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"

namespace content {

SharedStorageWorkletHostManager::SharedStorageWorkletHostManager() = default;
SharedStorageWorkletHostManager::~SharedStorageWorkletHostManager() = default;

void SharedStorageWorkletHostManager::OnDocumentServiceDestroyed(
    SharedStorageDocumentServiceImpl* document_service) {
  // Note: `attached_shared_storage_worklet_hosts_` will be populated when
  // there's an actual worklet operation request, but the
  // `SharedStorageDocumentServiceImpl` will call this method on destruction
  // irrespectively, so it may not exist in map.
  auto it = attached_shared_storage_worklet_hosts_.find(document_service);
  if (it == attached_shared_storage_worklet_hosts_.end())
    return;

  SharedStorageWorkletHost* worklet_host = it->second.get();
  if (!worklet_host->HasPendingOperations()) {
    attached_shared_storage_worklet_hosts_.erase(it);
    return;
  }

  DCHECK(!keep_alive_shared_storage_worklet_hosts_.count(worklet_host));
  keep_alive_shared_storage_worklet_hosts_.insert(
      {worklet_host, std::move(it->second)});

  attached_shared_storage_worklet_hosts_.erase(it);

  worklet_host->EnterKeepAliveOnDocumentDestroyed(base::BindOnce(
      &SharedStorageWorkletHostManager::OnWorkletKeepAliveFinished,
      base::Unretained(this)));
}

SharedStorageWorkletHost*
SharedStorageWorkletHostManager::GetOrCreateSharedStorageWorkletHost(
    SharedStorageDocumentServiceImpl* document_service) {
  auto it = attached_shared_storage_worklet_hosts_.find(document_service);
  if (it != attached_shared_storage_worklet_hosts_.end())
    return it->second.get();

  auto driver = std::make_unique<SharedStorageRenderThreadWorkletDriver>(
      &(static_cast<RenderFrameHostImpl&>(document_service->render_frame_host())
            .GetAgentSchedulingGroup()));

  std::unique_ptr<SharedStorageWorkletHost> worklet_host =
      CreateSharedStorageWorkletHost(std::move(driver), *document_service);

  SharedStorageWorkletHost* worklet_host_ptr = worklet_host.get();

  attached_shared_storage_worklet_hosts_.insert(
      {document_service, std::move(worklet_host)});
  return worklet_host_ptr;
}

std::unique_ptr<SharedStorageWorkletHost>
SharedStorageWorkletHostManager::CreateSharedStorageWorkletHost(
    std::unique_ptr<SharedStorageWorkletDriver> driver,
    SharedStorageDocumentServiceImpl& document_service) {
  return std::make_unique<SharedStorageWorkletHost>(std::move(driver),
                                                    document_service);
}

void SharedStorageWorkletHostManager::OnWorkletKeepAliveFinished(
    SharedStorageWorkletHost* worklet_host) {
  DCHECK(keep_alive_shared_storage_worklet_hosts_.count(worklet_host));
  keep_alive_shared_storage_worklet_hosts_.erase(worklet_host);
}

}  // namespace content
