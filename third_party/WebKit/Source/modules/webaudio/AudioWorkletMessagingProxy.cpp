// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletMessagingProxy.h"

#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "modules/webaudio/AudioWorkletThread.h"

namespace blink {

AudioWorkletMessagingProxy::AudioWorkletMessagingProxy(
    ExecutionContext* executionContext)
    : ThreadedWorkletMessagingProxy(executionContext) {}

AudioWorkletMessagingProxy::~AudioWorkletMessagingProxy() {}

std::unique_ptr<WorkerThread> AudioWorkletMessagingProxy::createWorkerThread(
    double originTime) {
  return AudioWorkletThread::create(loaderProxy(), workletObjectProxy());
}

}  // namespace blink
