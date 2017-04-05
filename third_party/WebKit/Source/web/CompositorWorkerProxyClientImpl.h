// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerProxyClientImpl_h
#define CompositorWorkerProxyClientImpl_h

#include "core/dom/CompositorWorkerProxyClient.h"
#include "platform/heap/Handle.h"
#include "web/CompositorAnimator.h"
#include "web/CompositorProxyClientImpl.h"
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
// Owned by the main thread.
// Should be accessed only on the compositor thread.
class CompositorWorkerProxyClientImpl final
    : public GarbageCollectedFinalized<CompositorWorkerProxyClientImpl>,
      public CompositorWorkerProxyClient,
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
  CompositorProxyClient* compositorProxyClient() override {
    return m_compositorProxyClient.get();
  };

 private:
  bool executeAnimationFrameCallbacks(double monotonicTimeNow);

  CrossThreadPersistent<CompositorMutatorImpl> m_mutator;

  CrossThreadPersistent<CompositorWorkerGlobalScope> m_globalScope;
  bool m_requestedAnimationFrameCallbacks;

  CrossThreadPersistent<CompositorProxyClientImpl> m_compositorProxyClient;
};

}  // namespace blink

#endif  // CompositorWorkerProxyClientImpl_h
