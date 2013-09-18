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
#include "core/css/FontFace.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "FontFamilyNames.h"
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ExceptionState.h"
#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/dom/Document.h"
#include "core/page/Frame.h"
#include "core/page/Settings.h"
#include "core/platform/graphics/FontTraitsMask.h"
#include "core/svg/SVGFontFaceElement.h"

namespace WebCore {

static PassRefPtr<CSSValue> parseCSSValue(const String& s, CSSPropertyID propertyID)
{
    if (s.isEmpty())
        return 0;
    RefPtr<MutableStylePropertySet> parsedStyle = MutableStylePropertySet::create();
    CSSParser::parseValue(parsedStyle.get(), propertyID, s, true, CSSStrictMode, 0);
    return parsedStyle->getPropertyCSSValue(propertyID);
}

PassRefPtr<FontFace> FontFace::create(const String& family, const String& source, const Dictionary& descriptors, ExceptionState& es)
{
    RefPtr<CSSValue> src = parseCSSValue(source, CSSPropertySrc);
    if (!src || !src->isValueList()) {
        es.throwDOMException(SyntaxError);
        return 0;
    }

    RefPtr<FontFace> fontFace = adoptRef<FontFace>(new FontFace(src));
    fontFace->setFamily(family, es);
    if (es.hadException())
        return 0;

    String value;
    if (descriptors.get("style", value)) {
        fontFace->setStyle(value, es);
        if (es.hadException())
            return 0;
    }
    if (descriptors.get("weight", value)) {
        fontFace->setWeight(value, es);
        if (es.hadException())
            return 0;
    }
    if (descriptors.get("stretch", value)) {
        fontFace->setStretch(value, es);
        if (es.hadException())
            return 0;
    }
    if (descriptors.get("unicodeRange", value)) {
        fontFace->setUnicodeRange(value, es);
        if (es.hadException())
            return 0;
    }
    if (descriptors.get("variant", value)) {
        fontFace->setVariant(value, es);
        if (es.hadException())
            return 0;
    }
    if (descriptors.get("featureSettings", value)) {
        fontFace->setFeatureSettings(value, es);
        if (es.hadException())
            return 0;
    }

    return fontFace;
}

PassRefPtr<FontFace> FontFace::create(const StyleRuleFontFace* fontFaceRule)
{
    const StylePropertySet* properties = fontFaceRule->properties();

    // Obtain the font-family property and the src property. Both must be defined.
    RefPtr<CSSValue> family = properties->getPropertyCSSValue(CSSPropertyFontFamily);
    if (!family || !family->isValueList())
        return 0;
    RefPtr<CSSValue> src = properties->getPropertyCSSValue(CSSPropertySrc);
    if (!src || !src->isValueList())
        return 0;

    RefPtr<FontFace> fontFace = adoptRef<FontFace>(new FontFace(src));

    if (fontFace->setFamilyValue(toCSSValueList(family.get()))
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontStyle)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontWeight)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontStretch)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyUnicodeRange)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontVariant)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyWebkitFontFeatureSettings))
        return fontFace;
    return 0;
}

FontFace::FontFace(PassRefPtr<CSSValue> src)
    : m_src(src)
    , m_status(Unloaded)
{
}

FontFace::~FontFace()
{
}

String FontFace::style() const
{
    return m_style ? m_style->cssText() : "normal";
}

String FontFace::weight() const
{
    return m_weight ? m_weight->cssText() : "normal";
}

String FontFace::stretch() const
{
    return m_stretch ? m_stretch->cssText() : "normal";
}

String FontFace::unicodeRange() const
{
    return m_unicodeRange ? m_unicodeRange->cssText() : "U+0-10FFFF";
}

String FontFace::variant() const
{
    return m_variant ? m_variant->cssText() : "normal";
}

String FontFace::featureSettings() const
{
    return m_featureSettings ? m_featureSettings->cssText() : "normal";
}

void FontFace::setStyle(const String& s, ExceptionState& es)
{
    setPropertyFromString(s, CSSPropertyFontStyle, es);
}

void FontFace::setWeight(const String& s, ExceptionState& es)
{
    setPropertyFromString(s, CSSPropertyFontWeight, es);
}

void FontFace::setStretch(const String& s, ExceptionState& es)
{
    setPropertyFromString(s, CSSPropertyFontStretch, es);
}

void FontFace::setUnicodeRange(const String& s, ExceptionState& es)
{
    setPropertyFromString(s, CSSPropertyUnicodeRange, es);
}

void FontFace::setVariant(const String& s, ExceptionState& es)
{
    setPropertyFromString(s, CSSPropertyFontVariant, es);
}

void FontFace::setFeatureSettings(const String& s, ExceptionState& es)
{
    setPropertyFromString(s, CSSPropertyWebkitFontFeatureSettings, es);
}

void FontFace::setPropertyFromString(const String& s, CSSPropertyID propertyID, ExceptionState& es)
{
    RefPtr<CSSValue> value = parseCSSValue(s, propertyID);
    if (!value || !setPropertyValue(value, propertyID))
        es.throwDOMException(SyntaxError);
}

bool FontFace::setPropertyFromStyle(const StylePropertySet* properties, CSSPropertyID propertyID)
{
    return setPropertyValue(properties->getPropertyCSSValue(propertyID), propertyID);
}

bool FontFace::setPropertyValue(PassRefPtr<CSSValue> value, CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyFontStyle:
        m_style = value;
        break;
    case CSSPropertyFontWeight:
        m_weight = value;
        break;
    case CSSPropertyFontStretch:
        m_stretch = value;
        break;
    case CSSPropertyUnicodeRange:
        if (value && !value->isValueList())
            return false;
        m_unicodeRange = value;
        break;
    case CSSPropertyFontVariant:
        m_variant = value;
        break;
    case CSSPropertyWebkitFontFeatureSettings:
        m_featureSettings = value;
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

bool FontFace::setFamilyValue(CSSValueList* familyList)
{
    // The font-family descriptor has to have exactly one family name.
    if (familyList->length() != 1)
        return false;

    CSSPrimitiveValue* familyValue = toCSSPrimitiveValue(familyList->itemWithoutBoundsCheck(0));
    String family;
    if (familyValue->isString()) {
        family = familyValue->getStringValue();
    } else if (familyValue->isValueID()) {
        // We need to use the raw text for all the generic family types, since @font-face is a way of actually
        // defining what font to use for those types.
        switch (familyValue->getValueID()) {
        case CSSValueSerif:
            family =  FontFamilyNames::serifFamily;
            break;
        case CSSValueSansSerif:
            family =  FontFamilyNames::sansSerifFamily;
            break;
        case CSSValueCursive:
            family =  FontFamilyNames::cursiveFamily;
            break;
        case CSSValueFantasy:
            family =  FontFamilyNames::fantasyFamily;
            break;
        case CSSValueMonospace:
            family =  FontFamilyNames::monospaceFamily;
            break;
        case CSSValueWebkitPictograph:
            family =  FontFamilyNames::pictographFamily;
            break;
        default:
            return false;
        }
    }
    m_family = family;
    return true;
}

String FontFace::status() const
{
    switch (m_status) {
    case Unloaded:
        return "unloaded";
    case Loading:
        return "loading";
    case Loaded:
        return "loaded";
    case Error:
        return "error";
    default:
        ASSERT_NOT_REACHED();
    }
    return emptyString();
}

unsigned FontFace::traitsMask() const
{
    unsigned traitsMask = 0;

    if (m_style) {
        if (!m_style->isPrimitiveValue())
            return 0;

        switch (toCSSPrimitiveValue(m_style.get())->getValueID()) {
        case CSSValueNormal:
            traitsMask |= FontStyleNormalMask;
            break;
        case CSSValueItalic:
        case CSSValueOblique:
            traitsMask |= FontStyleItalicMask;
            break;
        default:
            break;
        }
    } else {
        traitsMask |= FontStyleNormalMask;
    }

    if (m_weight) {
        if (!m_weight->isPrimitiveValue())
            return 0;

        switch (toCSSPrimitiveValue(m_weight.get())->getValueID()) {
        case CSSValueBold:
        case CSSValue700:
            traitsMask |= FontWeight700Mask;
            break;
        case CSSValueNormal:
        case CSSValue400:
            traitsMask |= FontWeight400Mask;
            break;
        case CSSValue900:
            traitsMask |= FontWeight900Mask;
            break;
        case CSSValue800:
            traitsMask |= FontWeight800Mask;
            break;
        case CSSValue600:
            traitsMask |= FontWeight600Mask;
            break;
        case CSSValue500:
            traitsMask |= FontWeight500Mask;
            break;
        case CSSValue300:
            traitsMask |= FontWeight300Mask;
            break;
        case CSSValue200:
            traitsMask |= FontWeight200Mask;
            break;
        case CSSValue100:
            traitsMask |= FontWeight100Mask;
            break;
        default:
            break;
        }
    } else {
        traitsMask |= FontWeight400Mask;
    }

    if (RefPtr<CSSValue> fontVariant = m_variant) {
        // font-variant descriptor can be a value list.
        if (fontVariant->isPrimitiveValue()) {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            list->append(fontVariant);
            fontVariant = list;
        } else if (!fontVariant->isValueList()) {
            return 0;
        }

        CSSValueList* variantList = toCSSValueList(fontVariant.get());
        unsigned numVariants = variantList->length();
        if (!numVariants)
            return 0;

        for (unsigned i = 0; i < numVariants; ++i) {
            switch (toCSSPrimitiveValue(variantList->itemWithoutBoundsCheck(i))->getValueID()) {
            case CSSValueNormal:
                traitsMask |= FontVariantNormalMask;
                break;
            case CSSValueSmallCaps:
                traitsMask |= FontVariantSmallCapsMask;
                break;
            default:
                break;
            }
        }
    } else {
        traitsMask |= FontVariantNormalMask;
    }
    return traitsMask;
}

PassRefPtr<CSSFontFace> FontFace::createCSSFontFace(Document* document)
{
    RefPtr<CSSFontFace> cssFontFace = CSSFontFace::create(this);

    // Each item in the src property's list is a single CSSFontFaceSource. Put them all into a CSSFontFace.
    CSSValueList* srcList = toCSSValueList(m_src.get());
    int srcLength = srcList->length();

    bool foundSVGFont = false;

    for (int i = 0; i < srcLength; i++) {
        // An item in the list either specifies a string (local font name) or a URL (remote font to download).
        CSSFontFaceSrcValue* item = static_cast<CSSFontFaceSrcValue*>(srcList->itemWithoutBoundsCheck(i));
        OwnPtr<CSSFontFaceSource> source;

#if ENABLE(SVG_FONTS)
        foundSVGFont = item->isSVGFontFaceSrc() || item->svgFontFaceElement();
#endif
        if (!item->isLocal()) {
            Settings* settings = document ? document->frame() ? document->frame()->settings() : 0 : 0;
            bool allowDownloading = foundSVGFont || (settings && settings->downloadableBinaryFontsEnabled());
            if (allowDownloading && item->isSupportedFormat() && document) {
                FontResource* fetched = item->fetch(document);
                if (fetched) {
                    source = adoptPtr(new CSSFontFaceSource(item->resource(), fetched));
#if ENABLE(SVG_FONTS)
                    if (foundSVGFont)
                        source->setHasExternalSVGFont(true);
#endif
                }
            }
        } else {
            source = adoptPtr(new CSSFontFaceSource(item->resource()));
        }

        if (source) {
#if ENABLE(SVG_FONTS)
            source->setSVGFontFaceElement(item->svgFontFaceElement());
#endif
            cssFontFace->addSource(source.release());
        }
    }

    if (CSSValueList* rangeList = toCSSValueList(m_unicodeRange.get())) {
        unsigned numRanges = rangeList->length();
        for (unsigned i = 0; i < numRanges; i++) {
            CSSUnicodeRangeValue* range = static_cast<CSSUnicodeRangeValue*>(rangeList->itemWithoutBoundsCheck(i));
            cssFontFace->addRange(range->from(), range->to());
        }
    }
    return cssFontFace;
}

} // namespace WebCore
