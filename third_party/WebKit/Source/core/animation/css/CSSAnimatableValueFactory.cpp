/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"

#include "CSSValueKeywords.h"
#include "core/animation/AnimatableNumber.h"
#include "core/animation/AnimatableUnknown.h"
#include "core/platform/Length.h"
#include "core/rendering/style/RenderStyle.h"


namespace WebCore {

// FIXME: Handle remaining animatable properties (pulled from CSSPropertyAnimation):
// CSSPropertyBackgroundColor
// CSSPropertyBackgroundImage
// CSSPropertyBackgroundPositionX
// CSSPropertyBackgroundPositionY
// CSSPropertyBackgroundSize
// CSSPropertyBaselineShift
// CSSPropertyBorderBottomColor
// CSSPropertyBorderBottomLeftRadius
// CSSPropertyBorderBottomRightRadius
// CSSPropertyBorderBottomWidth
// CSSPropertyBorderImageOutset
// CSSPropertyBorderImageSlice
// CSSPropertyBorderImageSource
// CSSPropertyBorderImageWidth
// CSSPropertyBorderLeftColor
// CSSPropertyBorderLeftWidth
// CSSPropertyBorderRightColor
// CSSPropertyBorderRightWidth
// CSSPropertyBorderTopColor
// CSSPropertyBorderTopLeftRadius
// CSSPropertyBorderTopRightRadius
// CSSPropertyBorderTopWidth
// CSSPropertyBoxShadow
// CSSPropertyClip
// CSSPropertyColor
// CSSPropertyFill
// CSSPropertyFillOpacity
// CSSPropertyFloodColor
// CSSPropertyFloodOpacity
// CSSPropertyFontSize
// CSSPropertyKerning
// CSSPropertyLetterSpacing
// CSSPropertyLightingColor
// CSSPropertyLineHeight
// CSSPropertyListStyleImage
// CSSPropertyOpacity
// CSSPropertyOrphans
// CSSPropertyOutlineColor
// CSSPropertyOutlineOffset
// CSSPropertyOutlineWidth
// CSSPropertyStopColor
// CSSPropertyStopOpacity
// CSSPropertyStroke
// CSSPropertyStrokeDashoffset
// CSSPropertyStrokeMiterlimit
// CSSPropertyStrokeOpacity
// CSSPropertyStrokeWidth
// CSSPropertyTextIndent
// CSSPropertyTextShadow
// CSSPropertyVisibility
// CSSPropertyWebkitBackgroundSize
// CSSPropertyWebkitBorderHorizontalSpacing
// CSSPropertyWebkitBorderVerticalSpacing
// CSSPropertyWebkitBoxShadow
// CSSPropertyWebkitClipPath
// CSSPropertyWebkitColumnCount
// CSSPropertyWebkitColumnGap
// CSSPropertyWebkitColumnRuleColor
// CSSPropertyWebkitColumnRuleWidth
// CSSPropertyWebkitColumnWidth
// CSSPropertyWebkitFilter
// CSSPropertyWebkitMaskBoxImage
// CSSPropertyWebkitMaskBoxImageSource
// CSSPropertyWebkitMaskImage
// CSSPropertyWebkitMaskPositionX
// CSSPropertyWebkitMaskPositionY
// CSSPropertyWebkitMaskSize
// CSSPropertyWebkitPerspective
// CSSPropertyWebkitShapeInside
// CSSPropertyWebkitTextFillColor
// CSSPropertyWebkitTextStrokeColor
// CSSPropertyWebkitTransform
// CSSPropertyWebkitTransformOriginZ
// CSSPropertyWidows
// CSSPropertyWordSpacing
// CSSPropertyZIndex
// CSSPropertyZoom

static PassRefPtr<AnimatableValue> createFromLength(const Length& length, const RenderStyle* style)
{
    switch (length.type()) {
    case Relative:
        return AnimatableNumber::create(length.value(), AnimatableNumber::UnitTypeNumber);
    case Fixed:
        return AnimatableNumber::create(adjustFloatForAbsoluteZoom(length.value(), style), AnimatableNumber::UnitTypeLength);
    case Percent:
        return AnimatableNumber::create(length.value(), AnimatableNumber::UnitTypePercentage);
    case ViewportPercentageWidth:
        return AnimatableNumber::create(length.value(), AnimatableNumber::UnitTypeViewportWidth);
    case ViewportPercentageHeight:
        return AnimatableNumber::create(length.value(), AnimatableNumber::UnitTypeViewportHeight);
    case ViewportPercentageMin:
        return AnimatableNumber::create(length.value(), AnimatableNumber::UnitTypeViewportMin);
    case ViewportPercentageMax:
        return AnimatableNumber::create(length.value(), AnimatableNumber::UnitTypeViewportMax);
    case Calculated:
        // FIXME: Convert platform calcs to CSS calcs.
        ASSERT_WITH_MESSAGE(false, "Web Animations not yet implemented: Convert platform CalculationValue to AnimatableValue");
        return 0;
    case Auto:
    case Intrinsic:
    case MinIntrinsic:
    case MinContent:
    case MaxContent:
    case FillAvailable:
    case FitContent:
        return AnimatableUnknown::create(CSSPrimitiveValue::create(length));
    case Undefined:
        return AnimatableUnknown::create(CSSPrimitiveValue::create(CSSValueNone));
    case ExtendToZoom: // Does not apply to elements.
        ASSERT_NOT_REACHED();
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

inline static PassRefPtr<AnimatableValue> createFromDouble(double value)
{
    return AnimatableNumber::create(value, AnimatableNumber::UnitTypeNumber);
}

// FIXME: Generate this function.
PassRefPtr<AnimatableValue> CSSAnimatableValueFactory::create(CSSPropertyID property, const RenderStyle* style)
{
    switch (property) {
    case CSSPropertyBottom:
        return createFromLength(style->bottom(), style);
    case CSSPropertyHeight:
        return createFromLength(style->height(), style);
    case CSSPropertyLeft:
        return createFromLength(style->left(), style);
    case CSSPropertyMarginBottom:
        return createFromLength(style->marginBottom(), style);
    case CSSPropertyMarginLeft:
        return createFromLength(style->marginLeft(), style);
    case CSSPropertyMarginRight:
        return createFromLength(style->marginRight(), style);
    case CSSPropertyMarginTop:
        return createFromLength(style->marginTop(), style);
    case CSSPropertyMaxHeight:
        return createFromLength(style->maxHeight(), style);
    case CSSPropertyMaxWidth:
        return createFromLength(style->maxWidth(), style);
    case CSSPropertyMinHeight:
        return createFromLength(style->minHeight(), style);
    case CSSPropertyMinWidth:
        return createFromLength(style->minWidth(), style);
    case CSSPropertyOpacity:
        return createFromDouble(style->opacity());
    case CSSPropertyPaddingBottom:
        return createFromLength(style->paddingBottom(), style);
    case CSSPropertyPaddingLeft:
        return createFromLength(style->paddingLeft(), style);
    case CSSPropertyPaddingRight:
        return createFromLength(style->paddingRight(), style);
    case CSSPropertyPaddingTop:
        return createFromLength(style->paddingTop(), style);
    case CSSPropertyRight:
        return createFromLength(style->right(), style);
    case CSSPropertyTop:
        return createFromLength(style->top(), style);
    case CSSPropertyWebkitPerspectiveOriginX:
        return createFromLength(style->perspectiveOriginX(), style);
    case CSSPropertyWebkitPerspectiveOriginY:
        return createFromLength(style->perspectiveOriginY(), style);
    case CSSPropertyWebkitTransformOriginX:
        return createFromLength(style->transformOriginX(), style);
    case CSSPropertyWebkitTransformOriginY:
        return createFromLength(style->transformOriginY(), style);
    case CSSPropertyWidth:
        return createFromLength(style->width(), style);
    default:
        RELEASE_ASSERT_WITH_MESSAGE(false, "Web Animations not yet implemented: Create AnimatableValue from render style");
        return 0;
    }
}

} // namespace WebCore
