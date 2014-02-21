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

#include "CSSValueKeywords.h"
#include "FontFamilyNames.h"
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/ScriptScope.h"
#include "bindings/v8/ScriptState.h"
#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/dom/DOMError.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/Frame.h"
#include "core/frame/Settings.h"
#include "core/svg/SVGFontFaceElement.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontTraitsMask.h"

namespace WebCore {

class FontFaceReadyPromiseResolver {
public:
    static PassOwnPtr<FontFaceReadyPromiseResolver> create(ScriptPromise promise, ExecutionContext* context)
    {
        return adoptPtr(new FontFaceReadyPromiseResolver(promise, context));
    }

    void resolve(PassRefPtr<FontFace> fontFace)
    {
        ScriptScope scope(m_scriptState);
        switch (fontFace->loadStatus()) {
        case FontFace::Loaded:
            m_resolver->resolve(fontFace);
            break;
        case FontFace::Error:
            m_resolver->reject(DOMError::create(NetworkError));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

private:
    FontFaceReadyPromiseResolver(ScriptPromise promise, ExecutionContext* context)
        : m_scriptState(ScriptState::current())
        , m_resolver(ScriptPromiseResolver::create(promise, context))
    { }
    ScriptState* m_scriptState;
    RefPtr<ScriptPromiseResolver> m_resolver;
};

static PassRefPtrWillBeRawPtr<CSSValue> parseCSSValue(const Document* document, const String& s, CSSPropertyID propertyID)
{
    if (s.isEmpty())
        return nullptr;
    RefPtr<MutableStylePropertySet> parsedStyle = MutableStylePropertySet::create();
    BisonCSSParser::parseValue(parsedStyle.get(), propertyID, s, true, *document);
    return parsedStyle->getPropertyCSSValue(propertyID);
}

PassRefPtr<FontFace> FontFace::create(ExecutionContext* context, const AtomicString& family, const String& source, const Dictionary& descriptors, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<CSSValue> src = parseCSSValue(toDocument(context), source, CSSPropertySrc);
    if (!src || !src->isValueList()) {
        exceptionState.throwDOMException(SyntaxError, "The source provided ('" + source + "') could not be parsed as a value list.");
        return nullptr;
    }

    RefPtr<FontFace> fontFace = adoptRef<FontFace>(new FontFace(src));
    fontFace->setFamily(context, family, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    String value;
    if (descriptors.get("style", value)) {
        fontFace->setStyle(context, value, exceptionState);
        if (exceptionState.hadException())
            return nullptr;
    }
    if (descriptors.get("weight", value)) {
        fontFace->setWeight(context, value, exceptionState);
        if (exceptionState.hadException())
            return nullptr;
    }
    if (descriptors.get("stretch", value)) {
        fontFace->setStretch(context, value, exceptionState);
        if (exceptionState.hadException())
            return nullptr;
    }
    if (descriptors.get("unicodeRange", value)) {
        fontFace->setUnicodeRange(context, value, exceptionState);
        if (exceptionState.hadException())
            return nullptr;
    }
    if (descriptors.get("variant", value)) {
        fontFace->setVariant(context, value, exceptionState);
        if (exceptionState.hadException())
            return nullptr;
    }
    if (descriptors.get("featureSettings", value)) {
        fontFace->setFeatureSettings(context, value, exceptionState);
        if (exceptionState.hadException())
            return nullptr;
    }

    fontFace->initCSSFontFace(toDocument(context));
    return fontFace;
}

PassRefPtr<FontFace> FontFace::create(Document* document, const StyleRuleFontFace* fontFaceRule)
{
    const StylePropertySet* properties = fontFaceRule->properties();

    // Obtain the font-family property and the src property. Both must be defined.
    RefPtrWillBeRawPtr<CSSValue> family = properties->getPropertyCSSValue(CSSPropertyFontFamily);
    if (!family || !family->isValueList())
        return nullptr;
    RefPtrWillBeRawPtr<CSSValue> src = properties->getPropertyCSSValue(CSSPropertySrc);
    if (!src || !src->isValueList())
        return nullptr;

    RefPtr<FontFace> fontFace = adoptRef<FontFace>(new FontFace(src));

    if (fontFace->setFamilyValue(toCSSValueList(family.get()))
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontStyle)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontWeight)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontStretch)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyUnicodeRange)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontVariant)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyWebkitFontFeatureSettings)
        && !fontFace->family().isEmpty()
        && fontFace->traitsMask()) {
        fontFace->initCSSFontFace(document);
        return fontFace;
    }
    return nullptr;
}

FontFace::FontFace(PassRefPtrWillBeRawPtr<CSSValue> src)
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

void FontFace::setStyle(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyFontStyle, exceptionState);
}

void FontFace::setWeight(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyFontWeight, exceptionState);
}

void FontFace::setStretch(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyFontStretch, exceptionState);
}

void FontFace::setUnicodeRange(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyUnicodeRange, exceptionState);
}

void FontFace::setVariant(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyFontVariant, exceptionState);
}

void FontFace::setFeatureSettings(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyWebkitFontFeatureSettings, exceptionState);
}

void FontFace::setPropertyFromString(const Document* document, const String& s, CSSPropertyID propertyID, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<CSSValue> value = parseCSSValue(document, s, propertyID);
    if (!value || !setPropertyValue(value, propertyID))
        exceptionState.throwDOMException(SyntaxError, "Failed to set '" + s + "' as a property value.");
}

bool FontFace::setPropertyFromStyle(const StylePropertySet* properties, CSSPropertyID propertyID)
{
    return setPropertyValue(properties->getPropertyCSSValue(propertyID), propertyID);
}

bool FontFace::setPropertyValue(PassRefPtrWillBeRawPtr<CSSValue> value, CSSPropertyID propertyID)
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
    AtomicString family;
    if (familyValue->isString()) {
        family = AtomicString(familyValue->getStringValue());
    } else if (familyValue->isValueID()) {
        // We need to use the raw text for all the generic family types, since @font-face is a way of actually
        // defining what font to use for those types.
        switch (familyValue->getValueID()) {
        case CSSValueSerif:
            family =  FontFamilyNames::webkit_serif;
            break;
        case CSSValueSansSerif:
            family =  FontFamilyNames::webkit_sans_serif;
            break;
        case CSSValueCursive:
            family =  FontFamilyNames::webkit_cursive;
            break;
        case CSSValueFantasy:
            family =  FontFamilyNames::webkit_fantasy;
            break;
        case CSSValueMonospace:
            family =  FontFamilyNames::webkit_monospace;
            break;
        case CSSValueWebkitPictograph:
            family =  FontFamilyNames::webkit_pictograph;
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

void FontFace::setLoadStatus(LoadStatus status)
{
    m_status = status;
    if (m_status == Loaded || m_status == Error)
        resolveReadyPromises();
}

void FontFace::load(ExecutionContext* context)
{
    if (m_status != Unloaded)
        return;

    FontDescription fontDescription;
    FontFamily fontFamily;
    fontFamily.setFamily(m_family);
    fontDescription.setFamily(fontFamily);
    fontDescription.setTraitsMask(static_cast<FontTraitsMask>(traitsMask()));

    CSSFontSelector* fontSelector = toDocument(context)->styleEngine()->fontSelector();
    m_cssFontFace->load(fontDescription, fontSelector);
    fontSelector->loadPendingFonts();
}

ScriptPromise FontFace::ready(ExecutionContext* context)
{
    ScriptPromise promise = ScriptPromise::createPending(context);
    OwnPtr<FontFaceReadyPromiseResolver> resolver = FontFaceReadyPromiseResolver::create(promise, context);
    if (m_status == Loaded || m_status == Error)
        resolver->resolve(this);
    else
        m_readyResolvers.append(resolver.release());
    return promise;
}

void FontFace::resolveReadyPromises()
{
    for (size_t i = 0; i < m_readyResolvers.size(); i++)
        m_readyResolvers[i]->resolve(this);
    m_readyResolvers.clear();
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
        case CSSValueLighter:
        case CSSValue100:
            traitsMask |= FontWeight100Mask;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else {
        traitsMask |= FontWeight400Mask;
    }

    if (RefPtr<CSSValue> fontVariant = m_variant) {
        // font-variant descriptor can be a value list.
        if (fontVariant->isPrimitiveValue()) {
            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
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

void FontFace::initCSSFontFace(Document* document)
{
    Vector<CSSFontFace::UnicodeRange> ranges;
    if (CSSValueList* rangeList = toCSSValueList(m_unicodeRange.get())) {
        unsigned numRanges = rangeList->length();
        for (unsigned i = 0; i < numRanges; i++) {
            CSSUnicodeRangeValue* range = toCSSUnicodeRangeValue(rangeList->itemWithoutBoundsCheck(i));
            ranges.append(CSSFontFace::UnicodeRange(range->from(), range->to()));
        }
    }

    m_cssFontFace = adoptPtr(new CSSFontFace(this, ranges));

    // Each item in the src property's list is a single CSSFontFaceSource. Put them all into a CSSFontFace.
    CSSValueList* srcList = toCSSValueList(m_src.get());
    int srcLength = srcList->length();

    bool foundSVGFont = false;

    for (int i = 0; i < srcLength; i++) {
        // An item in the list either specifies a string (local font name) or a URL (remote font to download).
        CSSFontFaceSrcValue* item = toCSSFontFaceSrcValue(srcList->itemWithoutBoundsCheck(i));
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
            m_cssFontFace->addSource(source.release());
        }
    }
}

} // namespace WebCore
