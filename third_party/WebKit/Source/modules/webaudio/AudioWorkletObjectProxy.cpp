// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletObjectProxy.h"

#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "core/workers/WorkerThread.h"
#include "modules/webaudio/CrossThreadAudioWorkletProcessorInfo.h"
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

  if (!success || global_scope_->NumberOfRegisteredDefinitions() == 0)
    return;

  std::unique_ptr<Vector<CrossThreadAudioWorkletProcessorInfo>>
      processor_info_list =
          global_scope_->WorkletProcessorInfoListForSynchronization();

  if (processor_info_list->size() == 0)
    return;

  GetParentFrameTaskRunners()->Get(TaskType::kUnthrottled)->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(
          &AudioWorkletMessagingProxy::SynchronizeWorkletProcessorInfoList,
          GetAudioWorkletMessagingProxyWeakPtr(),
          WTF::Passed(std::move(processor_info_list))));
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
