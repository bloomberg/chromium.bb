// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerProxyClientImpl_h
#define CompositorWorkerProxyClientImpl_h

#include "core/dom/CompositorWorkerProxyClient.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "web/CompositorAnimator.h"
#include "web/CompositorProxyClientImpl.h"

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
  bool Mutate(double monotonic_time_now,
              CompositorMutableStateProvider*) override;

  // CompositorWorkerProxyClient:
  void Dispose() override;
  void SetGlobalScope(WorkerGlobalScope*) override;
  void RequestAnimationFrame() override;
  CompositorProxyClient* GetCompositorProxyClient() override {
    return compositor_proxy_client_.Get();
  };

 private:
  bool ExecuteAnimationFrameCallbacks(double monotonic_time_now);

  CrossThreadPersistent<CompositorMutatorImpl> mutator_;

  CrossThreadPersistent<CompositorWorkerGlobalScope> global_scope_;
  bool requested_animation_frame_callbacks_;

  CrossThreadPersistent<CompositorProxyClientImpl> compositor_proxy_client_;
};

}  // namespace blink

#endif  // CompositorWorkerProxyClientImpl_h
