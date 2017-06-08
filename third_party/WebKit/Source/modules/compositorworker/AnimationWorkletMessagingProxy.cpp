// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletMessagingProxy.h"

#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "core/workers/WorkerClients.h"
#include "modules/compositorworker/AnimationWorkletThread.h"

namespace blink {

AnimationWorkletMessagingProxy::AnimationWorkletMessagingProxy(
    ExecutionContext* execution_context,
    WorkerClients* worker_clients)
    : ThreadedWorkletMessagingProxy(execution_context, worker_clients) {}

AnimationWorkletMessagingProxy::~AnimationWorkletMessagingProxy() {}

std::unique_ptr<WorkerThread>
AnimationWorkletMessagingProxy::CreateWorkerThread(double origin_time) {
  return AnimationWorkletThread::Create(CreateThreadableLoadingContext(),
                                        WorkletObjectProxy());
}

}  // namespace blink
