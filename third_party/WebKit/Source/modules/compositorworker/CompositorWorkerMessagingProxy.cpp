// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerMessagingProxy.h"

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

WTF::Optional<WorkerBackingThreadStartupData>
CompositorWorkerMessagingProxy::CreateBackingThreadStartupData(v8::Isolate*) {
  return WTF::nullopt;
}

std::unique_ptr<WorkerThread>
CompositorWorkerMessagingProxy::CreateWorkerThread() {
  return CompositorWorkerThread::Create(CreateThreadableLoadingContext(),
                                        WorkerObjectProxy());
}

}  // namespace blink
