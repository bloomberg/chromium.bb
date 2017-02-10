// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerProxyClientImpl_h
#define CompositorWorkerProxyClientImpl_h

#include "core/dom/CompositorWorkerProxyClient.h"
#include "platform/heap/Handle.h"
#include "web/CompositorAnimator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CompositorMutableStateProvider;
class CompositorMutatorImpl;
class CompositorWorkerGlobalScope;
class WorkerGlobalScope;

// Mediates between one CompositorWorkerGlobalScope and the associated
// CompositorMutatorImpl. There is one CompositorWorkerProxyClientImpl per
// worker but there may be multiple for a given mutator, e.g. if a single
// document creates multiple CompositorWorker objects.
//
// Should be accessed only on the compositor thread.
class CompositorWorkerProxyClientImpl final
    : public CompositorWorkerProxyClient,
      public CompositorAnimator {
  WTF_MAKE_NONCOPYABLE(CompositorWorkerProxyClientImpl);
  USING_GARBAGE_COLLECTED_MIXIN(CompositorWorkerProxyClientImpl);

 public:
  explicit CompositorWorkerProxyClientImpl(CompositorMutatorImpl*);
  DECLARE_TRACE();

  // CompositorAnimator:
  bool mutate(double monotonicTimeNow,
              CompositorMutableStateProvider*) override;

  // CompositorWorkerProxyClient:
  void dispose() override;
  void setGlobalScope(WorkerGlobalScope*) override;
  void requestAnimationFrame() override;
  void registerCompositorProxy(CompositorProxy*) override;
  void unregisterCompositorProxy(CompositorProxy*) override;

 private:
  bool executeAnimationFrameCallbacks(double monotonicTimeNow);

  CrossThreadPersistent<CompositorMutatorImpl> m_mutator;

  CrossThreadPersistent<CompositorWorkerGlobalScope> m_globalScope;
  bool m_requestedAnimationFrameCallbacks;

  // TODO(majidvp): move this out to a separate class that can be composed in.
  HeapHashSet<WeakMember<CompositorProxy>> m_proxies;
};

}  // namespace blink

#endif  // CompositorWorkerProxyClientImpl_h
