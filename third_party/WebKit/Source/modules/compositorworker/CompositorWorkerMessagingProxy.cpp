// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerMessagingProxy.h"

#include "core/workers/WorkerThreadStartupData.h"
#include "modules/compositorworker/CompositorWorkerThread.h"
#include <memory>

namespace blink {

CompositorWorkerMessagingProxy::CompositorWorkerMessagingProxy(
    InProcessWorkerBase* worker,
    WorkerClients* worker_clients)
    : InProcessWorkerMessagingProxy(worker, worker_clients) {
  DCHECK(IsMainThread());
}

CompositorWorkerMessagingProxy::~CompositorWorkerMessagingProxy() {}

std::unique_ptr<WorkerThread>
CompositorWorkerMessagingProxy::CreateWorkerThread(double origin_time) {
  return CompositorWorkerThread::Create(CreateThreadableLoadingContext(),
                                        WorkerObjectProxy(), origin_time);
}

}  // namespace blink
