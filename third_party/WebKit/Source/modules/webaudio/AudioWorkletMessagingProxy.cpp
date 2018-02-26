// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletMessagingProxy.h"

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/messaging/MessagePort.h"
#include "modules/webaudio/AudioWorklet.h"
#include "modules/webaudio/AudioWorkletGlobalScope.h"
#include "modules/webaudio/AudioWorkletNode.h"
#include "modules/webaudio/AudioWorkletObjectProxy.h"
#include "modules/webaudio/AudioWorkletProcessor.h"
#include "modules/webaudio/AudioWorkletThread.h"
#include "modules/webaudio/CrossThreadAudioWorkletProcessorInfo.h"
#include "public/platform/TaskType.h"

namespace blink {

AudioWorkletMessagingProxy::AudioWorkletMessagingProxy(
    ExecutionContext* execution_context,
    AudioWorklet* worklet)
    : ThreadedWorkletMessagingProxy(execution_context), worklet_(worklet) {}

void AudioWorkletMessagingProxy::CreateProcessor(
    AudioWorkletHandler* handler,
    MessagePortChannel message_port_channel,
    scoped_refptr<SerializedScriptValue> node_options) {
  DCHECK(IsMainThread());
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kMiscPlatformAPI), FROM_HERE,
      CrossThreadBind(
          &AudioWorkletMessagingProxy::CreateProcessorOnRenderingThread,
          WrapCrossThreadPersistent(this),
          CrossThreadUnretained(GetWorkerThread()),
          CrossThreadUnretained(handler),
          handler->Name(),
          std::move(message_port_channel),
          std::move(node_options)));
}

void AudioWorkletMessagingProxy::CreateProcessorOnRenderingThread(
    WorkerThread* worker_thread,
    AudioWorkletHandler* handler,
    const String& name,
    MessagePortChannel message_port_channel,
    scoped_refptr<SerializedScriptValue> node_options) {
  DCHECK(worker_thread->IsCurrentThread());
  AudioWorkletGlobalScope* global_scope =
      ToAudioWorkletGlobalScope(worker_thread->GlobalScope());
  AudioWorkletProcessor* processor =
      global_scope->CreateProcessor(name, message_port_channel, node_options);
  handler->SetProcessorOnRenderThread(processor);
}

void AudioWorkletMessagingProxy::SynchronizeWorkletProcessorInfoList(
    std::unique_ptr<Vector<CrossThreadAudioWorkletProcessorInfo>> info_list) {
  DCHECK(IsMainThread());
  for (auto& processor_info : *info_list) {
    processor_info_map_.insert(processor_info.Name(),
                               processor_info.ParamInfoList());
  }

  // Notify AudioWorklet object that the global scope has been updated after the
  // script evaluation.
  worklet_->NotifyGlobalScopeIsUpdated();
}

bool AudioWorkletMessagingProxy::IsProcessorRegistered(
    const String& name) const {
  return processor_info_map_.Contains(name);
}

const Vector<CrossThreadAudioParamInfo>
AudioWorkletMessagingProxy::GetParamInfoListForProcessor(
    const String& name) const {
  DCHECK(IsProcessorRegistered(name));
  return processor_info_map_.at(name);
}

WebThread* AudioWorkletMessagingProxy::GetBackingWebThread() {
  auto worklet_thread = static_cast<AudioWorkletThread*>(GetWorkerThread());
  return worklet_thread->GetSharedBackingThread();
}

WorkerThread* AudioWorkletMessagingProxy::GetBackingWorkerThread() {
  return GetWorkerThread();
}

std::unique_ptr<ThreadedWorkletObjectProxy>
AudioWorkletMessagingProxy::CreateObjectProxy(
    ThreadedWorkletMessagingProxy* messaging_proxy,
    ParentFrameTaskRunners* parent_frame_task_runners) {
  return std::make_unique<AudioWorkletObjectProxy>(
      static_cast<AudioWorkletMessagingProxy*>(messaging_proxy),
      parent_frame_task_runners,
      worklet_->GetBaseAudioContext()->sampleRate());
}

std::unique_ptr<WorkerThread> AudioWorkletMessagingProxy::CreateWorkerThread() {
  return AudioWorkletThread::Create(CreateThreadableLoadingContext(),
                                    WorkletObjectProxy());
}

void AudioWorkletMessagingProxy::Trace(Visitor* visitor) {
  visitor->Trace(worklet_);
  ThreadedWorkletMessagingProxy::Trace(visitor);
}


}  // namespace blink
