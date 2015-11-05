// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationType_h
#define InterpolationType_h

#include "core/animation/InterpolationValue.h"
#include "core/animation/Keyframe.h"
#include "core/animation/PrimitiveInterpolation.h"
#include "core/animation/PropertyHandle.h"
#include "core/animation/UnderlyingValue.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"

namespace blink {

class InterpolationEnvironment;

// A singleton that:
// - Converts from animation keyframe(s) to interpolation compatible representations: maybeConvertPairwise() and maybeConvertSingle()
// - Applies interpolation compatible representations of values to a StyleResolverState: apply()
class InterpolationType {
    USING_FAST_MALLOC(InterpolationType);
    WTF_MAKE_NONCOPYABLE(InterpolationType);
public:
    virtual ~InterpolationType() { ASSERT_NOT_REACHED(); }

    PropertyHandle property() const { return m_property; }

    // Represents logic for determining whether a conversion decision is no longer valid given the current environment.
    class ConversionChecker {
        USING_FAST_MALLOC(ConversionChecker);
        WTF_MAKE_NONCOPYABLE(ConversionChecker);
    public:
        virtual ~ConversionChecker() { }
        virtual bool isValid(const InterpolationEnvironment&, const UnderlyingValue&) const = 0;
        const InterpolationType& type() const { return m_type; }
        DEFINE_INLINE_VIRTUAL_TRACE() { }
    protected:
        ConversionChecker(const InterpolationType& type)
            : m_type(type)
        { }
        const InterpolationType& m_type;
    };
    using ConversionCheckers = Vector<OwnPtr<ConversionChecker>>;

    virtual PassOwnPtr<PairwisePrimitiveInterpolation> maybeConvertPairwise(const PropertySpecificKeyframe& startKeyframe, const PropertySpecificKeyframe& endKeyframe, const InterpolationEnvironment& environment, const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
    {
        OwnPtr<InterpolationValue> startValue = maybeConvertSingle(startKeyframe, environment, underlyingValue, conversionCheckers);
        if (!startValue)
            return nullptr;
        OwnPtr<InterpolationValue> endValue = maybeConvertSingle(endKeyframe, environment, underlyingValue, conversionCheckers);
        if (!endValue)
            return nullptr;
        return mergeSingleConversions(*startValue, *endValue);
    }

    virtual PassOwnPtr<InterpolationValue> maybeConvertSingle(const PropertySpecificKeyframe&, const InterpolationEnvironment&, const UnderlyingValue&, ConversionCheckers&) const = 0;

    virtual PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const InterpolationEnvironment&) const = 0;

    virtual void composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
    {
        ASSERT(!underlyingValue->nonInterpolableValue());
        ASSERT(!value.nonInterpolableValue());
        underlyingValue.mutableComponent().interpolableValue->scaleAndAdd(underlyingFraction, value.interpolableValue());
    }

    virtual void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const = 0;

    // Implement reference equality checking via pointer equality checking as these are singletons.
    bool operator==(const InterpolationType& other) const { return this == &other; }
    bool operator!=(const InterpolationType& other) const { return this != &other; }

protected:
    InterpolationType(PropertyHandle property)
        : m_property(property)
    { }

    virtual PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
    {
        ASSERT(!startValue.nonInterpolableValue());
        ASSERT(!endValue.nonInterpolableValue());
        return PairwisePrimitiveInterpolation::create(*this,
            startValue.mutableComponent().interpolableValue.release(),
            endValue.mutableComponent().interpolableValue.release(),
            nullptr);
    }

    const PropertyHandle m_property;
};

} // namespace blink

#endif // InterpolationType_h
