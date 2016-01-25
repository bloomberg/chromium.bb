// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/DeferredLegacyStyleInterpolation.h"

#include "core/animation/ElementAnimations.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSBasicShapeValues.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

void DeferredLegacyStyleInterpolation::apply(StyleResolverState& state) const
{
    if (m_outdated || !state.element()->elementAnimations() || !state.element()->elementAnimations()->isAnimationStyleChange()) {
        RefPtr<AnimatableValue> startAnimatableValue;
        RefPtr<AnimatableValue> endAnimatableValue;

        // Snapshot underlying values for neutral keyframes first because non-neutral keyframes will mutate the StyleResolverState.
        if (!m_endCSSValue) {
            endAnimatableValue = StyleResolver::createAnimatableValueSnapshot(state, m_id, m_endCSSValue.get());
            startAnimatableValue = StyleResolver::createAnimatableValueSnapshot(state, m_id, m_startCSSValue.get());
        } else {
            startAnimatableValue = StyleResolver::createAnimatableValueSnapshot(state, m_id, m_startCSSValue.get());
            endAnimatableValue = StyleResolver::createAnimatableValueSnapshot(state, m_id, m_endCSSValue.get());
        }

        m_innerInterpolation = LegacyStyleInterpolation::create(startAnimatableValue, endAnimatableValue, m_id);
        m_outdated = false;
    }

    m_innerInterpolation->interpolate(m_cachedIteration, m_cachedFraction);
    m_innerInterpolation->apply(state);
}

bool DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(const CSSValue& value)
{
    // FIXME: should not require resolving styles for inherit/initial/unset.
    if (value.isCSSWideKeyword())
        return true;
    if (value.isBasicShapeCircleValue())
        return interpolationRequiresStyleResolve(toCSSBasicShapeCircleValue(value));
    if (value.isBasicShapeEllipseValue())
        return interpolationRequiresStyleResolve(toCSSBasicShapeEllipseValue(value));
    if (value.isBasicShapePolygonValue())
        return interpolationRequiresStyleResolve(toCSSBasicShapePolygonValue(value));
    if (value.isBasicShapeInsetValue())
        return interpolationRequiresStyleResolve(toCSSBasicShapeInsetValue(value));
    if (value.isColorValue())
        return false;
    if (value.isStringValue() || value.isURIValue() || value.isCustomIdentValue())
        return false;
    if (value.isPrimitiveValue())
        return interpolationRequiresStyleResolve(toCSSPrimitiveValue(value));
    if (value.isQuadValue())
        return interpolationRequiresStyleResolve(toCSSQuadValue(value));
    if (value.isValueList())
        return interpolationRequiresStyleResolve(toCSSValueList(value));
    if (value.isValuePair())
        return interpolationRequiresStyleResolve(toCSSValuePair(value));
    if (value.isImageValue())
        return interpolationRequiresStyleResolve(toCSSImageValue(value));
    if (value.isShadowValue())
        return interpolationRequiresStyleResolve(toCSSShadowValue(value));
    if (value.isSVGDocumentValue())
        return interpolationRequiresStyleResolve(toCSSSVGDocumentValue(value));
    // FIXME: consider other custom types.
    return true;
}

bool DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(const CSSPrimitiveValue& primitiveValue)
{
    // FIXME: consider other types.
    if (primitiveValue.isNumber() || primitiveValue.isPercentage() || primitiveValue.isAngle())
        return false;

    if (primitiveValue.isLength())
        return primitiveValue.isFontRelativeLength() || primitiveValue.isViewportPercentageLength();

    if (primitiveValue.isCalculated()) {
        CSSLengthArray lengthArray(CSSPrimitiveValue::LengthUnitTypeCount);
        primitiveValue.accumulateLengthArray(lengthArray);
        return lengthArray[CSSPrimitiveValue::UnitTypeFontSize] != 0
            || lengthArray[CSSPrimitiveValue::UnitTypeFontXSize] != 0
            || lengthArray[CSSPrimitiveValue::UnitTypeRootFontSize] != 0
            || lengthArray[CSSPrimitiveValue::UnitTypeZeroCharacterWidth] != 0
            || lengthArray[CSSPrimitiveValue::UnitTypeViewportWidth] != 0
            || lengthArray[CSSPrimitiveValue::UnitTypeViewportHeight] != 0
            || lengthArray[CSSPrimitiveValue::UnitTypeViewportMin] != 0
            || lengthArray[CSSPrimitiveValue::UnitTypeViewportMax] != 0;
    }

    CSSValueID id = primitiveValue.getValueID();
    bool isColor = ((id >= CSSValueAqua && id <= CSSValueTransparent)
        || (id >= CSSValueAliceblue && id <= CSSValueYellowgreen)
        || id == CSSValueGrey);
    return (id != CSSValueNone) && !isColor;
}

bool DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(const CSSImageValue& imageValue)
{
    return false;
}

bool DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(const CSSShadowValue& shadowValue)
{
    return (shadowValue.x && interpolationRequiresStyleResolve(*shadowValue.x))
        || (shadowValue.y && interpolationRequiresStyleResolve(*shadowValue.y))
        || (shadowValue.blur && interpolationRequiresStyleResolve(*shadowValue.blur))
        || (shadowValue.spread && interpolationRequiresStyleResolve(*shadowValue.spread))
        || (shadowValue.style && interpolationRequiresStyleResolve(*shadowValue.style))
        || (shadowValue.color && interpolationRequiresStyleResolve(*shadowValue.color));
}

bool DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(const CSSSVGDocumentValue& documentValue)
{
    return true;
}

bool DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(const CSSValueList& valueList)
{
    size_t length = valueList.length();
    for (size_t index = 0; index < length; ++index) {
        if (interpolationRequiresStyleResolve(*valueList.item(index)))
            return true;
    }
    return false;
}

bool DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(const CSSValuePair& pair)
{
    return interpolationRequiresStyleResolve(pair.first())
        || interpolationRequiresStyleResolve(pair.second());
}

bool DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(const CSSBasicShapeCircleValue& shape)
{
    // FIXME: Should inspect the members.
    return false;
}

bool DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(const CSSBasicShapeEllipseValue& shape)
{
    // FIXME: Should inspect the members.
    return false;
}

bool DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(const CSSBasicShapePolygonValue& shape)
{
    // FIXME: Should inspect the members.
    return false;
}

bool DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(const CSSBasicShapeInsetValue& shape)
{
    // FIXME: Should inspect the members.
    return false;
}

bool DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(const CSSQuadValue& quad)
{
    return interpolationRequiresStyleResolve(*quad.top())
        || interpolationRequiresStyleResolve(*quad.right())
        || interpolationRequiresStyleResolve(*quad.bottom())
        || interpolationRequiresStyleResolve(*quad.left());
}

} // namespace blink
