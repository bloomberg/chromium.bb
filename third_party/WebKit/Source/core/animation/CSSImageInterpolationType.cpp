// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSImageInterpolationType.h"

#include "core/animation/ImagePropertyFunctions.h"
#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/StyleImage.h"

namespace blink {

class CSSImageNonInterpolableValue : public NonInterpolableValue {
public:
    ~CSSImageNonInterpolableValue() final { }

    static PassRefPtr<CSSImageNonInterpolableValue> create(PassRefPtrWillBeRawPtr<CSSValue> start, PassRefPtrWillBeRawPtr<CSSValue> end)
    {
        return adoptRef(new CSSImageNonInterpolableValue(start, end));
    }

    bool isSingle() const { return m_isSingle; }
    bool equals(const CSSImageNonInterpolableValue& other) const
    {
        return m_start->equals(*other.m_start) && m_end->equals(*other.m_end);
    }

    static PassRefPtr<CSSImageNonInterpolableValue> merge(PassRefPtr<NonInterpolableValue> start, PassRefPtr<NonInterpolableValue> end);

    PassRefPtrWillBeRawPtr<CSSValue> crossfade(double progress) const
    {
        if (m_isSingle || progress <= 0)
            return m_start;
        if (progress >= 1)
            return m_end;
        return CSSCrossfadeValue::create(m_start, m_end, CSSPrimitiveValue::create(progress, CSSPrimitiveValue::UnitType::Number));
    }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    CSSImageNonInterpolableValue(PassRefPtrWillBeRawPtr<CSSValue> start, PassRefPtrWillBeRawPtr<CSSValue> end)
        : m_start(start)
        , m_end(end)
        , m_isSingle(m_start == m_end)
    {
        ASSERT(m_start);
        ASSERT(m_end);
    }

    RefPtrWillBePersistent<CSSValue> m_start;
    RefPtrWillBePersistent<CSSValue> m_end;
    const bool m_isSingle;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSImageNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSImageNonInterpolableValue);

PassRefPtr<CSSImageNonInterpolableValue> CSSImageNonInterpolableValue::merge(PassRefPtr<NonInterpolableValue> start, PassRefPtr<NonInterpolableValue> end)
{
    const CSSImageNonInterpolableValue& startImagePair = toCSSImageNonInterpolableValue(*start);
    const CSSImageNonInterpolableValue& endImagePair = toCSSImageNonInterpolableValue(*end);
    ASSERT(startImagePair.m_isSingle);
    ASSERT(endImagePair.m_isSingle);
    return create(startImagePair.m_start, endImagePair.m_end);
}

InterpolationComponent CSSImageInterpolationType::maybeConvertStyleImage(const StyleImage& styleImage, bool acceptGradients)
{
    return maybeConvertCSSValue(*styleImage.cssValue(), acceptGradients);
}

InterpolationComponent CSSImageInterpolationType::maybeConvertCSSValue(const CSSValue& value, bool acceptGradients)
{
    if (value.isImageValue() || (value.isGradientValue() && acceptGradients)) {
        CSSValue* refableCSSValue = const_cast<CSSValue*>(&value);
        return InterpolationComponent(InterpolableNumber::create(1), CSSImageNonInterpolableValue::create(refableCSSValue, refableCSSValue));
    }
    return nullptr;
}

PairwiseInterpolationComponent CSSImageInterpolationType::mergeSingleConversionComponents(InterpolationComponent& startValue, InterpolationComponent& endValue)
{
    if (!toCSSImageNonInterpolableValue(*startValue.nonInterpolableValue).isSingle()
        || !toCSSImageNonInterpolableValue(*endValue.nonInterpolableValue).isSingle()) {
        return nullptr;
    }
    return PairwiseInterpolationComponent(
        InterpolableNumber::create(0),
        InterpolableNumber::create(1),
        CSSImageNonInterpolableValue::merge(startValue.nonInterpolableValue, endValue.nonInterpolableValue));
}

PassRefPtrWillBeRawPtr<CSSValue> CSSImageInterpolationType::createCSSValue(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue)
{
    return toCSSImageNonInterpolableValue(nonInterpolableValue)->crossfade(toInterpolableNumber(interpolableValue).value());
}

PassRefPtrWillBeRawPtr<StyleImage> CSSImageInterpolationType::resolveStyleImage(CSSPropertyID property, const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, StyleResolverState& state)
{
    RefPtrWillBeRawPtr<CSSValue> image = createCSSValue(interpolableValue, nonInterpolableValue);
    return state.styleImage(property, *image);
}

bool CSSImageInterpolationType::equalNonInterpolableValues(const NonInterpolableValue* a, const NonInterpolableValue* b)
{
    return toCSSImageNonInterpolableValue(*a).equals(toCSSImageNonInterpolableValue(*b));
}

class UnderlyingImageChecker : public InterpolationType::ConversionChecker {
public:
    ~UnderlyingImageChecker() final {}

    static PassOwnPtr<UnderlyingImageChecker> create(const InterpolationType& type, PassOwnPtr<InterpolationValue> underlyingValue)
    {
        return adoptPtr(new UnderlyingImageChecker(type, std::move(underlyingValue)));
    }

private:
    UnderlyingImageChecker(const InterpolationType& type, PassOwnPtr<InterpolationValue> underlyingValue)
        : ConversionChecker(type)
        , m_underlyingValue(std::move(underlyingValue))
    { }

    bool isValid(const InterpolationEnvironment&, const UnderlyingValue& underlyingValue) const final
    {
        if (!underlyingValue && !m_underlyingValue)
            return true;
        if (!underlyingValue || !m_underlyingValue)
            return false;
        return m_underlyingValue->interpolableValue().equals(underlyingValue->interpolableValue())
            && CSSImageInterpolationType::equalNonInterpolableValues(m_underlyingValue->nonInterpolableValue(), underlyingValue->nonInterpolableValue());
    }

    OwnPtr<InterpolationValue> m_underlyingValue;
};

PassOwnPtr<InterpolationValue> CSSImageInterpolationType::maybeConvertNeutral(const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
{
    if (!underlyingValue) {
        conversionCheckers.append(UnderlyingImageChecker::create(*this, nullptr));
        return nullptr;
    }
    conversionCheckers.append(UnderlyingImageChecker::create(*this, underlyingValue->clone()));
    return underlyingValue->clone();
}

PassOwnPtr<InterpolationValue> CSSImageInterpolationType::maybeConvertInitial() const
{
    InterpolationComponent component = maybeConvertStyleImage(ImagePropertyFunctions::getInitialStyleImage(cssProperty()), true);
    return component ? InterpolationValue::create(*this, component) : nullptr;
}

class ParentImageChecker : public InterpolationType::ConversionChecker {
public:
    ~ParentImageChecker() final {}

    static PassOwnPtr<ParentImageChecker> create(const InterpolationType& type, CSSPropertyID property, PassRefPtrWillBeRawPtr<StyleImage> inheritedImage)
    {
        return adoptPtr(new ParentImageChecker(type, property, inheritedImage));
    }

private:
    ParentImageChecker(const InterpolationType& type, CSSPropertyID property, PassRefPtrWillBeRawPtr<StyleImage> inheritedImage)
        : ConversionChecker(type)
        , m_property(property)
        , m_inheritedImage(inheritedImage)
    { }

    bool isValid(const InterpolationEnvironment& environment, const UnderlyingValue&) const final
    {
        const StyleImage* inheritedImage = ImagePropertyFunctions::getStyleImage(m_property, *environment.state().parentStyle());
        if (!m_inheritedImage && !inheritedImage)
            return true;
        if (!m_inheritedImage || !inheritedImage)
            return false;
        return *m_inheritedImage == *inheritedImage;
    }

    CSSPropertyID m_property;
    RefPtrWillBePersistent<StyleImage> m_inheritedImage;
};

PassOwnPtr<InterpolationValue> CSSImageInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    if (!state.parentStyle())
        return nullptr;

    const StyleImage* inheritedImage = ImagePropertyFunctions::getStyleImage(cssProperty(), *state.parentStyle());
    StyleImage* refableImage = const_cast<StyleImage*>(inheritedImage);
    conversionCheckers.append(ParentImageChecker::create(*this, cssProperty(), refableImage));
    InterpolationComponent component = maybeConvertStyleImage(inheritedImage, true);
    return component ? InterpolationValue::create(*this, component) : nullptr;
}

PassOwnPtr<InterpolationValue> CSSImageInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    InterpolationComponent component = maybeConvertCSSValue(value, true);
    return component ? InterpolationValue::create(*this, component) : nullptr;
}

PassOwnPtr<PairwisePrimitiveInterpolation> CSSImageInterpolationType::mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
{
    PairwiseInterpolationComponent pairwiseComponent = mergeSingleConversionComponents(startValue.mutableComponent(), endValue.mutableComponent());
    return pairwiseComponent ? PairwisePrimitiveInterpolation::create(*this, pairwiseComponent) : nullptr;
}

PassOwnPtr<InterpolationValue> CSSImageInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    InterpolationComponent component = maybeConvertStyleImage(ImagePropertyFunctions::getStyleImage(cssProperty(), *environment.state().style()), true);
    return component ? InterpolationValue::create(*this, component) : nullptr;
}

void CSSImageInterpolationType::composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
{
    underlyingValue.set(&value);
}

void CSSImageInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    ImagePropertyFunctions::setStyleImage(cssProperty(), *environment.state().style(), resolveStyleImage(cssProperty(), interpolableValue, nonInterpolableValue, environment.state()));
}

} // namespace blink
