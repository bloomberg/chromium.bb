// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutatorImpl_h
#define CompositorMutatorImpl_h

#include "core/animation/CustomCompositorAnimationManager.h"
#include "platform/graphics/CompositorMutator.h"
#include "platform/heap/Handle.h"
#include "platform/heap/HeapAllocator.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class CompositorAnimator;
class CompositorMutatorClient;

// Fans out requests from the compositor to all of the registered
// CompositorAnimators which can then mutate layers through their respective
// mutate interface. Requests for animation frames are received from
// CompositorAnimators and sent to the compositor to generate a new compositor
// frame.
//
// Should be accessed only on the compositor thread.
class CompositorMutatorImpl final : public CompositorMutator {
  WTF_MAKE_NONCOPYABLE(CompositorMutatorImpl);

 public:
  static std::unique_ptr<CompositorMutatorClient> createClient();
  static CompositorMutatorImpl* create();

  // CompositorMutator implementation.
  bool mutate(double monotonicTimeNow,
              CompositorMutableStateProvider*) override;

  void registerCompositorAnimator(CompositorAnimator*);
  void unregisterCompositorAnimator(CompositorAnimator*);

  void setNeedsMutate();

  void setClient(CompositorMutatorClient* client) { m_client = client; }
  CustomCompositorAnimationManager* animationManager() {
    return m_animationManager.get();
  }

 private:
  CompositorMutatorImpl();

  using CompositorAnimators =
      HashSet<CrossThreadPersistent<CompositorAnimator>>;
  CompositorAnimators m_animators;

  std::unique_ptr<CustomCompositorAnimationManager> m_animationManager;
  CompositorMutatorClient* m_client;
};

}  // namespace blink

#endif  // CompositorMutatorImpl_h
