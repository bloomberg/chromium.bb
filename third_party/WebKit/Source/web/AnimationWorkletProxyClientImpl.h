// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorkletProxyClientImpl_h
#define AnimationWorkletProxyClientImpl_h

#include "core/dom/AnimationWorkletProxyClient.h"
#include "platform/heap/Handle.h"
#include "web/CompositorAnimator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CompositorMutatorImpl;

// Mediates between one Animator and the associated CompositorMutatorImpl. There
// is one AnimationWorkletProxyClientImpl per Animator but there may be multiple
// for a given mutator and animatorWorklet.
//
// This is constructed on the main thread but it is used in the worklet backing
// thread i.e., compositor thread.
class AnimationWorkletProxyClientImpl final
    : public AnimationWorkletProxyClient,
      public CompositorAnimator {
  WTF_MAKE_NONCOPYABLE(AnimationWorkletProxyClientImpl);
  USING_GARBAGE_COLLECTED_MIXIN(AnimationWorkletProxyClientImpl);

 public:
  explicit AnimationWorkletProxyClientImpl(CompositorMutatorImpl*);
  DECLARE_VIRTUAL_TRACE();

  // CompositorAnimator:
  // This method is invoked in compositor thread
  bool mutate(double monotonicTimeNow,
              CompositorMutableStateProvider*) override;

  // CompositorProxyClient:
  void registerCompositorProxy(CompositorProxy*) override;
  void unregisterCompositorProxy(CompositorProxy*) override;

 private:
  CrossThreadPersistent<CompositorMutatorImpl> m_mutator;
  HeapHashSet<WeakMember<CompositorProxy>> m_proxies;
};

}  // namespace blink

#endif  // AnimationWorkletProxyClientImpl_h
