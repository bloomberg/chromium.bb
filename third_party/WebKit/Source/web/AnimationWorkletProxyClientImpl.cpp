// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/AnimationWorkletProxyClientImpl.h"

#include "core/dom/CompositorProxy.h"
#include "platform/graphics/CompositorMutableStateProvider.h"
#include "web/CompositorMutatorImpl.h"

namespace blink {

AnimationWorkletProxyClientImpl::AnimationWorkletProxyClientImpl(
    CompositorMutatorImpl* mutator)
    : m_mutator(mutator) {
  DCHECK(isMainThread());
}

DEFINE_TRACE(AnimationWorkletProxyClientImpl) {
  visitor->trace(m_proxies);
  AnimationWorkletProxyClient::trace(visitor);
  CompositorAnimator::trace(visitor);
}

bool AnimationWorkletProxyClientImpl::mutate(
    double monotonicTimeNow,
    CompositorMutableStateProvider* provider) {
  DCHECK(!isMainThread());
  // TODO(majidvp): actually call JS |animate| callbacks.

  // Always request another rAF for now.
  return true;
}

void AnimationWorkletProxyClientImpl::registerCompositorProxy(
    CompositorProxy* proxy) {
  DCHECK(!isMainThread());
  m_proxies.insert(proxy);
}

void AnimationWorkletProxyClientImpl::unregisterCompositorProxy(
    CompositorProxy* proxy) {
  DCHECK(!isMainThread());
  m_proxies.erase(proxy);
}

}  // namespace blink
