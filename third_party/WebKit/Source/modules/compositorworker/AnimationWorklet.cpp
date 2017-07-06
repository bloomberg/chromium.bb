// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorklet.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/AnimationWorkletProxyClient.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/page/ChromeClient.h"
#include "core/workers/WorkerClients.h"
#include "modules/compositorworker/AnimationWorkletMessagingProxy.h"
#include "modules/compositorworker/AnimationWorkletProxyClientImpl.h"
#include "modules/compositorworker/AnimationWorkletThread.h"

namespace blink {

// static
AnimationWorklet* AnimationWorklet::Create(LocalFrame* frame) {
  return new AnimationWorklet(frame);
}

AnimationWorklet::AnimationWorklet(LocalFrame* frame) : Worklet(frame) {}

AnimationWorklet::~AnimationWorklet() {}

bool AnimationWorklet::NeedsToCreateGlobalScope() {
  // For now, create only one global scope per document.
  // TODO(nhiroki): Revisit this later.
  return GetNumberOfGlobalScopes() == 0;
}

WorkletGlobalScopeProxy* AnimationWorklet::CreateGlobalScope() {
  DCHECK(NeedsToCreateGlobalScope());
  AnimationWorkletThread::EnsureSharedBackingThread();

  Document* document = ToDocument(GetExecutionContext());
  AnimationWorkletProxyClient* proxy_client =
      AnimationWorkletProxyClientImpl::FromDocument(document);

  WorkerClients* worker_clients = WorkerClients::Create();
  ProvideAnimationWorkletProxyClientTo(worker_clients, proxy_client);

  AnimationWorkletMessagingProxy* proxy =
      new AnimationWorkletMessagingProxy(GetExecutionContext(), worker_clients);
  proxy->Initialize();
  return proxy;
}

DEFINE_TRACE(AnimationWorklet) {
  Worklet::Trace(visitor);
}

}  // namespace blink
