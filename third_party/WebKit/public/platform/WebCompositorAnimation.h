// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCompositorAnimation_h
#define WebCompositorAnimation_h

#define WEB_ANIMATION_SUPPORTS_FRACTIONAL_ITERATIONS 1
#define WEB_ANIMATION_SUPPORTS_FULL_DIRECTION 1

namespace blink {

// A compositor driven animation.
class WebCompositorAnimation {
public:
    enum TargetProperty {
        TargetPropertyTransform = 0,
        TargetPropertyOpacity,
        TargetPropertyFilter,
        TargetPropertyScrollOffset
    };

    enum Direction {
        DirectionNormal = 0,
        DirectionReverse,
        DirectionAlternate,
        DirectionAlternateReverse
    };

    virtual ~WebCompositorAnimation() { }

    // An id is effectively the animation's name, and it is not unique.
    virtual int id() = 0;

    virtual TargetProperty targetProperty() const = 0;

    // This is the number of times that the animation will play. If this
    // value is zero the animation will not play. If it is negative, then
    // the animation will loop indefinitely.
    virtual double iterations() const = 0;
    virtual void setIterations(double) = 0;

    virtual double startTime() const = 0;
    virtual void setStartTime(double monotonicTime) = 0;

    virtual double timeOffset() const = 0;
    virtual void setTimeOffset(double monotonicTime) = 0;

    virtual Direction direction() const = 0;
    virtual void setDirection(Direction) = 0;
};

} // namespace blink

#endif // WebCompositorAnimation_h
