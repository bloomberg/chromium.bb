// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationHost_h
#define CompositorAnimationHost_h

#include "platform/PlatformExport.h"
#include "platform/graphics/CompositorElementId.h"
#include "ui/gfx/geometry/vector2d.h"
#include "wtf/Noncopyable.h"

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

  void addTimeline(const CompositorAnimationTimeline&);
  void removeTimeline(const CompositorAnimationTimeline&);

  void adjustImplOnlyScrollOffsetAnimation(CompositorElementId,
                                           const gfx::Vector2dF& adjustment);
  void takeOverImplOnlyScrollOffsetAnimation(CompositorElementId);

 private:
  cc::AnimationHost* m_animationHost;
};

}  // namespace blink

#endif  // CompositorAnimationTimeline_h
