// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/StringKeyframe.h"

#include "core/animation/ColorStyleInterpolation.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/ConstantStyleInterpolation.h"
#include "core/animation/DeferredLegacyStyleInterpolation.h"
#include "core/animation/DoubleStyleInterpolation.h"
#include "core/animation/ImageSliceStyleInterpolation.h"
#include "core/animation/ImageStyleInterpolation.h"
#include "core/animation/LegacyStyleInterpolation.h"
#include "core/animation/LengthBoxStyleInterpolation.h"
#include "core/animation/LengthPairStyleInterpolation.h"
#include "core/animation/LengthStyleInterpolation.h"
#include "core/animation/ListStyleInterpolation.h"
#include "core/animation/SVGStrokeDasharrayStyleInterpolation.h"
#include "core/animation/ShadowStyleInterpolation.h"
#include "core/animation/VisibilityStyleInterpolation.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/layout/style/LayoutStyle.h"

namespace blink {

StringKeyframe::StringKeyframe(const StringKeyframe& copyFrom)
    : Keyframe(copyFrom.m_offset, copyFrom.m_composite, copyFrom.m_easing)
    , m_propertySet(copyFrom.m_propertySet->mutableCopy())
{
}

void StringKeyframe::setPropertyValue(CSSPropertyID property, const String& value, StyleSheetContents* styleSheetContents)
{
    ASSERT(property != CSSPropertyInvalid);
    if (CSSAnimations::isAllowedAnimation(property))
        m_propertySet->setProperty(property, value, false, styleSheetContents);
}

void StringKeyframe::setPropertyValue(CSSPropertyID property, PassRefPtrWillBeRawPtr<CSSValue> value)
{
    ASSERT(property != CSSPropertyInvalid);
    ASSERT(CSSAnimations::isAllowedAnimation(property));
    m_propertySet->setProperty(property, value, false);
}

PropertySet StringKeyframe::properties() const
{
    // This is not used in time-critical code, so we probably don't need to
    // worry about caching this result.
    PropertySet properties;
    for (unsigned i = 0; i < m_propertySet->propertyCount(); ++i)
        properties.add(m_propertySet->propertyAt(i).id());
    return properties;
}

PassRefPtrWillBeRawPtr<Keyframe> StringKeyframe::clone() const
{
    return adoptRefWillBeNoop(new StringKeyframe(*this));
}
PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::createPropertySpecificKeyframe(CSSPropertyID property) const
{
    return adoptPtrWillBeNoop(new PropertySpecificKeyframe(offset(), &easing(), propertyValue(property), composite()));
}

DEFINE_TRACE(StringKeyframe)
{
    visitor->trace(m_propertySet);
    Keyframe::trace(visitor);
}

StringKeyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, CSSValue* value, AnimationEffect::CompositeOperation op)
    : Keyframe::PropertySpecificKeyframe(offset, easing, op)
    , m_value(value)
{ }

StringKeyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, CSSValue* value)
    : Keyframe::PropertySpecificKeyframe(offset, easing, AnimationEffect::CompositeReplace)
    , m_value(value)
{
    ASSERT(!isNull(m_offset));
}

void StringKeyframe::PropertySpecificKeyframe::ensureAnimatableValue(CSSPropertyID property, Element& element, const LayoutStyle* baseStyle) const
{
    if (!m_animatableValueCache)
        m_animatableValueCache = StyleResolver::createAnimatableValueSnapshot(element, baseStyle, property, m_value.get());
}

namespace {
InterpolationRange setRange(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyOrphans:
    case CSSPropertyWebkitColumnCount:
    case CSSPropertyWidows:
        return RangeRoundGreaterThanOrEqualToOne;
    case CSSPropertyWebkitColumnRuleWidth:
    case CSSPropertyZIndex:
        return RangeRound;
    case CSSPropertyFloodOpacity:
    case CSSPropertyStopOpacity:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyShapeImageThreshold:
        return RangeZeroToOne;
    case CSSPropertyFillOpacity:
    case CSSPropertyOpacity:
        return RangeOpacityFIXME;
    case CSSPropertyStrokeMiterlimit:
        return RangeGreaterThanOrEqualToOne;
    case CSSPropertyFontSizeAdjust:
        return RangeNonNegative;
    default:
        ASSERT_NOT_REACHED();
        return RangeAll;
    }
}

} // namespace

// FIXME: Refactor this into a generic piece that lives in InterpolationEffect, and a template parameter specific converter.
PassRefPtrWillBeRawPtr<Interpolation> StringKeyframe::PropertySpecificKeyframe::maybeCreateInterpolation(CSSPropertyID property, Keyframe::PropertySpecificKeyframe& end, Element* element, const LayoutStyle* baseStyle) const
{
    CSSValue* fromCSSValue = m_value.get();
    CSSValue* toCSSValue = toStringPropertySpecificKeyframe(end).value();
    InterpolationRange range = RangeAll;
    bool fallBackToLegacy = false;

    // FIXME: Remove this flag once we can rely on legacy's behaviour being correct.
    bool forceDefaultInterpolation = false;

    // FIXME: Remove this check once neutral keyframes are implemented in StringKeyframes.
    if (!fromCSSValue || !toCSSValue)
        return DeferredLegacyStyleInterpolation::create(fromCSSValue, toCSSValue, property);

    ASSERT(fromCSSValue && toCSSValue);

    if (!CSSPropertyMetadata::isAnimatableProperty(property)) {
        if (fromCSSValue == toCSSValue)
            return ConstantStyleInterpolation::create(fromCSSValue, property);

        return nullptr;
    }

    // FIXME: Generate this giant switch statement.
    switch (property) {
    case CSSPropertyLineHeight:
        if (LengthStyleInterpolation::canCreateFrom(*fromCSSValue) && LengthStyleInterpolation::canCreateFrom(*toCSSValue))
            return LengthStyleInterpolation::create(*fromCSSValue, *toCSSValue, property, RangeNonNegative);

        if (DoubleStyleInterpolation::canCreateFrom(*fromCSSValue) && DoubleStyleInterpolation::canCreateFrom(*toCSSValue))
            return DoubleStyleInterpolation::create(*fromCSSValue, *toCSSValue, property, CSSPrimitiveValue::CSS_NUMBER, RangeNonNegative);

        break;
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyFlexBasis:
    case CSSPropertyFontSize:
    case CSSPropertyHeight:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyOutlineWidth:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyPerspective:
    case CSSPropertyR:
    case CSSPropertyRx:
    case CSSPropertyRy:
    case CSSPropertyShapeMargin:
    case CSSPropertyStrokeWidth:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyWebkitColumnGap:
    case CSSPropertyWebkitColumnWidth:
    case CSSPropertyWidth:
        range = RangeNonNegative;
        // Fall through
    case CSSPropertyBaselineShift:
    case CSSPropertyBottom:
    case CSSPropertyCx:
    case CSSPropertyCy:
    case CSSPropertyLeft:
    case CSSPropertyLetterSpacing:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyMotionOffset:
    case CSSPropertyOutlineOffset:
    case CSSPropertyRight:
    case CSSPropertyStrokeDashoffset:
    case CSSPropertyTop:
    case CSSPropertyVerticalAlign:
    case CSSPropertyWordSpacing:
    case CSSPropertyWebkitColumnRuleWidth:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyX:
    case CSSPropertyY:
        if (LengthStyleInterpolation::canCreateFrom(*fromCSSValue, property) && LengthStyleInterpolation::canCreateFrom(*toCSSValue, property))
            return LengthStyleInterpolation::create(*fromCSSValue, *toCSSValue, property, range);

        // FIXME: Handle keywords e.g. 'smaller', 'larger'.
        if (property == CSSPropertyFontSize)
            fallBackToLegacy = true;

        // FIXME: Handle keywords e.g. 'baseline', 'sub'.
        if (property == CSSPropertyBaselineShift)
            fallBackToLegacy = true;

        break;
    case CSSPropertyFontSizeAdjust:
        // FIXME: Requires special handing for 0.
        fallBackToLegacy = true;
        break;
    case CSSPropertyOrphans:
    case CSSPropertyWidows:
    case CSSPropertyZIndex:
    case CSSPropertyWebkitColumnCount:
    case CSSPropertyShapeImageThreshold:
    case CSSPropertyFillOpacity:
    case CSSPropertyFloodOpacity:
    case CSSPropertyOpacity:
    case CSSPropertyStopOpacity:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyStrokeMiterlimit:
        if (DoubleStyleInterpolation::canCreateFrom(*fromCSSValue) && DoubleStyleInterpolation::canCreateFrom(*toCSSValue))
            return DoubleStyleInterpolation::create(*fromCSSValue, *toCSSValue, property, toCSSPrimitiveValue(fromCSSValue)->primitiveType(), setRange(property));
        break;

    case CSSPropertyMotionRotation: {
        RefPtrWillBeRawPtr<Interpolation> interpolation = DoubleStyleInterpolation::maybeCreateFromMotionRotation(*fromCSSValue, *toCSSValue, property);
        if (interpolation)
            return interpolation.release();
            break;
        }
    case CSSPropertyVisibility:
        if (VisibilityStyleInterpolation::canCreateFrom(*fromCSSValue) && VisibilityStyleInterpolation::canCreateFrom(*toCSSValue) && (VisibilityStyleInterpolation::isVisible(*fromCSSValue) || VisibilityStyleInterpolation::isVisible(*toCSSValue)))
            return VisibilityStyleInterpolation::create(*fromCSSValue, *toCSSValue, property);

        break;

    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyColor:
    case CSSPropertyFill:
    case CSSPropertyFloodColor:
    case CSSPropertyLightingColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyStopColor:
    case CSSPropertyStroke:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextStrokeColor:
        {
            RefPtrWillBeRawPtr<Interpolation> interpolation = ColorStyleInterpolation::maybeCreateFromColor(*fromCSSValue, *toCSSValue, property);
            if (interpolation)
                return interpolation.release();

            // Current color should use LegacyStyleInterpolation
            if (ColorStyleInterpolation::shouldUseLegacyStyleInterpolation(*fromCSSValue, *toCSSValue))
                fallBackToLegacy = true;

            break;
        }

    case CSSPropertyBorderImageSource:
    case CSSPropertyListStyleImage:
    case CSSPropertyWebkitMaskBoxImageSource:
        if (fromCSSValue == toCSSValue)
            return ConstantStyleInterpolation::create(fromCSSValue, property);

        if (ImageStyleInterpolation::canCreateFrom(*fromCSSValue) && ImageStyleInterpolation::canCreateFrom(*toCSSValue))
            return ImageStyleInterpolation::create(*fromCSSValue, *toCSSValue, property);

        forceDefaultInterpolation = true;
        break;
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
        range = RangeNonNegative;
        // Fall through
    case CSSPropertyObjectPosition:
        if (LengthPairStyleInterpolation::canCreateFrom(*fromCSSValue) && LengthPairStyleInterpolation::canCreateFrom(*toCSSValue))
            return LengthPairStyleInterpolation::create(*fromCSSValue, *toCSSValue, property, range);
        break;

    case CSSPropertyPerspectiveOrigin:
    case CSSPropertyTransformOrigin: {
        RefPtrWillBeRawPtr<Interpolation> interpolation = ListStyleInterpolation<LengthStyleInterpolation>::maybeCreateFromList(*fromCSSValue, *toCSSValue, property, range);
        if (interpolation)
            return interpolation.release();
        break;
    }

    case CSSPropertyBoxShadow:
    case CSSPropertyTextShadow:
    case CSSPropertyWebkitBoxShadow: {
        RefPtrWillBeRawPtr<Interpolation> interpolation = ListStyleInterpolation<ShadowStyleInterpolation>::maybeCreateFromList(*fromCSSValue, *toCSSValue, property);
        if (interpolation)
            return interpolation.release();

        // FIXME: AnimatableShadow incorrectly animates between inset and non-inset values so it will never indicate it needs default interpolation
        if (ShadowStyleInterpolation::usesDefaultStyleInterpolation(*fromCSSValue, *toCSSValue)) {
            forceDefaultInterpolation = true;
            break;
        }

        // FIXME: Handle interpolation from/to none, unspecified color values
        fallBackToLegacy = true;

        break;

    }

    case CSSPropertyClip: {
        if (LengthBoxStyleInterpolation::usesDefaultInterpolation(*fromCSSValue, *toCSSValue)) {
            forceDefaultInterpolation = true;
            break;
        }
        RefPtrWillBeRawPtr<Interpolation> interpolation = LengthBoxStyleInterpolation::maybeCreateFrom(*fromCSSValue, *toCSSValue, property);
        if (interpolation)
            return interpolation.release();
        break;
    }

    case CSSPropertyBorderImageSlice:
    case CSSPropertyWebkitMaskBoxImageSlice: {
        RefPtrWillBeRawPtr<Interpolation> interpolation = ImageSliceStyleInterpolation::maybeCreate(*fromCSSValue, *toCSSValue, property);
        if (interpolation)
            return interpolation.release();
        if (ImageSliceStyleInterpolation::usesDefaultInterpolation(*fromCSSValue, *toCSSValue))
            forceDefaultInterpolation = true;

        break;
    }

    case CSSPropertyStrokeDasharray: {
        RefPtrWillBeRawPtr<Interpolation> interpolation = SVGStrokeDasharrayStyleInterpolation::maybeCreate(*fromCSSValue, *toCSSValue, property);
        if (interpolation)
            return interpolation.release();

        break;
    }

    default:
        // Fall back to LegacyStyleInterpolation.
        fallBackToLegacy = true;
        break;
    }

    if (fromCSSValue == toCSSValue)
        return ConstantStyleInterpolation::create(fromCSSValue, property);

    if (forceDefaultInterpolation)
        return nullptr;

    if (fromCSSValue->isUnsetValue() || fromCSSValue->isInheritedValue() || fromCSSValue->isInitialValue()
        || toCSSValue->isUnsetValue() || toCSSValue->isInheritedValue() || toCSSValue->isInitialValue())
        fallBackToLegacy = true;

    if (fallBackToLegacy) {
        if (DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(*fromCSSValue) || DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(*toCSSValue)) {
            // FIXME: Handle these cases outside of DeferredLegacyStyleInterpolation.
            return DeferredLegacyStyleInterpolation::create(fromCSSValue, toCSSValue, property);
        }

        // FIXME: Remove the use of AnimatableValues and Elements here.
        ASSERT(element);
        ensureAnimatableValue(property, *element, baseStyle);
        end.ensureAnimatableValue(property, *element, baseStyle);
        return LegacyStyleInterpolation::create(getAnimatableValue(), end.getAnimatableValue(), property);
    }

    ASSERT(AnimatableValue::usesDefaultInterpolation(
        StyleResolver::createAnimatableValueSnapshot(*element, baseStyle, property, fromCSSValue).get(),
        StyleResolver::createAnimatableValueSnapshot(*element, baseStyle, property, toCSSValue).get()));

    return nullptr;

}

PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::PropertySpecificKeyframe::neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const
{
    return adoptPtrWillBeNoop(new PropertySpecificKeyframe(offset, easing, 0, AnimationEffect::CompositeAdd));
}

PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::PropertySpecificKeyframe::cloneWithOffset(double offset) const
{
    Keyframe::PropertySpecificKeyframe* theClone = new PropertySpecificKeyframe(offset, m_easing, m_value.get());
    toStringPropertySpecificKeyframe(theClone)->m_animatableValueCache = m_animatableValueCache;
    return adoptPtrWillBeNoop(theClone);
}

DEFINE_TRACE(StringKeyframe::PropertySpecificKeyframe)
{
    visitor->trace(m_value);
    visitor->trace(m_animatableValueCache);
    Keyframe::PropertySpecificKeyframe::trace(visitor);
}

}
