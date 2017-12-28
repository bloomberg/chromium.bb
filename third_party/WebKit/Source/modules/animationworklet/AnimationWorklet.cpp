// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/animationworklet/AnimationWorklet.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/AnimationWorkletProxyClient.h"
#include "core/dom/Document.h"
#include "core/page/ChromeClient.h"
#include "core/workers/WorkerClients.h"
#include "modules/animationworklet/AnimationWorkletMessagingProxy.h"
#include "modules/animationworklet/AnimationWorkletProxyClientImpl.h"
#include "modules/animationworklet/AnimationWorkletThread.h"

namespace blink {

AnimationWorklet::AnimationWorklet(Document* document) : Worklet(document) {}

AnimationWorklet::~AnimationWorklet() = default;

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
      new AnimationWorkletMessagingProxy(GetExecutionContext());
  proxy->Initialize(worker_clients);
  return proxy;
}

void AnimationWorklet::Trace(blink::Visitor* visitor) {
  Worklet::Trace(visitor);
}

}  // namespace blink
