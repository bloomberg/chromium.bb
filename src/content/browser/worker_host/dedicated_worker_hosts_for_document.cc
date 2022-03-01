// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_hosts_for_document.h"

#include "content/browser/worker_host/dedicated_worker_host.h"

namespace content {

DedicatedWorkerHostsForDocument::DedicatedWorkerHostsForDocument(
    RenderFrameHost* rfh)
    : DocumentUserData<DedicatedWorkerHostsForDocument>(rfh),
      dedicated_workers_([](const base::SafeRef<DedicatedWorkerHost>& lhs,
                            const base::SafeRef<DedicatedWorkerHost>& rhs) {
        return &*lhs < &*rhs;
      }) {}

DedicatedWorkerHostsForDocument::~DedicatedWorkerHostsForDocument() = default;

void DedicatedWorkerHostsForDocument::Add(
    base::SafeRef<DedicatedWorkerHost> dedicated_worker_host) {
  dedicated_workers_.insert(dedicated_worker_host);
}

void DedicatedWorkerHostsForDocument::Remove(
    base::SafeRef<DedicatedWorkerHost> dedicated_worker_host) {
  dedicated_workers_.erase(dedicated_worker_host);
}

blink::scheduler::WebSchedulerTrackedFeatures
DedicatedWorkerHostsForDocument::GetBackForwardCacheDisablingFeatures() const {
  blink::scheduler::WebSchedulerTrackedFeatures features;
  for (auto worker : dedicated_workers_) {
    features.PutAll(worker->GetBackForwardCacheDisablingFeatures());
  }
  return features;
}

DOCUMENT_USER_DATA_KEY_IMPL(DedicatedWorkerHostsForDocument);

}  // namespace content
