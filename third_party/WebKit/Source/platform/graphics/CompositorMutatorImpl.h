// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutatorImpl_h
#define CompositorMutatorImpl_h

#include <memory>

#include "base/macros.h"
#include "platform/graphics/CompositorAnimator.h"
#include "platform/graphics/CompositorMutator.h"
#include "platform/heap/Handle.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {

class CompositorMutatorClient;

// Fans out requests from the compositor to all of the registered
// CompositorAnimators which can then mutate layers through their respective
// mutate interface. Requests for animation frames are received from
// CompositorAnimators and sent to the compositor to generate a new compositor
// frame.
//
// Owned by the control thread (unless threaded compositing is disabled).
// Should be accessed only on the compositor thread.
class PLATFORM_EXPORT CompositorMutatorImpl final : public CompositorMutator {
 public:
  static std::unique_ptr<CompositorMutatorClient> CreateClient();
  static CompositorMutatorImpl* Create();

  // CompositorMutator implementation.
  void Mutate(std::unique_ptr<CompositorMutatorInputState>) override;
  // TODO(majidvp): Remove when timeline inputs are known.
  bool HasAnimators() override;

  void RegisterCompositorAnimator(CompositorAnimator*);
  void UnregisterCompositorAnimator(CompositorAnimator*);

  void SetMutationUpdate(std::unique_ptr<CompositorMutatorOutputState>);
  void SetClient(CompositorMutatorClient* client) { client_ = client; }

 private:
  CompositorMutatorImpl();

  using CompositorAnimators =
      HashSet<CrossThreadPersistent<CompositorAnimator>>;
  CompositorAnimators animators_;

  CompositorMutatorClient* client_;
  DISALLOW_COPY_AND_ASSIGN(CompositorMutatorImpl);
};

}  // namespace blink

#endif  // CompositorMutatorImpl_h
