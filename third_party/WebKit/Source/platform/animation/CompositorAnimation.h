// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimation_h
#define CompositorAnimation_h

#include "cc/animation/animation.h"
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorTargetProperty.h"
#include "wtf/Noncopyable.h"

#include <memory>

namespace cc {
class Animation;
}

namespace blink {

class CompositorAnimationCurve;

// A compositor driven animation.
class PLATFORM_EXPORT CompositorAnimation {
    WTF_MAKE_NONCOPYABLE(CompositorAnimation);
public:
    using Direction = cc::Animation::Direction;
    using FillMode = cc::Animation::FillMode;

    CompositorAnimation(const CompositorAnimationCurve&, CompositorTargetProperty::Type, int animationId, int groupId);
    virtual ~CompositorAnimation();

    // An id must be unique.
    virtual int id();
    virtual int group();

    virtual CompositorTargetProperty::Type targetProperty() const;

    // This is the number of times that the animation will play. If this
    // value is zero the animation will not play. If it is negative, then
    // the animation will loop indefinitely.
    virtual double iterations() const;
    virtual void setIterations(double);

    virtual double startTime() const;
    virtual void setStartTime(double monotonicTime);

    virtual double timeOffset() const;
    virtual void setTimeOffset(double monotonicTime);

    virtual Direction getDirection() const;
    virtual void setDirection(Direction);

    virtual double playbackRate() const;
    virtual void setPlaybackRate(double);

    virtual FillMode getFillMode() const;
    virtual void setFillMode(FillMode);

    virtual double iterationStart() const;
    virtual void setIterationStart(double);

    std::unique_ptr<cc::Animation> passAnimation();

protected:
    CompositorAnimation();

private:
    std::unique_ptr<cc::Animation> m_animation;
};

} // namespace blink

#endif // CompositorAnimation_h
