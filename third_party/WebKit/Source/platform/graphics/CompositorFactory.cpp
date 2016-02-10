// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorFactory.h"

#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/animation/CompositorFilterAnimationCurve.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorTransformAnimationCurve.h"
#include "platform/animation/CompositorTransformOperations.h"
#include "platform/graphics/CompositorFilterOperations.h"

namespace blink {

class CompositorFactoryImpl : public CompositorFactory {
public:
    CompositorFilterAnimationCurve* createFilterAnimationCurve() override
    {
        return new CompositorFilterAnimationCurve();
    }

    CompositorFloatAnimationCurve* createFloatAnimationCurve() override
    {
        return new CompositorFloatAnimationCurve();
    }

    CompositorScrollOffsetAnimationCurve* createScrollOffsetAnimationCurve(
        FloatPoint targetValue,
        CompositorAnimationCurve::TimingFunctionType timingFunctionType,
        CompositorScrollOffsetAnimationCurve::ScrollDurationBehavior durationBehavior)
    {
        return new CompositorScrollOffsetAnimationCurve(targetValue, timingFunctionType,
            durationBehavior);
    }

    CompositorTransformAnimationCurve* createTransformAnimationCurve() override
    {
        return new CompositorTransformAnimationCurve();
    }

    CompositorTransformOperations* createTransformOperations() override
    {
        return new CompositorTransformOperations();
    }

    CompositorFilterOperations* createFilterOperations() override
    {
        return new CompositorFilterOperations();
    }

    CompositorAnimation* createAnimation(const blink::CompositorAnimationCurve& curve, blink::CompositorAnimation::TargetProperty target, int groupId, int animationId) override
    {
        return new CompositorAnimation(curve, target, animationId, groupId);
    }

    CompositorAnimationPlayer* createAnimationPlayer()
    {
        return new CompositorAnimationPlayer();
    }

    CompositorAnimationTimeline* createAnimationTimeline()
    {
        return new CompositorAnimationTimeline();
    }
};

static CompositorFactory* s_factory = 0;

void CompositorFactory::initializeDefault()
{
    delete s_factory;
    s_factory = new CompositorFactoryImpl();
}

void CompositorFactory::initializeForTesting(PassOwnPtr<CompositorFactory> factory)
{
    delete s_factory;
    s_factory = factory.leakPtr();
}

void CompositorFactory::shutdown()
{
    delete s_factory;
    s_factory = nullptr;
}

CompositorFactory& CompositorFactory::current()
{
    ASSERT(s_factory);
    return *s_factory;
}

} // namespace blink
