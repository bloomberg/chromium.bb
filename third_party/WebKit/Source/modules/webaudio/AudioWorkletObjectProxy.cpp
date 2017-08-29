// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletObjectProxy.h"

#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "core/workers/WorkerThread.h"
#include "modules/webaudio/AudioWorkletGlobalScope.h"
#include "modules/webaudio/AudioWorkletMessagingProxy.h"
#include "platform/CrossThreadFunctional.h"

namespace blink {

AudioWorkletObjectProxy::AudioWorkletObjectProxy(
    AudioWorkletMessagingProxy* messaging_proxy_weak_ptr,
    ParentFrameTaskRunners* parent_frame_task_runners)
    : ThreadedWorkletObjectProxy(
          static_cast<ThreadedWorkletMessagingProxy*>(messaging_proxy_weak_ptr),
          parent_frame_task_runners) {}

void AudioWorkletObjectProxy::DidCreateWorkerGlobalScope(
    WorkerOrWorkletGlobalScope* global_scope) {
  global_scope_ = ToAudioWorkletGlobalScope(global_scope);
}

void AudioWorkletObjectProxy::DidEvaluateModuleScript(bool success) {
  DCHECK(global_scope_);
  // TODO(crbug.com/755566): Extract/build the information for synchronization
  // and send it to the associated AudioWorkletMessagingProxy. Currently this
  // is an empty cross-thread call for the future implementation.
  GetParentFrameTaskRunners()->Get(TaskType::kUnthrottled)
       ->PostTask(
           BLINK_FROM_HERE,
           CrossThreadBind(
                &AudioWorkletMessagingProxy::SynchronizeWorkletData,
                GetAudioWorkletMessagingProxyWeakPtr()));
}

void AudioWorkletObjectProxy::WillDestroyWorkerGlobalScope() {
  global_scope_ = nullptr;
}

CrossThreadWeakPersistent<AudioWorkletMessagingProxy>
AudioWorkletObjectProxy::GetAudioWorkletMessagingProxyWeakPtr() {
  return static_cast<AudioWorkletMessagingProxy*>(
      MessagingProxyWeakPtr().Get());
}

}  // namespace blink
