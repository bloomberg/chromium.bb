// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/animationworklet/AnimationWorkletMessagingProxy.h"

#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "modules/animationworklet/AnimationWorkletThread.h"

namespace blink {

AnimationWorkletMessagingProxy::AnimationWorkletMessagingProxy(
    ExecutionContext* execution_context)
    : ThreadedWorkletMessagingProxy(execution_context) {}

void AnimationWorkletMessagingProxy::Trace(blink::Visitor* visitor) {
  ThreadedWorkletMessagingProxy::Trace(visitor);
}

AnimationWorkletMessagingProxy::~AnimationWorkletMessagingProxy() = default;

std::unique_ptr<WorkerThread>
AnimationWorkletMessagingProxy::CreateWorkerThread() {
  return AnimationWorkletThread::Create(CreateThreadableLoadingContext(),
                                        WorkletObjectProxy());
}

}  // namespace blink
