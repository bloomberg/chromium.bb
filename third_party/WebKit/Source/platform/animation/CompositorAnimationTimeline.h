// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationTimeline_h
#define CompositorAnimationTimeline_h

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "cc/animation/animation_timeline.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

class CompositorAnimationPlayerClient;

// A compositor representation for cc::AnimationTimeline.
class PLATFORM_EXPORT CompositorAnimationTimeline {
  WTF_MAKE_NONCOPYABLE(CompositorAnimationTimeline);

 public:
  static std::unique_ptr<CompositorAnimationTimeline> Create() {
    return WTF::WrapUnique(new CompositorAnimationTimeline());
  }

  ~CompositorAnimationTimeline();

  cc::AnimationTimeline* GetAnimationTimeline() const;

  void PlayerAttached(const CompositorAnimationPlayerClient&);
  void PlayerDestroyed(const CompositorAnimationPlayerClient&);

 private:
  CompositorAnimationTimeline();

  scoped_refptr<cc::AnimationTimeline> animation_timeline_;
};

}  // namespace blink

#endif  // CompositorAnimationTimeline_h
