// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PrimitiveInterpolation_h
#define PrimitiveInterpolation_h

#include "core/animation/AnimationValue.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include <cmath>

namespace blink {

class StyleResolverState;

// Represents a conversion from a pair of keyframes to something compatible with interpolation.
// This is agnostic to whether the keyframes are compatible with each other or not.
class PrimitiveInterpolation : public NoBaseWillBeGarbageCollectedFinalized<PrimitiveInterpolation> {
public:
    virtual ~PrimitiveInterpolation() { }

    virtual void interpolate(double fraction, AnimationValue& result) const = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() { }
};

// Represents a pair of keyframes that are compatible for "smooth" interpolation eg. "0px" and "100px".
class PairwisePrimitiveInterpolation : public PrimitiveInterpolation {
public:
    virtual ~PairwisePrimitiveInterpolation() { }

    static PassOwnPtrWillBeRawPtr<PairwisePrimitiveInterpolation> create(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<NonInterpolableValue> nonInterpolableValue)
    {
        return adoptPtrWillBeNoop(new PairwisePrimitiveInterpolation(start, end, nonInterpolableValue));
    }

    void initializeAnimationValue(AnimationValue& value)
    {
        value.interpolableValue = m_start->clone();
        value.nonInterpolableValue = m_nonInterpolableValue;
    }

private:
    virtual void interpolate(double fraction, AnimationValue& result) const override final
    {
        ASSERT(result.nonInterpolableValue == m_nonInterpolableValue);
        m_start->interpolate(*m_end, fraction, *result.interpolableValue);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        PrimitiveInterpolation::trace(visitor);
        visitor->trace(m_start);
        visitor->trace(m_end);
        visitor->trace(m_nonInterpolableValue);
    }

    PairwisePrimitiveInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<NonInterpolableValue> nonInterpolableValue)
        : m_start(start)
        , m_end(end)
        , m_nonInterpolableValue(nonInterpolableValue)
    { }
    OwnPtrWillBeRawPtr<InterpolableValue> m_start;
    OwnPtrWillBeRawPtr<InterpolableValue> m_end;
    RefPtrWillBeRawPtr<NonInterpolableValue> m_nonInterpolableValue;
};

// Represents a pair of incompatible keyframes that fall back to 50% flip behaviour eg. "auto" and "0px".
class FlipPrimitiveInterpolation : public PrimitiveInterpolation {
public:
    struct Side : public NoBaseWillBeGarbageCollectedFinalized<Side> {
        const AnimationType& type;
        OwnPtrWillBeMember<InterpolableValue> interpolableValue;
        RefPtrWillBeMember<NonInterpolableValue> nonInterpolableValue;

        static PassOwnPtrWillBeRawPtr<Side> create(const AnimationType& type) { return adoptPtrWillBeNoop(new Side(type)); }

        DEFINE_INLINE_TRACE()
        {
            visitor->trace(interpolableValue);
            visitor->trace(nonInterpolableValue);
        }

    private:
        Side(const AnimationType& type)
            : type(type)
        { }
    };

    virtual ~FlipPrimitiveInterpolation() { }

    static PassOwnPtrWillBeRawPtr<FlipPrimitiveInterpolation> create(PassOwnPtrWillBeRawPtr<Side> start, PassOwnPtrWillBeRawPtr<Side> end)
    {
        return adoptPtrWillBeNoop(new FlipPrimitiveInterpolation(start, end));
    }

private:
    virtual void interpolate(double fraction, AnimationValue& result) const override final
    {
        // TODO(alancutter): Remove this optimisation once Oilpan is default.
        if (!std::isnan(m_lastFraction) && (fraction < 0.5) == (m_lastFraction < 0.5))
            return;
        result.copyFrom((fraction < 0.5) ? m_start : m_end);
        m_lastFraction = fraction;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        PrimitiveInterpolation::trace(visitor);
        visitor->trace(m_start);
        visitor->trace(m_end);
    }

    FlipPrimitiveInterpolation(PassOwnPtrWillBeRawPtr<Side> start, PassOwnPtrWillBeRawPtr<Side> end)
        : m_start(&start->type, start->interpolableValue.release(), start->nonInterpolableValue.release())
        , m_end(&end->type, end->interpolableValue.release(), end->nonInterpolableValue.release())
        , m_lastFraction(std::numeric_limits<double>::quiet_NaN())
    { }

    AnimationValue m_start;
    AnimationValue m_end;
    mutable double m_lastFraction;
};

} // namespace blink

#endif // PrimitiveInterpolation_h
