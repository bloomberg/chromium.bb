// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PrimitiveInterpolation_h
#define PrimitiveInterpolation_h

#include "core/animation/TypedInterpolationValue.h"
#include "platform/animation/AnimationUtilities.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include <cmath>

namespace blink {

class StyleResolverState;

// Represents an animation's effect between an adjacent pair of PropertySpecificKeyframes after converting
// the keyframes to an internal format with respect to the animation environment and underlying values.
class PrimitiveInterpolation {
    USING_FAST_MALLOC(PrimitiveInterpolation);
    WTF_MAKE_NONCOPYABLE(PrimitiveInterpolation);
public:
    virtual ~PrimitiveInterpolation() { }

    virtual void interpolateValue(double fraction, OwnPtr<TypedInterpolationValue>& result) const = 0;
    virtual double interpolateUnderlyingFraction(double start, double end, double fraction) const = 0;
    virtual bool isFlip() const { return false; }

protected:
    PrimitiveInterpolation() { }
};

// Represents a pair of keyframes that are compatible for "smooth" interpolation eg. "0px" and "100px".
class PairwisePrimitiveInterpolation : public PrimitiveInterpolation {
public:
    ~PairwisePrimitiveInterpolation() override { }

    static PassOwnPtr<PairwisePrimitiveInterpolation> create(const InterpolationType& type, PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, PassRefPtr<NonInterpolableValue> nonInterpolableValue)
    {
        return adoptPtr(new PairwisePrimitiveInterpolation(type, std::move(start), std::move(end), nonInterpolableValue));
    }

    const InterpolationType& type() const { return m_type; }

    PassOwnPtr<TypedInterpolationValue> initialValue() const
    {
        return TypedInterpolationValue::create(m_type, m_start->clone(), m_nonInterpolableValue);
    }

private:
    PairwisePrimitiveInterpolation(const InterpolationType& type, PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, PassRefPtr<NonInterpolableValue> nonInterpolableValue)
        : m_type(type)
        , m_start(std::move(start))
        , m_end(std::move(end))
        , m_nonInterpolableValue(nonInterpolableValue)
    {
        ASSERT(m_start);
        ASSERT(m_end);
    }

    void interpolateValue(double fraction, OwnPtr<TypedInterpolationValue>& result) const final
    {
        ASSERT(result);
        ASSERT(&result->type() == &m_type);
        ASSERT(result->nonInterpolableValue() == m_nonInterpolableValue.get());
        m_start->interpolate(*m_end, fraction, *result->mutableValue().interpolableValue);
    }

    double interpolateUnderlyingFraction(double start, double end, double fraction) const final { return blend(start, end, fraction); }

    const InterpolationType& m_type;
    OwnPtr<InterpolableValue> m_start;
    OwnPtr<InterpolableValue> m_end;
    RefPtr<NonInterpolableValue> m_nonInterpolableValue;
};

// Represents a pair of incompatible keyframes that fall back to 50% flip behaviour eg. "auto" and "0px".
class FlipPrimitiveInterpolation : public PrimitiveInterpolation {
public:
    ~FlipPrimitiveInterpolation() override { }

    static PassOwnPtr<FlipPrimitiveInterpolation> create(PassOwnPtr<TypedInterpolationValue> start, PassOwnPtr<TypedInterpolationValue> end)
    {
        return adoptPtr(new FlipPrimitiveInterpolation(std::move(start), std::move(end)));
    }

private:
    FlipPrimitiveInterpolation(PassOwnPtr<TypedInterpolationValue> start, PassOwnPtr<TypedInterpolationValue> end)
        : m_start(std::move(start))
        , m_end(std::move(end))
        , m_lastFraction(std::numeric_limits<double>::quiet_NaN())
    { }

    void interpolateValue(double fraction, OwnPtr<TypedInterpolationValue>& result) const final
    {
        // TODO(alancutter): Remove this optimisation once Oilpan is default.
        if (!std::isnan(m_lastFraction) && (fraction < 0.5) == (m_lastFraction < 0.5))
            return;
        const TypedInterpolationValue* side = ((fraction < 0.5) ? m_start : m_end).get();
        result = side ? side->clone() : nullptr;
        m_lastFraction = fraction;
    }

    double interpolateUnderlyingFraction(double start, double end, double fraction) const final { return fraction < 0.5 ? start : end; }

    bool isFlip() const final { return true; }

    OwnPtr<TypedInterpolationValue> m_start;
    OwnPtr<TypedInterpolationValue> m_end;
    mutable double m_lastFraction;
};

} // namespace blink

#endif // PrimitiveInterpolation_h
