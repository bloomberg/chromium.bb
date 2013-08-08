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

#include "core/animation/AnimatableNumber.h"
#include "core/animation/AnimatableUnknown.h"
#include "core/animation/AnimatableValue.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/rendering/style/RenderStyle.h"

namespace WebCore {

namespace {

Length animatableValueToLength(const AnimatableValue* value, const StyleResolverState& state)
{
    const RenderStyle* style = state.style();
    return toAnimatableNumber(value)->toLength(style, state.rootElementStyle(), style->effectiveZoom());
}

} // namespace

// FIXME: This should handle all animatable properties
//   (see CSSAnimatableValueFactory for list of remaining)

// FIXME: Generate this function.
void AnimatedStyleBuilder::applyProperty(CSSPropertyID property, StyleResolverState& state, const AnimatableValue* value)
{
    if (value->isUnknown()) {
        StyleBuilder::applyProperty(property, state, toAnimatableUnknown(value)->toCSSValue().get());
        return;
    }
    RenderStyle* style = state.style();
    switch (property) {
    case CSSPropertyBottom:
        style->setBottom(animatableValueToLength(value, state));
        return;
    case CSSPropertyHeight:
        style->setHeight(animatableValueToLength(value, state));
        return;
    case CSSPropertyLeft:
        style->setLeft(animatableValueToLength(value, state));
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
    case CSSPropertyTop:
        style->setTop(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitPerspectiveOriginX:
        style->setPerspectiveOriginX(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitPerspectiveOriginY:
        style->setPerspectiveOriginY(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitTransformOriginX:
        style->setTransformOriginX(animatableValueToLength(value, state));
        return;
    case CSSPropertyWebkitTransformOriginY:
        style->setTransformOriginY(animatableValueToLength(value, state));
        return;
    case CSSPropertyWidth:
        style->setWidth(animatableValueToLength(value, state));
        return;
    default:
        RELEASE_ASSERT_WITH_MESSAGE(false, "Unable to apply AnimatableValue to RenderStyle, not yet implemented!");
        return;
    }
}

} // namespace WebCore
