// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationType_h
#define InterpolationType_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/InterpolationValue.h"
#include "core/animation/NonInterpolableValue.h"
#include "core/animation/PrimitiveInterpolation.h"
#include "core/animation/StringKeyframe.h"
#include "core/animation/UnderlyingValue.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"

namespace blink {

class StyleResolverState;

// A singleton that:
// - Converts from animation keyframe(s) to interpolation compatible representations: maybeConvertPairwise() and maybeConvertSingle()
// - Applies interpolation compatible representations of values to a StyleResolverState: apply()
class InterpolationType {
    WTF_MAKE_FAST_ALLOCATED(InterpolationType);
    WTF_MAKE_NONCOPYABLE(InterpolationType);
public:
    CSSPropertyID property() const { return m_property; }

    // Represents logic for determining whether a conversion decision is no longer valid given the current environment.
    class ConversionChecker {
        WTF_MAKE_FAST_ALLOCATED(ConversionChecker);
        WTF_MAKE_NONCOPYABLE(ConversionChecker);
    public:
        virtual ~ConversionChecker() { }
        virtual bool isValid(const StyleResolverState&, const UnderlyingValue&) const = 0;
        const InterpolationType& type() const { return m_type; }
        DEFINE_INLINE_VIRTUAL_TRACE() { }
    protected:
        ConversionChecker(const InterpolationType& type)
            : m_type(type)
        { }
        const InterpolationType& m_type;
    };
    using ConversionCheckers = Vector<OwnPtr<ConversionChecker>>;

    virtual PassOwnPtr<PairwisePrimitiveInterpolation> maybeConvertPairwise(const CSSPropertySpecificKeyframe& startKeyframe, const CSSPropertySpecificKeyframe& endKeyframe, const StyleResolverState* state, const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
    {
        OwnPtr<InterpolationValue> startValue = maybeConvertSingle(startKeyframe, state, underlyingValue, conversionCheckers);
        if (!startValue)
            return nullptr;
        OwnPtr<InterpolationValue> endValue = maybeConvertSingle(endKeyframe, state, underlyingValue, conversionCheckers);
        if (!endValue)
            return nullptr;
        return mergeSingleConversions(*startValue, *endValue);
    }

    virtual PassOwnPtr<InterpolationValue> maybeConvertSingle(const CSSPropertySpecificKeyframe& keyframe, const StyleResolverState* state, const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
    {
        const CSSValue* value = keyframe.value();
        if (!value)
            return maybeConvertNeutral(underlyingValue, conversionCheckers);
        if (value->isInitialValue() || (value->isUnsetValue() && !CSSPropertyMetadata::isInheritedProperty(m_property)))
            return maybeConvertInitial();
        if (value->isInheritedValue() || (value->isUnsetValue() && CSSPropertyMetadata::isInheritedProperty(m_property)))
            return maybeConvertInherit(state, conversionCheckers);
        return maybeConvertValue(*value, state, conversionCheckers);
    }

    virtual PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const StyleResolverState&) const = 0;

    virtual void composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
    {
        ASSERT(!underlyingValue->nonInterpolableValue());
        ASSERT(!value.nonInterpolableValue());
        underlyingValue.mutableComponent().interpolableValue->scaleAndAdd(underlyingFraction, value.interpolableValue());
    }

    virtual void apply(const InterpolableValue&, const NonInterpolableValue*, StyleResolverState&) const = 0;

    // Implement reference equality checking via pointer equality checking as these are singletons.
    bool operator==(const InterpolationType& other) const { return this == &other; }
    bool operator!=(const InterpolationType& other) const { return this != &other; }

protected:
    InterpolationType(CSSPropertyID property)
        : m_property(property)
    { }

    virtual PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual PassOwnPtr<InterpolationValue> maybeConvertInitial() const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState*, ConversionCheckers&) const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue& value, const StyleResolverState* state, ConversionCheckers& conversionCheckers) const { ASSERT_NOT_REACHED(); return nullptr; }

    virtual PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
    {
        ASSERT(!startValue.nonInterpolableValue());
        ASSERT(!endValue.nonInterpolableValue());
        return PairwisePrimitiveInterpolation::create(*this,
            startValue.mutableComponent().interpolableValue.release(),
            endValue.mutableComponent().interpolableValue.release(),
            nullptr);
    }

    const CSSPropertyID m_property;
};

} // namespace blink

#endif // InterpolationType_h
