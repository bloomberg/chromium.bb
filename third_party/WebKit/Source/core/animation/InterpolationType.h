// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationType_h
#define InterpolationType_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/NonInterpolableValue.h"
#include "core/animation/PrimitiveInterpolation.h"
#include "core/animation/StringKeyframe.h"
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
    class ConversionChecker : public NoBaseWillBeGarbageCollectedFinalized<ConversionChecker> {
        WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(ConversionChecker);
        WTF_MAKE_NONCOPYABLE(ConversionChecker);
    public:
        virtual ~ConversionChecker() { }
        virtual bool isValid(const StyleResolverState&) const = 0;
        DEFINE_INLINE_VIRTUAL_TRACE() { }
    protected:
        ConversionChecker() { }
    };
    using ConversionCheckers = WillBeHeapVector<OwnPtrWillBeMember<ConversionChecker>>;

    virtual PassOwnPtrWillBeRawPtr<PairwisePrimitiveInterpolation> maybeConvertPairwise(const CSSPropertySpecificKeyframe& startKeyframe, const CSSPropertySpecificKeyframe& endKeyframe, const StyleResolverState* state, ConversionCheckers& conversionCheckers) const
    {
        OwnPtrWillBeRawPtr<InterpolationValue> startValue = maybeConvertSingle(startKeyframe, state, conversionCheckers);
        if (!startValue)
            return nullptr;
        OwnPtrWillBeRawPtr<InterpolationValue> endValue = maybeConvertSingle(endKeyframe, state, conversionCheckers);
        if (!endValue)
            return nullptr;
        return PairwisePrimitiveInterpolation::create(*this, startValue->m_interpolableValue.release(), endValue->m_interpolableValue.release(), startValue->m_nonInterpolableValue.release());
    }

    virtual PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertSingle(const CSSPropertySpecificKeyframe& keyframe, const StyleResolverState* state, ConversionCheckers& conversionCheckers) const
    {
        const CSSValue* value = keyframe.value();
        if (!value)
            return maybeConvertNeutral();
        if (value->isInitialValue() || (value->isUnsetValue() && !CSSPropertyMetadata::isInheritedProperty(m_property)))
            return maybeConvertInitial();
        if (value->isInheritedValue() || (value->isUnsetValue() && CSSPropertyMetadata::isInheritedProperty(m_property)))
            return maybeConvertInherit(state, conversionCheckers);
        return maybeConvertValue(*value, state, conversionCheckers);
    }

    virtual PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertUnderlyingValue(const StyleResolverState&) const = 0;

    virtual void apply(const InterpolableValue&, const NonInterpolableValue*, StyleResolverState&) const = 0;

    // Implement reference equality checking via pointer equality checking as these are singletons
    bool operator==(const InterpolationType& other) const { return this == &other; }
    bool operator!=(const InterpolationType& other) const { return this != &other; }

protected:
    InterpolationType(CSSPropertyID property)
        : m_property(property)
    { }

    virtual PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertNeutral() const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertInitial() const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState* state, ConversionCheckers& conversionCheckers) const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertValue(const CSSValue& value, const StyleResolverState* state, ConversionCheckers& conversionCheckers) const { ASSERT_NOT_REACHED(); return nullptr; }

    const CSSPropertyID m_property;
};

} // namespace blink

#endif // InterpolationType_h
