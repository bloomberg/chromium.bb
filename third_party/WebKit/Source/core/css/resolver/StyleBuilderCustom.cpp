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

#include "CSSValueKeywords.h"
#include "StyleBuilderFunctions.h"
#include "core/css/resolver/StyleResolver.h"

namespace WebCore {

static void resetEffectiveZoom(StyleResolver* styleResolver)
{
    // Reset the zoom in effect. This allows the setZoom method to accurately compute a new zoom in effect.
    styleResolver->setEffectiveZoom(styleResolver->parentStyle() ? styleResolver->parentStyle()->effectiveZoom() : RenderStyle::initialZoom());
}

void StyleBuilderFunctions::applyInitialCSSPropertyZoom(StyleResolver* styleResolver)
{
    resetEffectiveZoom(styleResolver);
    styleResolver->setZoom(RenderStyle::initialZoom());
}

void StyleBuilderFunctions::applyInheritCSSPropertyZoom(StyleResolver* styleResolver)
{
    resetEffectiveZoom(styleResolver);
    styleResolver->setZoom(styleResolver->parentStyle()->zoom());
}

void StyleBuilderFunctions::applyValueCSSPropertyZoom(StyleResolver* styleResolver, CSSValue* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(value->isPrimitiveValue());
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->getValueID() == CSSValueNormal) {
        resetEffectiveZoom(styleResolver);
        styleResolver->setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue->getValueID() == CSSValueReset) {
        styleResolver->setEffectiveZoom(RenderStyle::initialZoom());
        styleResolver->setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue->getValueID() == CSSValueDocument) {
        float docZoom = styleResolver->rootElementStyle() ? styleResolver->rootElementStyle()->zoom() : RenderStyle::initialZoom();
        styleResolver->setEffectiveZoom(docZoom);
        styleResolver->setZoom(docZoom);
    } else if (primitiveValue->isPercentage()) {
        resetEffectiveZoom(styleResolver);
        if (float percent = primitiveValue->getFloatValue())
            styleResolver->setZoom(percent / 100.0f);
    } else if (primitiveValue->isNumber()) {
        resetEffectiveZoom(styleResolver);
        if (float number = primitiveValue->getFloatValue())
            styleResolver->setZoom(number);
    }
}

} // namespace WebCore
