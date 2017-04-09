// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletMessagingProxy.h"

#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "modules/webaudio/AudioWorkletThread.h"

namespace blink {

AudioWorkletMessagingProxy::AudioWorkletMessagingProxy(
    ExecutionContext* execution_context)
    : ThreadedWorkletMessagingProxy(execution_context) {}

AudioWorkletMessagingProxy::~AudioWorkletMessagingProxy() {}

std::unique_ptr<WorkerThread> AudioWorkletMessagingProxy::CreateWorkerThread(
    double origin_time) {
  return AudioWorkletThread::Create(LoaderProxy(), WorkletObjectProxy());
}

}  // namespace blink
