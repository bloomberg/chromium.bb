// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/DedicatedWorkerMessagingProxy.h"

#include "core/workers/DedicatedWorkerThread.h"
#include "core/workers/WorkerClients.h"
#include <memory>

namespace blink {

DedicatedWorkerMessagingProxy::DedicatedWorkerMessagingProxy(
    InProcessWorkerBase* worker_object,
    WorkerClients* worker_clients)
    : InProcessWorkerMessagingProxy(worker_object, worker_clients) {}

DedicatedWorkerMessagingProxy::~DedicatedWorkerMessagingProxy() {}

std::unique_ptr<WorkerThread> DedicatedWorkerMessagingProxy::CreateWorkerThread(
    double origin_time) {
  return DedicatedWorkerThread::Create(LoaderProxy(), WorkerObjectProxy(),
                                       origin_time);
}

bool DedicatedWorkerMessagingProxy::IsAtomicsWaitAllowed() {
  return true;
}

}  // namespace blink
