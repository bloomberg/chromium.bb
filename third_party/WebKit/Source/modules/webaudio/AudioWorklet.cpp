// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorklet.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/UseCounter.h"
#include "core/workers/WorkerClients.h"
#include "modules/webaudio/AudioWorkletMessagingProxy.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "modules/webaudio/CrossThreadAudioWorkletProcessorInfo.h"

namespace blink {

AudioWorklet* AudioWorklet::Create(BaseAudioContext* context) {
  return new AudioWorklet(context);
}

AudioWorklet::AudioWorklet(BaseAudioContext* context)
    : Worklet(ToDocument(context->GetExecutionContext())), context_(context) {}

void AudioWorklet::CreateProcessor(AudioWorkletHandler* handler,
                                   MessagePortChannel message_port_channel) {
  DCHECK(IsMainThread());
  DCHECK(GetMessagingProxy());
  GetMessagingProxy()->CreateProcessor(handler,
                                       std::move(message_port_channel));
}

void AudioWorklet::NotifyGlobalScopeIsUpdated() {
  DCHECK(IsMainThread());

  if (!worklet_started_) {
    context_->NotifyWorkletIsReady();
    worklet_started_ = true;
  }
}

WebThread* AudioWorklet::GetBackingThread() {
  DCHECK(IsMainThread());
  DCHECK(GetMessagingProxy());
  return GetMessagingProxy()->GetWorkletBackingThread();
}

const Vector<CrossThreadAudioParamInfo>
    AudioWorklet::GetParamInfoListForProcessor(
    const String& name) {
  DCHECK(IsMainThread());
  DCHECK(GetMessagingProxy());
  return GetMessagingProxy()->GetParamInfoListForProcessor(name);
}

bool AudioWorklet::IsProcessorRegistered(const String& name) {
  DCHECK(IsMainThread());
  DCHECK(GetMessagingProxy());
  return GetMessagingProxy()->IsProcessorRegistered(name);
}

bool AudioWorklet::IsReady() {
  DCHECK(IsMainThread());
  return GetMessagingProxy() && GetBackingThread();
}

bool AudioWorklet::NeedsToCreateGlobalScope() {
  // This is a callback from |Worklet::FetchAndInvokeScript| call, which only
  // can be triggered by Worklet.addModule() call.
  UseCounter::Count(GetExecutionContext(), WebFeature::kAudioWorkletAddModule);

  return GetNumberOfGlobalScopes() == 0;
}

WorkletGlobalScopeProxy* AudioWorklet::CreateGlobalScope() {
  DCHECK_EQ(GetNumberOfGlobalScopes(), 0u);

  AudioWorkletMessagingProxy* proxy =
      new AudioWorkletMessagingProxy(GetExecutionContext(), this);
  proxy->Initialize(WorkerClients::Create());
  return proxy;
}

AudioWorkletMessagingProxy* AudioWorklet::GetMessagingProxy() {
  return GetNumberOfGlobalScopes() == 0
             ? nullptr
             : static_cast<AudioWorkletMessagingProxy*>(
                   FindAvailableGlobalScope());
}

void AudioWorklet::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_);
  Worklet::Trace(visitor);
}

}  // namespace blink
