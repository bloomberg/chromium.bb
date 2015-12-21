// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/ImageSliceStyleInterpolation.h"

#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/resolver/StyleBuilder.h"

namespace blink {

bool ImageSliceStyleInterpolation::usesDefaultInterpolation(const CSSValue& start, const CSSValue& end)
{
    if (!start.isBorderImageSliceValue() || !end.isBorderImageSliceValue())
        return true;
    const CSSBorderImageSliceValue& startSlice = toCSSBorderImageSliceValue(start);
    const CSSBorderImageSliceValue& endSlice = toCSSBorderImageSliceValue(end);
    return startSlice.slices()->top()->isPercentage() != endSlice.slices()->top()->isPercentage()
        || startSlice.m_fill != endSlice.m_fill;
}

namespace {

class Decomposition {
    STACK_ALLOCATED();
public:
    Decomposition(const CSSBorderImageSliceValue& value)
    {
        decompose(value);
    }

    OwnPtr<InterpolableValue> interpolableValue;
    ImageSliceStyleInterpolation::Metadata metadata;

private:
    void decompose(const CSSBorderImageSliceValue& value)
    {
        const size_t kQuadSides = 4;
        OwnPtr<InterpolableList> interpolableList = InterpolableList::create(kQuadSides);
        const CSSQuadValue& quad = *value.slices();
        interpolableList->set(0, InterpolableNumber::create(quad.top()->getDoubleValue()));
        interpolableList->set(1, InterpolableNumber::create(quad.right()->getDoubleValue()));
        interpolableList->set(2, InterpolableNumber::create(quad.bottom()->getDoubleValue()));
        interpolableList->set(3, InterpolableNumber::create(quad.left()->getDoubleValue()));
        bool isPercentage = quad.top()->isPercentage();
        ASSERT(quad.bottom()->isPercentage() == isPercentage
            && quad.left()->isPercentage() == isPercentage
            && quad.right()->isPercentage() == isPercentage);

        interpolableValue = interpolableList.release();
        metadata = ImageSliceStyleInterpolation::Metadata {isPercentage, value.m_fill};
    }
};

PassRefPtrWillBeRawPtr<CSSBorderImageSliceValue> compose(const InterpolableValue& value, const ImageSliceStyleInterpolation::Metadata& metadata)
{
    const InterpolableList& interpolableList = toInterpolableList(value);
    CSSPrimitiveValue::UnitType type = metadata.isPercentage ? CSSPrimitiveValue::UnitType::Percentage : CSSPrimitiveValue::UnitType::Number;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> top = CSSPrimitiveValue::create(clampTo<double>(toInterpolableNumber(interpolableList.get(0))->value(), 0), type);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> right = CSSPrimitiveValue::create(clampTo<double>(toInterpolableNumber(interpolableList.get(1))->value(), 0), type);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> bottom = CSSPrimitiveValue::create(clampTo<double>(toInterpolableNumber(interpolableList.get(2))->value(), 0), type);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> left = CSSPrimitiveValue::create(clampTo<double>(toInterpolableNumber(interpolableList.get(3))->value(), 0), type);
    return CSSBorderImageSliceValue::create(CSSQuadValue::create(top.release(), right.release(), bottom.release(), left.release(), CSSQuadValue::SerializeAsQuad), metadata.fill);
}

} // namespace

PassRefPtr<ImageSliceStyleInterpolation> ImageSliceStyleInterpolation::maybeCreate(const CSSValue& start, const CSSValue& end, CSSPropertyID property)
{
    if (!start.isBorderImageSliceValue() || !end.isBorderImageSliceValue())
        return nullptr;

    Decomposition startDecompose(toCSSBorderImageSliceValue(start));
    Decomposition endDecompose(toCSSBorderImageSliceValue(end));
    if (!(startDecompose.metadata == endDecompose.metadata))
        return nullptr;

    return adoptRef(new ImageSliceStyleInterpolation(
        startDecompose.interpolableValue.release(),
        endDecompose.interpolableValue.release(),
        property,
        startDecompose.metadata
    ));
}

void ImageSliceStyleInterpolation::apply(StyleResolverState& state) const
{
    StyleBuilder::applyProperty(m_id, state, compose(*m_cachedValue, m_metadata).get());
}

} // namespace blink
