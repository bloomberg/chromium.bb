/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/page/RuntimeCSSEnabled.h"
#include "RuntimeEnabledFeatures.h"
#include "wtf/Vector.h"

namespace WebCore {

// FIXME: We should use a real BitVector class instead!
typedef Vector<bool> BoolVector;

static void setCSSPropertiesEnabled(CSSPropertyID* properties, size_t length, bool featureFlag)
{
    for (size_t i = 0; i < length; i++)
        RuntimeCSSEnabled::setCSSPropertyEnabled(properties[i], featureFlag);
}

static void setPropertySwitchesFromRuntimeFeatures()
{
    CSSPropertyID regionProperites[] = {
        CSSPropertyWebkitFlowInto,
        CSSPropertyWebkitFlowFrom,
        CSSPropertyWebkitRegionFragment,
        CSSPropertyWebkitRegionBreakAfter,
        CSSPropertyWebkitRegionBreakBefore,
        CSSPropertyWebkitRegionBreakInside
    };
    setCSSPropertiesEnabled(regionProperites, WTF_ARRAY_LENGTH(regionProperites), RuntimeEnabledFeatures::cssRegionsEnabled());
    CSSPropertyID exclusionProperties[] = {
        CSSPropertyWebkitWrapFlow,
        CSSPropertyWebkitWrapThrough,
    };
    setCSSPropertiesEnabled(exclusionProperties, WTF_ARRAY_LENGTH(exclusionProperties), RuntimeEnabledFeatures::cssExclusionsEnabled());
    CSSPropertyID shapeProperties[] = {
        CSSPropertyWebkitShapeMargin,
        CSSPropertyWebkitShapePadding,
        CSSPropertyWebkitShapeInside,
        CSSPropertyWebkitShapeOutside,
    };
    setCSSPropertiesEnabled(shapeProperties, WTF_ARRAY_LENGTH(shapeProperties), RuntimeEnabledFeatures::cssShapesEnabled());
    CSSPropertyID css3TextDecorationProperties[] = {
        CSSPropertyTextDecorationColor,
        CSSPropertyTextDecorationLine,
        CSSPropertyTextDecorationStyle,
    };
    setCSSPropertiesEnabled(css3TextDecorationProperties, WTF_ARRAY_LENGTH(css3TextDecorationProperties), RuntimeEnabledFeatures::css3TextDecorationsEnabled());
    CSSPropertyID css3TextProperties[] = {
        CSSPropertyTextAlignLast,
    };
    setCSSPropertiesEnabled(css3TextProperties, WTF_ARRAY_LENGTH(css3TextProperties), RuntimeEnabledFeatures::css3TextEnabled());
    CSSPropertyID cssGridLayoutProperties[] = {
        CSSPropertyGridAutoColumns,
        CSSPropertyGridAutoRows,
        CSSPropertyGridDefinitionColumns,
        CSSPropertyGridDefinitionRows,
        CSSPropertyGridColumnStart,
        CSSPropertyGridColumnEnd,
        CSSPropertyGridRowStart,
        CSSPropertyGridRowEnd,
        CSSPropertyGridColumn,
        CSSPropertyGridRow,
        CSSPropertyGridArea,
        CSSPropertyGridAutoFlow,
        CSSPropertyGridTemplate
    };
    setCSSPropertiesEnabled(cssGridLayoutProperties, WTF_ARRAY_LENGTH(cssGridLayoutProperties), RuntimeEnabledFeatures::cssGridLayoutEnabled());

    CSSPropertyID animationProperties[] = {
        CSSPropertyAnimation,
        CSSPropertyAnimationName,
        CSSPropertyAnimationDuration,
        CSSPropertyAnimationTimingFunction,
        CSSPropertyAnimationDelay,
        CSSPropertyAnimationIterationCount,
        CSSPropertyAnimationDirection,
        CSSPropertyAnimationFillMode,
        CSSPropertyAnimationPlayState
    };
    setCSSPropertiesEnabled(animationProperties, WTF_ARRAY_LENGTH(animationProperties), RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled());

    RuntimeCSSEnabled::setCSSPropertyEnabled(CSSPropertyBackgroundBlendMode, RuntimeEnabledFeatures::cssCompositingEnabled());
    RuntimeCSSEnabled::setCSSPropertyEnabled(CSSPropertyMixBlendMode, RuntimeEnabledFeatures::cssCompositingEnabled());
    RuntimeCSSEnabled::setCSSPropertyEnabled(CSSPropertyTouchAction, RuntimeEnabledFeatures::cssTouchActionEnabled());
    RuntimeCSSEnabled::setCSSPropertyEnabled(CSSPropertyPaintOrder, RuntimeEnabledFeatures::svgPaintOrderEnabled());
    RuntimeCSSEnabled::setCSSPropertyEnabled(CSSPropertyVariable, RuntimeEnabledFeatures::cssVariablesEnabled());
    RuntimeCSSEnabled::setCSSPropertyEnabled(CSSPropertyObjectFit, RuntimeEnabledFeatures::objectFitPositionEnabled());
    RuntimeCSSEnabled::setCSSPropertyEnabled(CSSPropertyMaskSourceType, RuntimeEnabledFeatures::cssMaskSourceTypeEnabled());
}

static BoolVector& propertySwitches()
{
    static BoolVector* switches = 0;
    if (!switches) {
        switches = new BoolVector;
        // Accomodate CSSPropertyIDs that fall outside the firstCSSProperty, lastCSSProperty range (eg. CSSPropertyVariable).
        switches->fill(true, lastCSSProperty + 1);
        setPropertySwitchesFromRuntimeFeatures();
    }
    return *switches;
}

size_t indexForProperty(CSSPropertyID propertyId)
{
    RELEASE_ASSERT(propertyId >= 0 && propertyId <= lastCSSProperty);
    ASSERT(propertyId != CSSPropertyInvalid);
    return static_cast<size_t>(propertyId);
}

bool RuntimeCSSEnabled::isCSSPropertyEnabled(CSSPropertyID propertyId)
{
    // Internal properties shouldn't be exposed to the web
    // so they are considered to be always disabled.
    if (isInternalProperty(propertyId))
        return false;

    return propertySwitches()[indexForProperty(propertyId)];
}

void RuntimeCSSEnabled::setCSSPropertyEnabled(CSSPropertyID propertyId, bool enable)
{
    propertySwitches()[indexForProperty(propertyId)] = enable;
}

void RuntimeCSSEnabled::filterEnabledCSSPropertiesIntoVector(const CSSPropertyID* properties, size_t propertyCount, Vector<CSSPropertyID>& outVector)
{
    for (unsigned i = 0; i < propertyCount; i++) {
        CSSPropertyID property = properties[i];
        if (RuntimeCSSEnabled::isCSSPropertyEnabled(property))
            outVector.append(property);
    }
}

} // namespace WebCore
