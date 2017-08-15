// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletMessagingProxy.h"

#include "modules/webaudio/AudioWorkletObjectProxy.h"
#include "modules/webaudio/AudioWorkletThread.h"

namespace blink {

AudioWorkletMessagingProxy::AudioWorkletMessagingProxy(
    ExecutionContext* execution_context,
    WorkerClients* worker_clients)
    : ThreadedWorkletMessagingProxy(execution_context, worker_clients) {}

AudioWorkletMessagingProxy::~AudioWorkletMessagingProxy() {}

void AudioWorkletMessagingProxy::SynchronizeWorkletData() {
  DCHECK(IsMainThread());

  // TODO(crbug.com/755566): the argument will be a set of a node name and
  // parameter descriptors. Use the information to update the copy in
  // AudioWorkletMessagingProxy.
}

std::unique_ptr<ThreadedWorkletObjectProxy>
    AudioWorkletMessagingProxy::CreateObjectProxy(
        ThreadedWorkletMessagingProxy* messaging_proxy,
        ParentFrameTaskRunners* parent_frame_task_runners) {
  return WTF::MakeUnique<AudioWorkletObjectProxy>(
      static_cast<AudioWorkletMessagingProxy*>(messaging_proxy),
      parent_frame_task_runners);
}

std::unique_ptr<WorkerThread> AudioWorkletMessagingProxy::CreateWorkerThread() {
  return AudioWorkletThread::Create(CreateThreadableLoadingContext(),
                                    WorkletObjectProxy());
}

}  // namespace blink
