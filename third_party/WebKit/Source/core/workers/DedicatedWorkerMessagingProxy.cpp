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

bool DedicatedWorkerMessagingProxy::IsAtomicsWaitAllowed() {
  return true;
}

WTF::Optional<WorkerBackingThreadStartupData>
DedicatedWorkerMessagingProxy::CreateBackingThreadStartupData(
    v8::Isolate* isolate) {
  using HeapLimitMode = WorkerBackingThreadStartupData::HeapLimitMode;
  using AtomicsWaitMode = WorkerBackingThreadStartupData::AtomicsWaitMode;
  return WorkerBackingThreadStartupData(
      isolate->IsHeapLimitIncreasedForDebugging()
          ? HeapLimitMode::kIncreasedForDebugging
          : HeapLimitMode::kDefault,
      IsAtomicsWaitAllowed() ? AtomicsWaitMode::kAllow
                             : AtomicsWaitMode::kDisallow);
}

std::unique_ptr<WorkerThread> DedicatedWorkerMessagingProxy::CreateWorkerThread(
    double origin_time) {
  return DedicatedWorkerThread::Create(CreateThreadableLoadingContext(),
                                       WorkerObjectProxy(), origin_time);
}

}  // namespace blink
