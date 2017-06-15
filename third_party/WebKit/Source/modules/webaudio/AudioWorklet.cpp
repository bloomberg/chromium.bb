// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorklet.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerClients.h"
#include "modules/webaudio/AudioWorkletMessagingProxy.h"
#include "modules/webaudio/AudioWorkletThread.h"

namespace blink {

AudioWorklet* AudioWorklet::Create(LocalFrame* frame) {
  return new AudioWorklet(frame);
}

AudioWorklet::AudioWorklet(LocalFrame* frame)
    : ThreadedWorklet(frame), worklet_messaging_proxy_(nullptr) {}

AudioWorklet::~AudioWorklet() {
  if (worklet_messaging_proxy_)
    worklet_messaging_proxy_->ParentObjectDestroyed();
}

void AudioWorklet::Initialize() {
  AudioWorkletThread::EnsureSharedBackingThread();

  DCHECK(!worklet_messaging_proxy_);
  DCHECK(GetExecutionContext());

  WorkerClients* worker_clients = WorkerClients::Create();
  worklet_messaging_proxy_ =
      new AudioWorkletMessagingProxy(GetExecutionContext(), worker_clients);
  worklet_messaging_proxy_->Initialize();
}

bool AudioWorklet::IsInitialized() const {
  return worklet_messaging_proxy_;
}

WorkletGlobalScopeProxy* AudioWorklet::GetWorkletGlobalScopeProxy() const {
  DCHECK(worklet_messaging_proxy_);
  return worklet_messaging_proxy_;
}

DEFINE_TRACE(AudioWorklet) {
  visitor->Trace(worklet_messaging_proxy_);
  ThreadedWorklet::Trace(visitor);
}

}  // namespace blink
