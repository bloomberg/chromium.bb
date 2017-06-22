// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutatorImpl_h
#define CompositorMutatorImpl_h

#include <memory>
#include "core/animation/CustomCompositorAnimationManager.h"
#include "platform/graphics/CompositorMutator.h"
#include "platform/heap/Handle.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class CompositorAnimator;
class CompositorMutatorClient;

// Fans out requests from the compositor to all of the registered
// CompositorAnimators which can then mutate layers through their respective
// mutate interface. Requests for animation frames are received from
// CompositorAnimators and sent to the compositor to generate a new compositor
// frame.
//
// Owned by the control thread (unless threaded compositing is disabled).
// Should be accessed only on the compositor thread.
class CORE_EXPORT CompositorMutatorImpl final : public CompositorMutator {
  WTF_MAKE_NONCOPYABLE(CompositorMutatorImpl);

 public:
  static std::unique_ptr<CompositorMutatorClient> CreateClient();
  static CompositorMutatorImpl* Create();

  // CompositorMutator implementation.
  bool Mutate(double monotonic_time_now) override;

  void RegisterCompositorAnimator(CompositorAnimator*);
  void UnregisterCompositorAnimator(CompositorAnimator*);

  void SetNeedsMutate();

  void SetClient(CompositorMutatorClient* client) { client_ = client; }
  CustomCompositorAnimationManager* AnimationManager() {
    return animation_manager_.get();
  }

 private:
  CompositorMutatorImpl();

  using CompositorAnimators =
      HashSet<CrossThreadPersistent<CompositorAnimator>>;
  CompositorAnimators animators_;

  std::unique_ptr<CustomCompositorAnimationManager> animation_manager_;
  CompositorMutatorClient* client_;
};

}  // namespace blink

#endif  // CompositorMutatorImpl_h
