// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorklet.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerClients.h"
#include "modules/webaudio/AudioWorkletMessagingProxy.h"
#include "modules/webaudio/BaseAudioContext.h"

namespace blink {

AudioWorklet* AudioWorklet::Create(LocalFrame* frame) {
  return new AudioWorklet(frame);
}

AudioWorklet::AudioWorklet(LocalFrame* frame) : Worklet(frame) {}

AudioWorklet::~AudioWorklet() {
  contexts_.clear();
}

void AudioWorklet::RegisterContext(BaseAudioContext* context) {
  DCHECK(!contexts_.Contains(context));
  contexts_.insert(context);

  // Check if AudioWorklet loads the script and has an active
  // AudioWorkletGlobalScope before getting the messaging proxy.
  if (IsWorkletMessagingProxyCreated())
    context->SetWorkletMessagingProxy(FindAvailableMessagingProxy());
}

void AudioWorklet::UnregisterContext(BaseAudioContext* context) {
  // This may be called multiple times from BaseAudioContext.
  if (!contexts_.Contains(context))
    return;

  contexts_.erase(context);
}

AudioWorkletMessagingProxy* AudioWorklet::FindAvailableMessagingProxy() {
  return static_cast<AudioWorkletMessagingProxy*>(FindAvailableGlobalScope());
}

bool AudioWorklet::IsWorkletMessagingProxyCreated() const {
  return GetNumberOfGlobalScopes() > 0;
}

bool AudioWorklet::NeedsToCreateGlobalScope() {
  // TODO(hongchan): support multiple WorkletGlobalScopes, one scope per a
  // BaseAudioContext. In order to do it, FindAvailableGlobalScope() needs to
  // be inherited and rewritten.
  return GetNumberOfGlobalScopes() == 0;
}

WorkletGlobalScopeProxy* AudioWorklet::CreateGlobalScope() {
  DCHECK(NeedsToCreateGlobalScope());

  WorkerClients* worker_clients = WorkerClients::Create();
  AudioWorkletMessagingProxy* proxy =
      new AudioWorkletMessagingProxy(GetExecutionContext(), worker_clients);
  proxy->Initialize();

  for (BaseAudioContext* context : contexts_) {
    // TODO(hongchan): Currently all BaseAudioContexts shares a single
    // AudioWorkletMessagingProxy. Fix this to support one messaging proxy for
    // each BaseAudioContext.
    if (!context->HasWorkletMessagingProxy())
      context->SetWorkletMessagingProxy(proxy);
  }

  return proxy;
}

DEFINE_TRACE(AudioWorklet) {
  visitor->Trace(contexts_);
  Worklet::Trace(visitor);
}

}  // namespace blink
