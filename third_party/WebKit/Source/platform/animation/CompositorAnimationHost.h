// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationHost_h
#define CompositorAnimationHost_h

#include "platform/PlatformExport.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/wtf/Noncopyable.h"
#include "ui/gfx/geometry/vector2d.h"

namespace cc {
class AnimationHost;
}

namespace blink {

class CompositorAnimationTimeline;

// A compositor representation for cc::AnimationHost.
class PLATFORM_EXPORT CompositorAnimationHost {
  WTF_MAKE_NONCOPYABLE(CompositorAnimationHost);

 public:
  explicit CompositorAnimationHost(cc::AnimationHost*);

  void AddTimeline(const CompositorAnimationTimeline&);
  void RemoveTimeline(const CompositorAnimationTimeline&);

  void AdjustImplOnlyScrollOffsetAnimation(CompositorElementId,
                                           const gfx::Vector2dF& adjustment);
  void TakeOverImplOnlyScrollOffsetAnimation(CompositorElementId);
  void SetAnimationCounts(size_t total_animations_count,
                          size_t main_thread_compositable_animations_count);
  size_t GetMainThreadAnimationsCountForTesting();
  size_t GetMainThreadCompositableAnimationsCountForTesting();
  size_t GetCompositedAnimationsCountForTesting();

 private:
  cc::AnimationHost* animation_host_;
};

}  // namespace blink

#endif  // CompositorAnimationTimeline_h
