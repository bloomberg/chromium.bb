// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PrimitiveInterpolation_h
#define PrimitiveInterpolation_h

#include "core/animation/InterpolationValue.h"
#include "platform/animation/AnimationUtilities.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include <cmath>

namespace blink {

class StyleResolverState;

// Represents a conversion from a pair of keyframes to something compatible with interpolation.
// This is agnostic to whether the keyframes are compatible with each other or not.
class PrimitiveInterpolation : public NoBaseWillBeGarbageCollectedFinalized<PrimitiveInterpolation> {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(PrimitiveInterpolation);
    WTF_MAKE_NONCOPYABLE(PrimitiveInterpolation);
public:
    virtual ~PrimitiveInterpolation() { }

    virtual void interpolateValue(double fraction, OwnPtrWillBeMember<InterpolationValue>& result) const = 0;
    virtual double interpolateUnderlyingFraction(double start, double end, double fraction) const = 0;
    virtual bool isFlip() const { return false; }

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    PrimitiveInterpolation() { }
};

// Represents a pair of keyframes that are compatible for "smooth" interpolation eg. "0px" and "100px".
class PairwisePrimitiveInterpolation : public PrimitiveInterpolation {
public:
    ~PairwisePrimitiveInterpolation() override { }

    static PassOwnPtrWillBeRawPtr<PairwisePrimitiveInterpolation> create(const InterpolationType& type, PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<NonInterpolableValue> nonInterpolableValue)
    {
        return adoptPtrWillBeNoop(new PairwisePrimitiveInterpolation(type, start, end, nonInterpolableValue));
    }

    PassOwnPtrWillBeRawPtr<InterpolationValue> initialValue() const
    {
        return InterpolationValue::create(m_type, m_start->clone(), m_nonInterpolableValue);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_start);
        visitor->trace(m_end);
        visitor->trace(m_nonInterpolableValue);
        PrimitiveInterpolation::trace(visitor);
    }

private:
    PairwisePrimitiveInterpolation(const InterpolationType& type, PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<NonInterpolableValue> nonInterpolableValue)
        : m_type(type)
        , m_start(start)
        , m_end(end)
        , m_nonInterpolableValue(nonInterpolableValue)
    { }

    void interpolateValue(double fraction, OwnPtrWillBeMember<InterpolationValue>& result) const final
    {
        ASSERT(result);
        ASSERT(&result->type() == &m_type);
        ASSERT(result->nonInterpolableValue() == m_nonInterpolableValue.get());
        m_start->interpolate(*m_end, fraction, result->interpolableValue());
    }

    double interpolateUnderlyingFraction(double start, double end, double fraction) const final { return blend(start, end, fraction); }

    const InterpolationType& m_type;
    OwnPtrWillBeMember<InterpolableValue> m_start;
    OwnPtrWillBeMember<InterpolableValue> m_end;
    RefPtrWillBeMember<NonInterpolableValue> m_nonInterpolableValue;
};

// Represents a pair of incompatible keyframes that fall back to 50% flip behaviour eg. "auto" and "0px".
class FlipPrimitiveInterpolation : public PrimitiveInterpolation {
public:
    ~FlipPrimitiveInterpolation() override { }

    static PassOwnPtrWillBeRawPtr<FlipPrimitiveInterpolation> create(PassOwnPtrWillBeRawPtr<InterpolationValue> start, PassOwnPtrWillBeRawPtr<InterpolationValue> end)
    {
        return adoptPtrWillBeNoop(new FlipPrimitiveInterpolation(start, end));
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_start);
        visitor->trace(m_end);
        PrimitiveInterpolation::trace(visitor);
    }

private:
    FlipPrimitiveInterpolation(PassOwnPtrWillBeRawPtr<InterpolationValue> start, PassOwnPtrWillBeRawPtr<InterpolationValue> end)
        : m_start(start)
        , m_end(end)
        , m_lastFraction(std::numeric_limits<double>::quiet_NaN())
    { }

    void interpolateValue(double fraction, OwnPtrWillBeMember<InterpolationValue>& result) const final
    {
        // TODO(alancutter): Remove this optimisation once Oilpan is default.
        if (!std::isnan(m_lastFraction) && (fraction < 0.5) == (m_lastFraction < 0.5))
            return;
        const InterpolationValue* side = ((fraction < 0.5) ? m_start : m_end).get();
        result = side ? side->clone() : nullptr;
        m_lastFraction = fraction;
    }

    double interpolateUnderlyingFraction(double start, double end, double fraction) const final { return fraction < 0.5 ? start : end; }

    bool isFlip() const final { return true; }

    OwnPtrWillBeMember<InterpolationValue> m_start;
    OwnPtrWillBeMember<InterpolationValue> m_end;
    mutable double m_lastFraction;
};

} // namespace blink

#endif // PrimitiveInterpolation_h
