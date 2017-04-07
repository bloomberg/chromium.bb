// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/CompositorWorkerProxyClientImpl.h"

#include "core/dom/CompositorProxy.h"
#include "modules/compositorworker/CompositorWorkerGlobalScope.h"
#include "platform/graphics/CompositorMutableStateProvider.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/CurrentTime.h"
#include "web/CompositorMutatorImpl.h"

namespace blink {

// A helper class that updates proxies mutable state on creation and reset it
// on destruction. This can be used with rAF callback to ensure no mutation is
// allowed outside rAF.
class ScopedCompositorMutableState final {
  WTF_MAKE_NONCOPYABLE(ScopedCompositorMutableState);
  STACK_ALLOCATED();

 public:
  ScopedCompositorMutableState(CompositorProxyClientImpl* compositorProxyClient,
                               CompositorMutableStateProvider* stateProvider)
      : m_compositorProxyClient(compositorProxyClient) {
    DCHECK(!isMainThread());
    DCHECK(m_compositorProxyClient);
    for (CompositorProxy* proxy : m_compositorProxyClient->proxies()) {
      proxy->takeCompositorMutableState(
          stateProvider->getMutableStateFor(proxy->elementId()));
    }
  }
  ~ScopedCompositorMutableState() {
    for (CompositorProxy* proxy : m_compositorProxyClient->proxies())
      proxy->takeCompositorMutableState(nullptr);
  }

 private:
  Member<CompositorProxyClientImpl> m_compositorProxyClient;
};

CompositorWorkerProxyClientImpl::CompositorWorkerProxyClientImpl(
    CompositorMutatorImpl* mutator)
    : m_mutator(mutator) {
  DCHECK(isMainThread());
}

DEFINE_TRACE(CompositorWorkerProxyClientImpl) {
  CompositorAnimator::trace(visitor);
  CompositorWorkerProxyClient::trace(visitor);
}

void CompositorWorkerProxyClientImpl::setGlobalScope(WorkerGlobalScope* scope) {
  DCHECK(!isMainThread());
  TRACE_EVENT0("compositor-worker",
               "CompositorWorkerProxyClientImpl::setGlobalScope");
  DCHECK(!m_globalScope);
  DCHECK(!m_compositorProxyClient);
  DCHECK(scope);
  m_globalScope = static_cast<CompositorWorkerGlobalScope*>(scope);
  m_mutator->registerCompositorAnimator(this);
  m_compositorProxyClient = new CompositorProxyClientImpl();
}

void CompositorWorkerProxyClientImpl::dispose() {
  DCHECK(!isMainThread());
  // CompositorWorkerProxyClientImpl and CompositorMutatorImpl form a reference
  // cycle. CompositorWorkerGlobalScope and CompositorWorkerProxyClientImpl
  // also form another big reference cycle. So dispose needs to be called on
  // Worker termination to break these cycles. If not, layout test leak
  // detection will report a WorkerGlobalScope leak.
  m_mutator->unregisterCompositorAnimator(this);
  m_globalScope = nullptr;
}

void CompositorWorkerProxyClientImpl::requestAnimationFrame() {
  DCHECK(!isMainThread());
  TRACE_EVENT0("compositor-worker",
               "CompositorWorkerProxyClientImpl::requestAnimationFrame");
  m_requestedAnimationFrameCallbacks = true;
  m_mutator->setNeedsMutate();
}

bool CompositorWorkerProxyClientImpl::mutate(
    double monotonicTimeNow,
    CompositorMutableStateProvider* stateProvider) {
  DCHECK(!isMainThread());
  if (!m_globalScope)
    return false;

  TRACE_EVENT0("compositor-worker", "CompositorWorkerProxyClientImpl::mutate");
  if (!m_requestedAnimationFrameCallbacks)
    return false;

  {
    ScopedCompositorMutableState mutableState(m_compositorProxyClient,
                                              stateProvider);
    m_requestedAnimationFrameCallbacks =
        executeAnimationFrameCallbacks(monotonicTimeNow);
  }

  return m_requestedAnimationFrameCallbacks;
}

bool CompositorWorkerProxyClientImpl::executeAnimationFrameCallbacks(
    double monotonicTimeNow) {
  DCHECK(!isMainThread());
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

}  // namespace blink
