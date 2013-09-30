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
#include "core/css/resolver/AnimatedStyleBuilder.h"

#include "core/animation/AnimatableColor.h"
#include "core/animation/AnimatableImage.h"
#include "core/animation/AnimatableLengthBox.h"
#include "core/animation/AnimatableLengthSize.h"
#include "core/animation/AnimatableNumber.h"
#include "core/animation/AnimatableTransform.h"
#include "core/animation/AnimatableUnknown.h"
#include "core/animation/AnimatableValue.h"
#include "core/animation/AnimatableVisibility.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/rendering/style/RenderStyle.h"
#include "wtf/MathExtras.h"
#include "wtf/TypeTraits.h"

namespace WebCore {

namespace {

Length animatableValueToLength(const AnimatableValue* value, const StyleResolverState& state, NumberRange range = AllValues)
{
    const RenderStyle* style = state.style();
    if (value->isNumber())
        return toAnimatableNumber(value)->toLength(style, state.rootElementStyle(), style->effectiveZoom(), range);
    RefPtr<CSSValue> cssValue = toAnimatableUnknown(value)->toCSSValue();
    CSSPrimitiveValue* cssPrimitiveValue = toCSSPrimitiveValue(cssValue.get());
    return cssPrimitiveValue->convertToLength<AnyConversion>(style, state.rootElementStyle(), style->effectiveZoom());
}

template<typename T> T animatableValueRoundClampTo(const AnimatableValue* value)
{
    COMPILE_ASSERT(WTF::IsInteger<T>::value, ShouldUseIntegralTypeTWhenRoundingValues);
    return clampTo<T>(round(toAnimatableNumber(value)->toDouble()));
}

LengthBox animatableValueToLengthBox(const AnimatableValue* value, const StyleResolverState& state)
{
    const AnimatableLengthBox* animatableLengthBox = toAnimatableLengthBox(value);
    return LengthBox(
        animatableValueToLength(animatableLengthBox->top(), state),
        animatableValueToLength(animatableLengthBox->right(), state),
        animatableValueToLength(animatableLengthBox->bottom(), state),
        animatableValueToLength(animatableLengthBox->left(), state));
}

LengthSize animatableValueToLengthSize(const AnimatableValue* value, const StyleResolverState& state, NumberRange range)
{
    const AnimatableLengthSize* animatableLengthSize = toAnimatableLengthSize(value);
    return LengthSize(
        animatableValueToLength(animatableLengthSize->width(), state, range),
        animatableValueToLength(animatableLengthSize->height(), state, range));
}

} // namespace

// FIXME: Generate this function.
void AnimatedStyleBuilder::applyProperty(CSSPropertyID property, StyleResolverState& state, const AnimatableValue* value)
{
    if (value->isUnknown()) {
        StyleBuilder::applyProperty(property, state, toAnimatableUnknown(value)->toCSSValue().get());
        return;
    }
    RenderStyle* style = state.style();
    switch (property) {
    case CSSPropertyBackgroundColor:
        style->setBackgroundColor(toAnimatableColor(value)->color());
        style->setVisitedLinkBackgroundColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyBorderBottomColor:
        style->setBorderBottomColor(toAnimatableColor(value)->color());
        style->setVisitedLinkBorderBottomColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyBorderBottomLeftRadius:
        style->setBorderBottomLeftRadius(animatableValueToLengthSize(value, state, NonNegativeValues));
        return;
    case CSSPropertyBorderBottomRightRadius:
        style->setBorderBottomRightRadius(animatableValueToLengthSize(value, state, NonNegativeValues));
        return;
    case CSSPropertyBorderBottomWidth:
        style->setBorderBottomWidth(animatableValueRoundClampTo<unsigned>(value));
        return;
    case CSSPropertyBorderImageOutset:
        style->setBorderImageOutset(animatableValueToLengthBox(value, state));
        return;
    case CSSPropertyBorderImageSlice:
        style->setBorderImageSlices(animatableValueToLengthBox(value, state));
        return;
    case CSSPropertyBorderImageSource:
        style->setBorderImageSource(toAnimatableImage(value)->toStyleImage());
        return;
    case CSSPropertyBorderImageWidth:
        style->setBorderImageWidth(animatableValueToLengthBox(value, state));
        return;
    case CSSPropertyBorderLeftColor:
        style->setBorderLeftColor(toAnimatableColor(value)->color());
        style->setVisitedLinkBorderLeftColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyBorderLeftWidth:
        style->setBorderLeftWidth(animatableValueRoundClampTo<unsigned>(value));
        return;
    case CSSPropertyBorderRightColor:
        style->setBorderRightColor(toAnimatableColor(value)->color());
        style->setVisitedLinkBorderRightColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyBorderRightWidth:
        style->setBorderRightWidth(animatableValueRoundClampTo<unsigned>(value));
        return;
    case CSSPropertyBorderTopColor:
        style->setBorderTopColor(toAnimatableColor(value)->color());
        style->setVisitedLinkBorderTopColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyBorderTopLeftRadius:
        style->setBorderTopLeftRadius(animatableValueToLengthSize(value, state, NonNegativeValues));
        return;
    case CSSPropertyBorderTopRightRadius:
        style->setBorderTopRightRadius(animatableValueToLengthSize(value, state, NonNegativeValues));
        return;
    case CSSPropertyBorderTopWidth:
        style->setBorderTopWidth(animatableValueRoundClampTo<unsigned>(value));
        return;
    case CSSPropertyBottom:
        style->setBottom(animatableValueToLength(value, state));
        return;
    case CSSPropertyClip:
        style->setClip(animatableValueToLengthBox(value, state));
        return;
    case CSSPropertyColor:
        style->setColor(toAnimatableColor(value)->color());
        style->setVisitedLinkColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyHeight:
        style->setHeight(animatableValueToLength(value, state));
        return;
    case CSSPropertyLeft:
        style->setLeft(animatableValueToLength(value, state));
        return;
    case CSSPropertyListStyleImage:
        style->setListStyleImage(toAnimatableImage(value)->toStyleImage());
        return;
    case CSSPropertyMarginBottom:
        style->setMarginBottom(animatableValueToLength(value, state));
        return;
    case CSSPropertyMarginLeft:
        style->setMarginLeft(animatableValueToLength(value, state));
        return;
    case CSSPropertyMarginRight:
        style->setMarginRight(animatableValueToLength(value, state));
        return;
    case CSSPropertyMarginTop:
        style->setMarginTop(animatableValueToLength(value, state));
        return;
    case CSSPropertyMaxHeight:
        style->setMaxHeight(animatableValueToLength(value, state));
        return;
    case CSSPropertyMaxWidth:
        style->setMaxWidth(animatableValueToLength(value, state));
        return;
    case CSSPropertyMinHeight:
        style->setMinHeight(animatableValueToLength(value, state));
        return;
    case CSSPropertyMinWidth:
        style->setMinWidth(animatableValueToLength(value, state));
        return;
    case CSSPropertyOpacity:
        style->setOpacity(toAnimatableNumber(value)->toDouble());
        return;
    case CSSPropertyOutlineColor:
        style->setOutlineColor(toAnimatableColor(value)->color());
        style->setVisitedLinkOutlineColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyOutlineOffset:
        style->setOutlineOffset(animatableValueRoundClampTo<int>(value));
        return;
    case CSSPropertyOutlineWidth:
        style->setOutlineWidth(animatableValueRoundClampTo<unsigned short>(value));
        return;
    case CSSPropertyPaddingBottom:
        style->setPaddingBottom(animatableValueToLength(value, state));
        return;
    case CSSPropertyPaddingLeft:
        style->setPaddingLeft(animatableValueToLength(value, state));
        return;
    case CSSPropertyPaddingRight:
        style->setPaddingRight(animatableValueToLength(value, state));
        return;
    case CSSPropertyPaddingTop:
        style->setPaddingTop(animatableValueToLength(value, state));
        return;
    case CSSPropertyRight:
        style->setRight(animatableValueToLength(value, state));
        return;
    case CSSPropertyTextDecorationColor:
        style->setTextDecorationColor(toAnimatableColor(value)->color());
        style->setVisitedLinkTextDecorationColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyTextIndent:
        style->setTextIndent(animatableValueToLength(value, state));
        return;
    case CSSPropertyTop:
        style->setTop(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitBorderHorizontalSpacing:
        style->setHorizontalBorderSpacing(animatableValueRoundClampTo<unsigned short>(value));
        return;
    case CSSPropertyWebkitBorderVerticalSpacing:
        style->setVerticalBorderSpacing(animatableValueRoundClampTo<unsigned short>(value));
        return;
    case CSSPropertyWebkitColumnRuleColor:
        style->setColumnRuleColor(toAnimatableColor(value)->color());
        style->setVisitedLinkColumnRuleColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyWebkitColumnRuleWidth:
        style->setColumnRuleWidth(animatableValueRoundClampTo<unsigned short>(value));
        return;
    case CSSPropertyWebkitMaskBoxImageSource:
        style->setMaskBoxImageSource(toAnimatableImage(value)->toStyleImage());
        return;
    case CSSPropertyWebkitMaskImage:
        style->setMaskImage(toAnimatableImage(value)->toStyleImage());
        return;
    case CSSPropertyWebkitPerspectiveOriginX:
        style->setPerspectiveOriginX(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitPerspectiveOriginY:
        style->setPerspectiveOriginY(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitTextEmphasisColor:
        style->setTextEmphasisColor(toAnimatableColor(value)->color());
        style->setVisitedLinkTextEmphasisColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyWebkitTextFillColor:
        style->setTextFillColor(toAnimatableColor(value)->color());
        style->setVisitedLinkTextFillColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyWebkitTextStrokeColor:
        style->setTextStrokeColor(toAnimatableColor(value)->color());
        style->setVisitedLinkTextStrokeColor(toAnimatableColor(value)->visitedLinkColor());
        return;
    case CSSPropertyWebkitTransform:
        style->setTransform(toAnimatableTransform(value)->transformOperations());
        return;
    case CSSPropertyWebkitTransformOriginX:
        style->setTransformOriginX(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitTransformOriginY:
        style->setTransformOriginY(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitTransformOriginZ:
        style->setTransformOriginZ(toAnimatableNumber(value)->toDouble());
        return;
    case CSSPropertyWidth:
        style->setWidth(animatableValueToLength(value, state));
        return;
    case CSSPropertyWordSpacing:
        style->setWordSpacing(clampTo<float>(toAnimatableNumber(value)->toDouble()));
        return;
    case CSSPropertyVisibility:
        style->setVisibility(toAnimatableVisibility(value)->visibility());
        return;
    case CSSPropertyZIndex:
        style->setZIndex(animatableValueRoundClampTo<int>(value));
        return;
    default:
        RELEASE_ASSERT_WITH_MESSAGE(!CSSAnimations::isAnimatableProperty(property), "Web Animations not yet implemented: Unable to apply AnimatableValue to RenderStyle: %s", getPropertyNameString(property).utf8().data());
        ASSERT_NOT_REACHED();
    }
}

} // namespace WebCore
