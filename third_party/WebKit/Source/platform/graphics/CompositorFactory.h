// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFactory_h
#define CompositorFactory_h

#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorScrollOffsetAnimationCurve.h"
#include "platform/geometry/FloatPoint.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class CompositorAnimationPlayer;
class CompositorAnimationTimeline;
class CompositorFilterAnimationCurve;
class CompositorFilterOperations;
class CompositorFloatAnimationCurve;
class CompositorTransformAnimationCurve;
class CompositorTransformOperations;

class PLATFORM_EXPORT CompositorFactory {
public:
    static void initializeDefault();
    static void initializeForTesting(PassOwnPtr<CompositorFactory>);
    static void shutdown();
    static CompositorFactory& current();

    // Animation ----------------------------------------------------

    virtual CompositorFilterAnimationCurve* createFilterAnimationCurve() { return nullptr; }

    virtual CompositorFloatAnimationCurve* createFloatAnimationCurve() { return nullptr; }

    virtual CompositorScrollOffsetAnimationCurve* createScrollOffsetAnimationCurve(
        FloatPoint targetValue,
        CompositorAnimationCurve::TimingFunctionType,
        CompositorScrollOffsetAnimationCurve::ScrollDurationBehavior) { return nullptr; }

    virtual CompositorTransformAnimationCurve* createTransformAnimationCurve() { return nullptr; }

    virtual CompositorTransformOperations* createTransformOperations() { return nullptr; }

    virtual CompositorFilterOperations* createFilterOperations() { return nullptr; }

    virtual CompositorAnimation* createAnimation(const CompositorAnimationCurve&, CompositorAnimation::TargetProperty, int groupId = 0, int animationId = 0) { return nullptr; }

    virtual CompositorAnimationPlayer* createAnimationPlayer() { return nullptr; }

    virtual CompositorAnimationTimeline* createAnimationTimeline() { return nullptr; }

    virtual ~CompositorFactory() {}
};

} // namespace blink

#endif // CompositorFactory_h
