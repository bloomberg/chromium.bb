// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/ImageInterpolationType.h"

#include "core/animation/ImagePropertyFunctions.h"
#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/StyleImage.h"

namespace blink {

class ImageNonInterpolableValue : public NonInterpolableValue {
public:
    ~ImageNonInterpolableValue() final { }

    static PassRefPtr<ImageNonInterpolableValue> create(PassRefPtrWillBeRawPtr<CSSValue> start, PassRefPtrWillBeRawPtr<CSSValue> end)
    {
        return adoptRef(new ImageNonInterpolableValue(start, end));
    }

    bool isSingle() const { return m_isSingle; }
    bool equals(const ImageNonInterpolableValue& other) const
    {
        return m_start->equals(*other.m_start) && m_end->equals(*other.m_end);
    }

    static PassRefPtr<ImageNonInterpolableValue> merge(PassRefPtr<NonInterpolableValue> start, PassRefPtr<NonInterpolableValue> end);

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
    ImageNonInterpolableValue(PassRefPtrWillBeRawPtr<CSSValue> start, PassRefPtrWillBeRawPtr<CSSValue> end)
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

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(ImageNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(ImageNonInterpolableValue);

PassRefPtr<ImageNonInterpolableValue> ImageNonInterpolableValue::merge(PassRefPtr<NonInterpolableValue> start, PassRefPtr<NonInterpolableValue> end)
{
    const ImageNonInterpolableValue& startImagePair = toImageNonInterpolableValue(*start);
    const ImageNonInterpolableValue& endImagePair = toImageNonInterpolableValue(*end);
    ASSERT(startImagePair.m_isSingle);
    ASSERT(endImagePair.m_isSingle);
    return create(startImagePair.m_start, endImagePair.m_end);
}

InterpolationComponent ImageInterpolationType::maybeConvertStyleImage(const StyleImage& styleImage)
{
    return maybeConvertCSSValue(*styleImage.cssValue());
}

InterpolationComponent ImageInterpolationType::maybeConvertCSSValue(const CSSValue& value)
{
    if (!value.isImageValue() && !value.isGradientValue())
        return nullptr;
    CSSValue* refableCSSValue = const_cast<CSSValue*>(&value);
    return InterpolationComponent(InterpolableNumber::create(1), ImageNonInterpolableValue::create(refableCSSValue, refableCSSValue));
}

PairwiseInterpolationComponent ImageInterpolationType::mergeSingleConversionComponents(InterpolationComponent& startValue, InterpolationComponent& endValue)
{
    if (!toImageNonInterpolableValue(*startValue.nonInterpolableValue).isSingle()
        || !toImageNonInterpolableValue(*endValue.nonInterpolableValue).isSingle()) {
        return nullptr;
    }
    return PairwiseInterpolationComponent(
        InterpolableNumber::create(0),
        InterpolableNumber::create(1),
        ImageNonInterpolableValue::merge(startValue.nonInterpolableValue, endValue.nonInterpolableValue));
}

PassRefPtrWillBeRawPtr<CSSValue> ImageInterpolationType::createCSSValue(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue)
{
    return toImageNonInterpolableValue(nonInterpolableValue)->crossfade(toInterpolableNumber(interpolableValue).value());
}

PassRefPtrWillBeRawPtr<StyleImage> ImageInterpolationType::resolveStyleImage(CSSPropertyID property, const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, StyleResolverState& state)
{
    RefPtrWillBeRawPtr<CSSValue> image = createCSSValue(interpolableValue, nonInterpolableValue);
    return state.styleImage(property, *image);
}

class UnderlyingImageChecker : public InterpolationType::ConversionChecker {
public:
    ~UnderlyingImageChecker() final {}

    static PassOwnPtr<UnderlyingImageChecker> create(const InterpolationType& type, PassOwnPtr<InterpolationValue> underlyingValue)
    {
        return adoptPtr(new UnderlyingImageChecker(type, underlyingValue));
    }

private:
    UnderlyingImageChecker(const InterpolationType& type, PassOwnPtr<InterpolationValue> underlyingValue)
        : ConversionChecker(type)
        , m_underlyingValue(underlyingValue)
    { }

    bool isValid(const StyleResolverState&, const UnderlyingValue& underlyingValue) const final
    {
        if (!underlyingValue && !m_underlyingValue)
            return true;
        if (!underlyingValue || !m_underlyingValue)
            return false;
        return m_underlyingValue->interpolableValue().equals(underlyingValue->interpolableValue())
            && toImageNonInterpolableValue(*m_underlyingValue->nonInterpolableValue()).equals(toImageNonInterpolableValue(*underlyingValue->nonInterpolableValue()));
    }

    OwnPtr<InterpolationValue> m_underlyingValue;
};

PassOwnPtr<InterpolationValue> ImageInterpolationType::maybeConvertNeutral(const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
{
    if (!underlyingValue) {
        conversionCheckers.append(UnderlyingImageChecker::create(*this, nullptr));
        return nullptr;
    }
    conversionCheckers.append(UnderlyingImageChecker::create(*this, underlyingValue->clone()));
    return underlyingValue->clone();
}

PassOwnPtr<InterpolationValue> ImageInterpolationType::maybeConvertInitial() const
{
    InterpolationComponent component = maybeConvertStyleImage(ImagePropertyFunctions::getInitialStyleImage(m_property));
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

    bool isValid(const StyleResolverState& state, const UnderlyingValue&) const final
    {
        const StyleImage* inheritedImage = ImagePropertyFunctions::getStyleImage(m_property, *state.parentStyle());
        if (!m_inheritedImage && !inheritedImage)
            return true;
        if (!m_inheritedImage || !inheritedImage)
            return false;
        return *m_inheritedImage == *inheritedImage;
    }

    CSSPropertyID m_property;
    RefPtrWillBePersistent<StyleImage> m_inheritedImage;
};

PassOwnPtr<InterpolationValue> ImageInterpolationType::maybeConvertInherit(const StyleResolverState* state, ConversionCheckers& conversionCheckers) const
{
    if (!state || !state->parentStyle())
        return nullptr;

    const StyleImage* inheritedImage = ImagePropertyFunctions::getStyleImage(m_property, *state->parentStyle());
    StyleImage* refableImage = const_cast<StyleImage*>(inheritedImage);
    conversionCheckers.append(ParentImageChecker::create(*this, m_property, refableImage));
    InterpolationComponent component = maybeConvertStyleImage(inheritedImage);
    return component ? InterpolationValue::create(*this, component) : nullptr;
}

PassOwnPtr<InterpolationValue> ImageInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState*, ConversionCheckers&) const
{
    InterpolationComponent component = maybeConvertCSSValue(value);
    return component ? InterpolationValue::create(*this, component) : nullptr;
}

PassOwnPtr<PairwisePrimitiveInterpolation> ImageInterpolationType::mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
{
    PairwiseInterpolationComponent pairwiseComponent = mergeSingleConversionComponents(startValue.mutableComponent(), endValue.mutableComponent());
    return pairwiseComponent ? PairwisePrimitiveInterpolation::create(*this, pairwiseComponent) : nullptr;
}

PassOwnPtr<InterpolationValue> ImageInterpolationType::maybeConvertUnderlyingValue(const StyleResolverState& state) const
{
    InterpolationComponent component = maybeConvertStyleImage(ImagePropertyFunctions::getStyleImage(m_property, *state.style()));
    return component ? InterpolationValue::create(*this, component) : nullptr;
}

void ImageInterpolationType::composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
{
    underlyingValue.set(&value);
}

void ImageInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, StyleResolverState& state) const
{
    ImagePropertyFunctions::setStyleImage(m_property, *state.style(), resolveStyleImage(m_property, interpolableValue, nonInterpolableValue, state));
}

} // namespace blink
