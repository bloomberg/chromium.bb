/*
 * CSS Media Query
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/MediaQueryExp.h"

#include "CSSValueKeywords.h"
#include "core/css/CSSAspectRatioValue.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/CSSPrimitiveValue.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

static inline bool featureWithCSSValueID(const AtomicString& mediaFeature, const CSSParserValue* value)
{
    if (!value->id)
        return false;

    return mediaFeature == MediaFeatureNames::orientationMediaFeature
        || mediaFeature == MediaFeatureNames::viewModeMediaFeature
        || mediaFeature == MediaFeatureNames::pointerMediaFeature
        || mediaFeature == MediaFeatureNames::scanMediaFeature;
}

static inline bool featureWithValidIdent(const AtomicString& mediaFeature, CSSValueID ident)
{
    if (mediaFeature == MediaFeatureNames::orientationMediaFeature)
        return ident == CSSValuePortrait || ident == CSSValueLandscape;

    if (mediaFeature == MediaFeatureNames::viewModeMediaFeature) {
        switch (ident) {
        case CSSValueWindowed:
        case CSSValueFloating:
        case CSSValueFullscreen:
        case CSSValueMaximized:
        case CSSValueMinimized:
            return true;
        default:
            return false;
        }
    }

    if (mediaFeature == MediaFeatureNames::pointerMediaFeature)
        return ident == CSSValueNone || ident == CSSValueCoarse || ident == CSSValueFine;

    if (mediaFeature == MediaFeatureNames::scanMediaFeature)
        return ident == CSSValueInterlace || ident == CSSValueProgressive;

    ASSERT_NOT_REACHED();
    return false;
}

static inline bool featureWithValidPositiveLenghtOrNumber(const AtomicString& mediaFeature, const CSSParserValue* value)
{
    if (!(((value->unit >= CSSPrimitiveValue::CSS_EMS && value->unit <= CSSPrimitiveValue::CSS_PC) || value->unit == CSSPrimitiveValue::CSS_REMS) || value->unit == CSSPrimitiveValue::CSS_NUMBER) || value->fValue < 0)
        return false;

    return mediaFeature == MediaFeatureNames::heightMediaFeature
        || mediaFeature == MediaFeatureNames::maxHeightMediaFeature
        || mediaFeature == MediaFeatureNames::minHeightMediaFeature
        || mediaFeature == MediaFeatureNames::widthMediaFeature
        || mediaFeature == MediaFeatureNames::maxWidthMediaFeature
        || mediaFeature == MediaFeatureNames::minWidthMediaFeature
        || mediaFeature == MediaFeatureNames::deviceHeightMediaFeature
        || mediaFeature == MediaFeatureNames::maxDeviceHeightMediaFeature
        || mediaFeature == MediaFeatureNames::minDeviceHeightMediaFeature
        || mediaFeature == MediaFeatureNames::deviceWidthMediaFeature
        || mediaFeature == MediaFeatureNames::maxDeviceWidthMediaFeature
        || mediaFeature == MediaFeatureNames::minDeviceWidthMediaFeature;
}

static inline bool featureWithValidDensity(const AtomicString& mediaFeature, const CSSParserValue* value)
{
    if ((value->unit != CSSPrimitiveValue::CSS_DPPX && value->unit != CSSPrimitiveValue::CSS_DPI && value->unit != CSSPrimitiveValue::CSS_DPCM) || value->fValue <= 0)
        return false;

    return mediaFeature == MediaFeatureNames::resolutionMediaFeature
        || mediaFeature == MediaFeatureNames::maxResolutionMediaFeature
        || mediaFeature == MediaFeatureNames::minResolutionMediaFeature;
}

static inline bool featureWithPositiveInteger(const AtomicString& mediaFeature, const CSSParserValue* value)
{
    if (!value->isInt || value->fValue < 0)
        return false;

    return mediaFeature == MediaFeatureNames::colorMediaFeature
        || mediaFeature == MediaFeatureNames::maxColorMediaFeature
        || mediaFeature == MediaFeatureNames::minColorMediaFeature
        || mediaFeature == MediaFeatureNames::colorIndexMediaFeature
        || mediaFeature == MediaFeatureNames::maxColorIndexMediaFeature
        || mediaFeature == MediaFeatureNames::minColorIndexMediaFeature
        || mediaFeature == MediaFeatureNames::monochromeMediaFeature
        || mediaFeature == MediaFeatureNames::minMonochromeMediaFeature
        || mediaFeature == MediaFeatureNames::maxMonochromeMediaFeature;
}

static inline bool featureWithPositiveNumber(const AtomicString& mediaFeature, const CSSParserValue* value)
{
    if (value->unit != CSSPrimitiveValue::CSS_NUMBER || value->fValue < 0)
        return false;

    return mediaFeature == MediaFeatureNames::transform2dMediaFeature
        || mediaFeature == MediaFeatureNames::transform3dMediaFeature
        || mediaFeature == MediaFeatureNames::animationMediaFeature
        || mediaFeature == MediaFeatureNames::devicePixelRatioMediaFeature
        || mediaFeature == MediaFeatureNames::maxDevicePixelRatioMediaFeature
        || mediaFeature == MediaFeatureNames::minDevicePixelRatioMediaFeature;
}

static inline bool featureWithZeroOrOne(const AtomicString& mediaFeature, const CSSParserValue* value)
{
    if (!value->isInt || !(value->fValue == 1 || !value->fValue))
        return false;

    return mediaFeature == MediaFeatureNames::gridMediaFeature
        || mediaFeature == MediaFeatureNames::hoverMediaFeature;
}

static inline bool featureWithAspectRatio(const AtomicString& mediaFeature)
{
    return mediaFeature == MediaFeatureNames::aspectRatioMediaFeature
        || mediaFeature == MediaFeatureNames::deviceAspectRatioMediaFeature
        || mediaFeature == MediaFeatureNames::minAspectRatioMediaFeature
        || mediaFeature == MediaFeatureNames::maxAspectRatioMediaFeature
        || mediaFeature == MediaFeatureNames::minDeviceAspectRatioMediaFeature
        || mediaFeature == MediaFeatureNames::maxDeviceAspectRatioMediaFeature;
}

static inline bool featureWithoutValue(const AtomicString& mediaFeature)
{
    // Media features that are prefixed by min/max cannot be used without a value.
    return mediaFeature == MediaFeatureNames::monochromeMediaFeature
        || mediaFeature == MediaFeatureNames::colorMediaFeature
        || mediaFeature == MediaFeatureNames::colorIndexMediaFeature
        || mediaFeature == MediaFeatureNames::gridMediaFeature
        || mediaFeature == MediaFeatureNames::heightMediaFeature
        || mediaFeature == MediaFeatureNames::widthMediaFeature
        || mediaFeature == MediaFeatureNames::deviceHeightMediaFeature
        || mediaFeature == MediaFeatureNames::deviceWidthMediaFeature
        || mediaFeature == MediaFeatureNames::orientationMediaFeature
        || mediaFeature == MediaFeatureNames::aspectRatioMediaFeature
        || mediaFeature == MediaFeatureNames::deviceAspectRatioMediaFeature
        || mediaFeature == MediaFeatureNames::hoverMediaFeature
        || mediaFeature == MediaFeatureNames::transform2dMediaFeature
        || mediaFeature == MediaFeatureNames::transform3dMediaFeature
        || mediaFeature == MediaFeatureNames::animationMediaFeature
        || mediaFeature == MediaFeatureNames::viewModeMediaFeature
        || mediaFeature == MediaFeatureNames::pointerMediaFeature
        || mediaFeature == MediaFeatureNames::devicePixelRatioMediaFeature
        || mediaFeature == MediaFeatureNames::resolutionMediaFeature
        || mediaFeature == MediaFeatureNames::scanMediaFeature;
}

bool MediaQueryExp::isViewportDependent() const
{
    return m_mediaFeature == MediaFeatureNames::widthMediaFeature
        || m_mediaFeature == MediaFeatureNames::heightMediaFeature
        || m_mediaFeature == MediaFeatureNames::minWidthMediaFeature
        || m_mediaFeature == MediaFeatureNames::minHeightMediaFeature
        || m_mediaFeature == MediaFeatureNames::maxWidthMediaFeature
        || m_mediaFeature == MediaFeatureNames::maxHeightMediaFeature
        || m_mediaFeature == MediaFeatureNames::orientationMediaFeature
        || m_mediaFeature == MediaFeatureNames::aspectRatioMediaFeature
        || m_mediaFeature == MediaFeatureNames::minAspectRatioMediaFeature
        || m_mediaFeature == MediaFeatureNames::devicePixelRatioMediaFeature
        || m_mediaFeature == MediaFeatureNames::resolutionMediaFeature
        || m_mediaFeature == MediaFeatureNames::maxAspectRatioMediaFeature;
}

MediaQueryExp::MediaQueryExp(const MediaQueryExp& other)
    : m_mediaFeature(other.mediaFeature())
    , m_value(other.value())
{
}

MediaQueryExp::MediaQueryExp(const AtomicString& mediaFeature, PassRefPtrWillBeRawPtr<CSSValue> value)
    : m_mediaFeature(mediaFeature)
    , m_value(value)
{
}

PassOwnPtrWillBeRawPtr<MediaQueryExp> MediaQueryExp::create(const AtomicString& mediaFeature, CSSParserValueList* valueList)
{
    RefPtrWillBeRawPtr<CSSValue> cssValue;
    bool isValid = false;

    // Create value for media query expression that must have 1 or more values.
    if (valueList) {
        if (valueList->size() == 1) {
            CSSParserValue* value = valueList->current();

            if (featureWithCSSValueID(mediaFeature, value)) {
                // Media features that use CSSValueIDs.
                cssValue = CSSPrimitiveValue::createIdentifier(value->id);
                if (!featureWithValidIdent(mediaFeature, toCSSPrimitiveValue(cssValue.get())->getValueID()))
                    cssValue.clear();
            } else if (featureWithValidDensity(mediaFeature, value)) {
                // Media features that must have non-negative <density>, ie. dppx, dpi or dpcm.
                cssValue = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
            } else if (featureWithValidPositiveLenghtOrNumber(mediaFeature, value)) {
                // Media features that must have non-negative <lenght> or number value.
                cssValue = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
            } else if (featureWithPositiveInteger(mediaFeature, value)) {
                // Media features that must have non-negative integer value.
                cssValue = CSSPrimitiveValue::create(value->fValue, CSSPrimitiveValue::CSS_NUMBER);
            } else if (featureWithPositiveNumber(mediaFeature, value)) {
                // Media features that must have non-negative number value.
                cssValue = CSSPrimitiveValue::create(value->fValue, CSSPrimitiveValue::CSS_NUMBER);
            } else if (featureWithZeroOrOne(mediaFeature, value)) {
                // Media features that must have (0|1) value.
                cssValue = CSSPrimitiveValue::create(value->fValue, CSSPrimitiveValue::CSS_NUMBER);
            }

            isValid = cssValue;

        } else if (valueList->size() == 3 && featureWithAspectRatio(mediaFeature)) {
            // Create list of values.
            // Currently accepts only <integer>/<integer>.
            // Applicable to device-aspect-ratio and aspec-ratio.
            isValid = true;
            float numeratorValue = 0;
            float denominatorValue = 0;
            // The aspect-ratio must be <integer> (whitespace)? / (whitespace)? <integer>.
            for (unsigned i = 0; i < 3; ++i, valueList->next()) {
                const CSSParserValue* value = valueList->current();
                if (i != 1 && value->unit == CSSPrimitiveValue::CSS_NUMBER && value->fValue > 0 && value->isInt) {
                    if (!i)
                        numeratorValue = value->fValue;
                    else
                        denominatorValue = value->fValue;
                } else if (i == 1 && value->unit == CSSParserValue::Operator && value->iValue == '/') {
                    continue;
                } else {
                    isValid = false;
                    break;
                }
            }

            if (isValid)
                cssValue = CSSAspectRatioValue::create(numeratorValue, denominatorValue);
        }
    } else if (featureWithoutValue(mediaFeature)) {
        isValid = true;
    }

    if (!isValid)
        return nullptr;

    return adoptPtrWillBeNoop(new MediaQueryExp(mediaFeature, cssValue));
}

MediaQueryExp::~MediaQueryExp()
{
}

String MediaQueryExp::serialize() const
{
    StringBuilder result;
    result.append("(");
    result.append(m_mediaFeature.lower());
    if (m_value) {
        result.append(": ");
        result.append(m_value->cssText());
    }
    result.append(")");

    return result.toString();
}

} // namespace
