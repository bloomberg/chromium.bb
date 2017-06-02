// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletProxyClientImpl.h"

#include "core/animation/CompositorMutatorImpl.h"
#include "core/dom/CompositorProxy.h"
#include "platform/graphics/CompositorMutableStateProvider.h"

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

bool AnimationWorkletProxyClientImpl::Mutate(
    double monotonic_time_now,
    CompositorMutableStateProvider* provider) {
  DCHECK(!IsMainThread());
  // TODO(majidvp): actually call JS |animate| callbacks.

  // Always request another rAF for now.
  return true;
}

}  // namespace blink
