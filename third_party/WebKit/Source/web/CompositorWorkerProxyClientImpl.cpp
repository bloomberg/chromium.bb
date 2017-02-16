// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/CompositorWorkerProxyClientImpl.h"

#include "core/dom/CompositorProxy.h"
#include "modules/compositorworker/CompositorWorkerGlobalScope.h"
#include "platform/graphics/CompositorMutableStateProvider.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "web/CompositorMutatorImpl.h"
#include "wtf/CurrentTime.h"

namespace blink {

// A helper class that updates proxies mutable state on creation and reset it
// on destruction. This can be used with rAF callback to ensure no mutation is
// allowed outside rAF.
class ScopedCompositorMutableState final {
  WTF_MAKE_NONCOPYABLE(ScopedCompositorMutableState);
  STACK_ALLOCATED();

 public:
  ScopedCompositorMutableState(
      HeapHashSet<WeakMember<CompositorProxy>>& proxies,
      CompositorMutableStateProvider* stateProvider)
      : m_proxies(proxies) {
    for (CompositorProxy* proxy : m_proxies) {
      proxy->takeCompositorMutableState(
          stateProvider->getMutableStateFor(proxy->elementId()));
    }
  }
  ~ScopedCompositorMutableState() {
    for (CompositorProxy* proxy : m_proxies)
      proxy->takeCompositorMutableState(nullptr);
  }

 private:
  HeapHashSet<WeakMember<CompositorProxy>>& m_proxies;
};

CompositorWorkerProxyClientImpl::CompositorWorkerProxyClientImpl(
    CompositorMutatorImpl* mutator)
    : m_mutator(mutator), m_globalScope(nullptr) {
  DCHECK(isMainThread());
}

DEFINE_TRACE(CompositorWorkerProxyClientImpl) {
  visitor->trace(m_proxies);
  CompositorAnimator::trace(visitor);
  CompositorWorkerProxyClient::trace(visitor);
}

void CompositorWorkerProxyClientImpl::setGlobalScope(WorkerGlobalScope* scope) {
  TRACE_EVENT0("compositor-worker",
               "CompositorWorkerProxyClientImpl::setGlobalScope");
  DCHECK(!m_globalScope);
  DCHECK(scope);
  m_globalScope = static_cast<CompositorWorkerGlobalScope*>(scope);
  m_mutator->registerCompositorAnimator(this);
}

void CompositorWorkerProxyClientImpl::dispose() {
  // CompositorWorkerProxyClientImpl and CompositorMutatorImpl form a reference
  // cycle. CompositorWorkerGlobalScope and CompositorWorkerProxyClientImpl
  // also form another big reference cycle. So dispose needs to be called on
  // Worker termination to break these cycles. If not, layout test leak
  // detection will report a WorkerGlobalScope leak.
  m_mutator->unregisterCompositorAnimator(this);
  m_globalScope = nullptr;
}

void CompositorWorkerProxyClientImpl::requestAnimationFrame() {
  TRACE_EVENT0("compositor-worker",
               "CompositorWorkerProxyClientImpl::requestAnimationFrame");
  m_requestedAnimationFrameCallbacks = true;
  m_mutator->setNeedsMutate();
}

bool CompositorWorkerProxyClientImpl::mutate(
    double monotonicTimeNow,
    CompositorMutableStateProvider* stateProvider) {
  if (!m_globalScope)
    return false;

  TRACE_EVENT0("compositor-worker", "CompositorWorkerProxyClientImpl::mutate");
  if (!m_requestedAnimationFrameCallbacks)
    return false;

  {
    ScopedCompositorMutableState mutableState(m_proxies, stateProvider);
    m_requestedAnimationFrameCallbacks =
        executeAnimationFrameCallbacks(monotonicTimeNow);
  }

  return m_requestedAnimationFrameCallbacks;
}

bool CompositorWorkerProxyClientImpl::executeAnimationFrameCallbacks(
    double monotonicTimeNow) {
  TRACE_EVENT0(
      "compositor-worker",
      "CompositorWorkerProxyClientImpl::executeAnimationFrameCallbacks");

  DCHECK(m_globalScope);
  // Convert to zero based document time in milliseconds consistent with
  // requestAnimationFrame.
  double highResTimeMs =
      1000.0 * (monotonicTimeNow - m_globalScope->timeOrigin());
  return m_globalScope->executeAnimationFrameCallbacks(highResTimeMs);
}

void CompositorWorkerProxyClientImpl::registerCompositorProxy(
    CompositorProxy* proxy) {
  m_proxies.insert(proxy);
}

void CompositorWorkerProxyClientImpl::unregisterCompositorProxy(
    CompositorProxy* proxy) {
  m_proxies.erase(proxy);
}

}  // namespace blink
