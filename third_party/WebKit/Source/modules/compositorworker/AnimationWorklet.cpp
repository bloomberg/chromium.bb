// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorklet.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/dom/AnimationWorkletProxyClient.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/page/ChromeClient.h"
#include "modules/compositorworker/AnimationWorkletMessagingProxy.h"
#include "modules/compositorworker/AnimationWorkletThread.h"

namespace blink {

// static
AnimationWorklet* AnimationWorklet::Create(LocalFrame* frame) {
  return new AnimationWorklet(frame);
}

AnimationWorklet::AnimationWorklet(LocalFrame* frame)
    : Worklet(frame), worklet_messaging_proxy_(nullptr) {}

AnimationWorklet::~AnimationWorklet() {
  if (worklet_messaging_proxy_)
    worklet_messaging_proxy_->ParentObjectDestroyed();
}

void AnimationWorklet::Initialize() {
  AnimationWorkletThread::EnsureSharedBackingThread();

  DCHECK(!worklet_messaging_proxy_);
  DCHECK(GetExecutionContext());
  Document* document = ToDocument(GetExecutionContext());
  AnimationWorkletProxyClient* proxy_client =
      document->GetFrame()->GetChromeClient().CreateAnimationWorkletProxyClient(
          document->GetFrame());

  worklet_messaging_proxy_ =
      new AnimationWorkletMessagingProxy(GetExecutionContext(), proxy_client);
  worklet_messaging_proxy_->Initialize();
}

bool AnimationWorklet::IsInitialized() const {
  return worklet_messaging_proxy_;
}

WorkletGlobalScopeProxy* AnimationWorklet::GetWorkletGlobalScopeProxy() const {
  DCHECK(worklet_messaging_proxy_);
  return worklet_messaging_proxy_;
}

DEFINE_TRACE(AnimationWorklet) {
  Worklet::Trace(visitor);
}

}  // namespace blink
