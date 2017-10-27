// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/animationworklet/AnimationWorkletProxyClientImpl.h"

#include "core/animation/CompositorMutatorImpl.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameImpl.h"

namespace blink {

AnimationWorkletProxyClientImpl::AnimationWorkletProxyClientImpl(
    CompositorMutatorImpl* mutator)
    : mutator_(mutator) {
  DCHECK(IsMainThread());
}

void AnimationWorkletProxyClientImpl::Trace(blink::Visitor* visitor) {
  AnimationWorkletProxyClient::Trace(visitor);
  CompositorAnimator::Trace(visitor);
}

void AnimationWorkletProxyClientImpl::SetGlobalScope(
    WorkletGlobalScope* global_scope) {
  DCHECK(global_scope->IsContextThread());
  DCHECK(global_scope);
  global_scope_ = static_cast<AnimationWorkletGlobalScope*>(global_scope);
  mutator_->RegisterCompositorAnimator(this);
}

void AnimationWorkletProxyClientImpl::Dispose() {
  DCHECK(global_scope_->IsContextThread());
  // At worklet scope termination break the reference cycle between
  // CompositorMutatorImpl and AnimationProxyClientImpl and also the cycle
  // between AnimationWorkletGlobalScope and AnimationWorkletProxyClientImpl.
  mutator_->UnregisterCompositorAnimator(this);
  global_scope_ = nullptr;
}

void AnimationWorkletProxyClientImpl::Mutate(
    const CompositorMutatorInputState& state) {
  DCHECK(global_scope_->IsContextThread());

  std::unique_ptr<CompositorMutatorOutputState> output = nullptr;

  if (global_scope_)
    output = global_scope_->Mutate(state);

  mutator_->SetMutationUpdate(std::move(output));
}

// static
AnimationWorkletProxyClientImpl* AnimationWorkletProxyClientImpl::FromDocument(
    Document* document) {
  WebLocalFrameImpl* local_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());
  return new AnimationWorkletProxyClientImpl(
      local_frame->LocalRootFrameWidget()->CompositorMutator());
}

}  // namespace blink
