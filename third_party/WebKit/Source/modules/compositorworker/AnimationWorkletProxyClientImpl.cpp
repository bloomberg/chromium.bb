// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletProxyClientImpl.h"

#include "core/animation/CompositorMutatorImpl.h"

namespace blink {

AnimationWorkletProxyClientImpl::AnimationWorkletProxyClientImpl(
    CompositorMutatorImpl* mutator)
    : mutator_(mutator) {
  DCHECK(IsMainThread());
}

DEFINE_TRACE(AnimationWorkletProxyClientImpl) {
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

bool AnimationWorkletProxyClientImpl::Mutate(double monotonic_time_now) {
  DCHECK(global_scope_->IsContextThread());

  if (global_scope_)
    global_scope_->Mutate();

  // Always request another rAF for now.
  return true;
}

}  // namespace blink
