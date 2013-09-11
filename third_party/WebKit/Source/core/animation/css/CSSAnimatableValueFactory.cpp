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
#include "core/animation/AnimatableColor.h"
#include "core/animation/AnimatableImage.h"
#include "core/animation/AnimatableLengthBox.h"
#include "core/animation/AnimatableNumber.h"
#include "core/animation/AnimatableTransform.h"
#include "core/animation/AnimatableUnknown.h"
#include "core/animation/AnimatableVisibility.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/platform/Length.h"
#include "core/platform/LengthBox.h"
#include "core/rendering/style/RenderStyle.h"


namespace WebCore {

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

inline static PassRefPtr<AnimatableValue> createFromLengthBox(const LengthBox lengthBox, const RenderStyle* style)
{
    return AnimatableLengthBox::create(
        createFromLength(lengthBox.left(), style),
        createFromLength(lengthBox.right(), style),
        createFromLength(lengthBox.top(), style),
        createFromLength(lengthBox.bottom(), style));
}

PassRefPtr<AnimatableValue> CSSAnimatableValueFactory::createFromColor(CSSPropertyID property, const RenderStyle* style)
{
    Color color = style->colorIncludingFallback(property, false);
    Color visitedLinkColor = style->colorIncludingFallback(property, true);
    Color fallbackColor = style->color();
    Color fallbackVisitedLinkColor = style->visitedLinkColor();
    Color resolvedColor;
    if (color.isValid())
        resolvedColor = color;
    else
        resolvedColor = fallbackColor;
    Color resolvedVisitedLinkColor;
    if (visitedLinkColor.isValid())
        resolvedVisitedLinkColor = visitedLinkColor;
    else
        resolvedVisitedLinkColor = fallbackVisitedLinkColor;
    return AnimatableColor::create(resolvedColor, resolvedVisitedLinkColor);
}

// FIXME: Generate this function.
PassRefPtr<AnimatableValue> CSSAnimatableValueFactory::create(CSSPropertyID property, const RenderStyle* style)
{
    switch (property) {
    case CSSPropertyBackgroundColor:
        return createFromColor(property, style);
    case CSSPropertyBorderBottomColor:
        return createFromColor(property, style);
    case CSSPropertyBorderBottomWidth:
        return createFromDouble(style->borderBottomWidth());
    case CSSPropertyBorderImageOutset:
        return createFromLengthBox(style->borderImageOutset(), style);
    case CSSPropertyBorderImageSlice:
        return createFromLengthBox(style->borderImageSlices(), style);
    case CSSPropertyBorderImageSource:
        return AnimatableImage::create(style->borderImageSource());
    case CSSPropertyBorderImageWidth:
        return createFromLengthBox(style->borderImageWidth(), style);
    case CSSPropertyBorderLeftColor:
        return createFromColor(property, style);
    case CSSPropertyBorderLeftWidth:
        return createFromDouble(style->borderLeftWidth());
    case CSSPropertyBorderRightColor:
        return createFromColor(property, style);
    case CSSPropertyBorderRightWidth:
        return createFromDouble(style->borderRightWidth());
    case CSSPropertyBorderTopColor:
        return createFromColor(property, style);
    case CSSPropertyBorderTopWidth:
        return createFromDouble(style->borderTopWidth());
    case CSSPropertyBottom:
        return createFromLength(style->bottom(), style);
    case CSSPropertyClip:
        return createFromLengthBox(style->clip(), style);
    case CSSPropertyColor:
        return createFromColor(property, style);
    case CSSPropertyHeight:
        return createFromLength(style->height(), style);
    case CSSPropertyListStyleImage:
        return AnimatableImage::create(style->listStyleImage());
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
    case CSSPropertyOutlineColor:
        return createFromColor(property, style);
    case CSSPropertyOutlineOffset:
        return createFromDouble(style->outlineOffset());
    case CSSPropertyOutlineWidth:
        return createFromDouble(style->outlineWidth());
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
    case CSSPropertyTextDecorationColor:
        return createFromColor(property, style);
    case CSSPropertyTop:
        return createFromLength(style->top(), style);
    case CSSPropertyWebkitColumnRuleColor:
        return createFromColor(property, style);
    case CSSPropertyWebkitMaskBoxImageSource:
        return AnimatableImage::create(style->maskBoxImageSource());
    case CSSPropertyWebkitMaskImage:
        return AnimatableImage::create(style->maskImage());
    case CSSPropertyWebkitPerspectiveOriginX:
        return createFromLength(style->perspectiveOriginX(), style);
    case CSSPropertyWebkitPerspectiveOriginY:
        return createFromLength(style->perspectiveOriginY(), style);
    case CSSPropertyWebkitTextEmphasisColor:
        return createFromColor(property, style);
    case CSSPropertyWebkitTextFillColor:
        return createFromColor(property, style);
    case CSSPropertyWebkitTextStrokeColor:
        return createFromColor(property, style);
    case CSSPropertyWebkitTransform:
        return AnimatableTransform::create(style->transform());
    case CSSPropertyWebkitTransformOriginX:
        return createFromLength(style->transformOriginX(), style);
    case CSSPropertyWebkitTransformOriginY:
        return createFromLength(style->transformOriginY(), style);
    case CSSPropertyWidth:
        return createFromLength(style->width(), style);
    case CSSPropertyVisibility:
        return AnimatableVisibility::create(style->visibility());
    default:
        RELEASE_ASSERT_WITH_MESSAGE(!CSSAnimations::isAnimatableProperty(property), "Web Animations not yet implemented: Create AnimatableValue from render style: %s", getPropertyNameString(property).utf8().data());
        ASSERT_NOT_REACHED();
        return 0;
    }
}

} // namespace WebCore
