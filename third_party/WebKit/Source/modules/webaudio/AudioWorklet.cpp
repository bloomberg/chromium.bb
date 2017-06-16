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

AudioWorklet::AudioWorklet(LocalFrame* frame) : Worklet(frame) {}

AudioWorklet::~AudioWorklet() {}

bool AudioWorklet::NeedsToCreateGlobalScope() {
  // For now, create only one global scope per document.
  // TODO(nhiroki): Revisit this later.
  return GetNumberOfGlobalScopes() == 0;
}

WorkletGlobalScopeProxy* AudioWorklet::CreateGlobalScope() {
  DCHECK(NeedsToCreateGlobalScope());
  AudioWorkletThread::EnsureSharedBackingThread();

  WorkerClients* worker_clients = WorkerClients::Create();
  AudioWorkletMessagingProxy* proxy =
      new AudioWorkletMessagingProxy(GetExecutionContext(), worker_clients);
  proxy->Initialize();
  return proxy;
}

DEFINE_TRACE(AudioWorklet) {
  Worklet::Trace(visitor);
}

}  // namespace blink
