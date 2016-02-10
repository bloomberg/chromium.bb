// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationTimeline_h
#define CompositorAnimationTimeline_h

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/animation/animation_timeline.h"
#include "platform/PlatformExport.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CompositorAnimationPlayerClient;

// A compositor representation for timeline.
class PLATFORM_EXPORT CompositorAnimationTimeline {
    WTF_MAKE_NONCOPYABLE(CompositorAnimationTimeline);
public:
    CompositorAnimationTimeline();
    virtual ~CompositorAnimationTimeline();

    cc::AnimationTimeline* animationTimeline() const;

    virtual void playerAttached(const CompositorAnimationPlayerClient&);
    virtual void playerDestroyed(const CompositorAnimationPlayerClient&);

private:
    scoped_refptr<cc::AnimationTimeline> m_animationTimeline;
};

} // namespace blink

#endif // CompositorAnimationTimeline_h
