/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/css/resolver/FontBuilder.h"

#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/FontSize.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "platform/text/LocaleToScriptMapping.h"

namespace WebCore {

// FIXME: This scoping class is a short-term fix to minimize the changes in
// Font-constructing logic.
class FontDescriptionChangeScope {
public:
    FontDescriptionChangeScope(FontBuilder* fontBuilder)
        : m_fontBuilder(fontBuilder)
        , m_fontDescription(fontBuilder->m_style->fontDescription())
    {
    }

    void reset() { m_fontDescription = FontDescription(); }
    void set(const FontDescription& fontDescription) { m_fontDescription = fontDescription; }
    FontDescription& fontDescription() { return m_fontDescription; }

    ~FontDescriptionChangeScope()
    {
        m_fontBuilder->didChangeFontParameters(m_fontBuilder->m_style->setFontDescription(m_fontDescription));
    }

private:
    FontBuilder* m_fontBuilder;
    FontDescription m_fontDescription;
};

FontBuilder::FontBuilder()
    : m_document(0)
    , m_useSVGZoomRules(false)
    , m_fontSizehasViewportUnits(false)
    , m_fontDirty(false)
{
}

void FontBuilder::initForStyleResolve(const Document& document, RenderStyle* style, bool useSVGZoomRules)
{
    // All documents need to be in a frame (and thus have access to Settings)
    // for style-resolution to make sense.
    // Unfortunately SVG Animations currently violate this: crbug.com/260966
    // ASSERT(m_document->frame());
    m_document = &document;
    m_useSVGZoomRules = useSVGZoomRules;
    m_style = style;
    m_fontDirty = false;
}

void FontBuilder::setInitial(float effectiveZoom)
{
    ASSERT(m_document && m_document->settings());
    if (!m_document || !m_document->settings())
        return;

    FontDescriptionChangeScope scope(this);

    scope.reset();
    scope.fontDescription().setGenericFamily(FontDescription::StandardFamily);
    scope.fontDescription().setUsePrinterFont(m_document->printing());
    const AtomicString& standardFontFamily = m_document->settings()->genericFontFamilySettings().standard();
    if (!standardFontFamily.isEmpty()) {
        scope.fontDescription().firstFamily().setFamily(standardFontFamily);
        scope.fontDescription().firstFamily().appendFamily(nullptr);
    }
    scope.fontDescription().setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
    setSize(scope.fontDescription(), effectiveZoom, FontSize::fontSizeForKeyword(m_document, CSSValueMedium, false));
}

void FontBuilder::inheritFrom(const FontDescription& fontDescription)
{
    FontDescriptionChangeScope scope(this);

    scope.set(fontDescription);
}

void FontBuilder::didChangeFontParameters(bool changed)
{
    m_fontDirty |= changed;
}

void FontBuilder::fromSystemFont(CSSValueID valueId, float effectiveZoom)
{
    FontDescriptionChangeScope scope(this);

    FontDescription fontDescription;
    RenderTheme::theme().systemFont(valueId, fontDescription);

    // Double-check and see if the theme did anything. If not, don't bother updating the font.
    if (!fontDescription.isAbsoluteSize())
        return;

    // Make sure the rendering mode and printer font settings are updated.
    const Settings* settings = m_document->settings();
    ASSERT(settings); // If we're doing style resolution, this document should always be in a frame and thus have settings
    if (!settings)
        return;
    fontDescription.setUsePrinterFont(m_document->printing());

    // Handle the zoom factor.
    fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(fontDescription, effectiveZoom, fontDescription.specifiedSize()));
    scope.set(fontDescription);
}

void FontBuilder::setFontFamilyInitial(float effectiveZoom)
{
    FontDescriptionChangeScope scope(this);

    FontDescription initialDesc = FontDescription();

    // We need to adjust the size to account for the generic family change from monospace to non-monospace.
    if (scope.fontDescription().keywordSize() && scope.fontDescription().useFixedDefaultSize())
        setSize(scope.fontDescription(), effectiveZoom, FontSize::fontSizeForKeyword(m_document, CSSValueXxSmall + scope.fontDescription().keywordSize() - 1, false));
    scope.fontDescription().setGenericFamily(initialDesc.genericFamily());
    if (!initialDesc.firstFamily().familyIsEmpty())
        scope.fontDescription().setFamily(initialDesc.firstFamily());
}

void FontBuilder::setFontFamilyInherit(const FontDescription& parentFontDescription)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setGenericFamily(parentFontDescription.genericFamily());
    scope.fontDescription().setFamily(parentFontDescription.family());
    scope.fontDescription().setIsSpecifiedFont(parentFontDescription.isSpecifiedFont());
}

// FIXME: I am not convinced FontBuilder needs to know anything about CSSValues.
void FontBuilder::setFontFamilyValue(CSSValue* value, float effectiveZoom)
{
    FontDescriptionChangeScope scope(this);

    if (!value->isValueList())
        return;

    FontFamily& firstFamily = scope.fontDescription().firstFamily();
    FontFamily* currFamily = 0;

    // Before mapping in a new font-family property, we should reset the generic family.
    bool oldFamilyUsedFixedDefaultSize = scope.fontDescription().useFixedDefaultSize();
    scope.fontDescription().setGenericFamily(FontDescription::NoFamily);

    for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
        CSSValue* item = i.value();
        if (!item->isPrimitiveValue())
            continue;
        CSSPrimitiveValue* contentValue = toCSSPrimitiveValue(item);
        AtomicString face;
        Settings* settings = m_document->settings();
        if (contentValue->isString()) {
            face = AtomicString(contentValue->getStringValue());
        } else if (settings) {
            switch (contentValue->getValueID()) {
            case CSSValueWebkitBody:
                face = settings->genericFontFamilySettings().standard();
                break;
            case CSSValueSerif:
                face = FontFamilyNames::webkit_serif;
                scope.fontDescription().setGenericFamily(FontDescription::SerifFamily);
                break;
            case CSSValueSansSerif:
                face = FontFamilyNames::webkit_sans_serif;
                scope.fontDescription().setGenericFamily(FontDescription::SansSerifFamily);
                break;
            case CSSValueCursive:
                face = FontFamilyNames::webkit_cursive;
                scope.fontDescription().setGenericFamily(FontDescription::CursiveFamily);
                break;
            case CSSValueFantasy:
                face = FontFamilyNames::webkit_fantasy;
                scope.fontDescription().setGenericFamily(FontDescription::FantasyFamily);
                break;
            case CSSValueMonospace:
                face = FontFamilyNames::webkit_monospace;
                scope.fontDescription().setGenericFamily(FontDescription::MonospaceFamily);
                break;
            case CSSValueWebkitPictograph:
                face = FontFamilyNames::webkit_pictograph;
                scope.fontDescription().setGenericFamily(FontDescription::PictographFamily);
                break;
            default:
                break;
            }
        }

        if (!face.isEmpty()) {
            if (!currFamily) {
                // Filling in the first family.
                firstFamily.setFamily(face);
                firstFamily.appendFamily(nullptr); // Remove any inherited family-fallback list.
                currFamily = &firstFamily;
                scope.fontDescription().setIsSpecifiedFont(scope.fontDescription().genericFamily() == FontDescription::NoFamily);
            } else {
                RefPtr<SharedFontFamily> newFamily = SharedFontFamily::create();
                newFamily->setFamily(face);
                currFamily->appendFamily(newFamily);
                currFamily = newFamily.get();
            }
        }
    }

    // We can't call useFixedDefaultSize() until all new font families have been added
    // If currFamily is non-zero then we set at least one family on this description.
    if (!currFamily)
        return;

    if (scope.fontDescription().keywordSize() && scope.fontDescription().useFixedDefaultSize() != oldFamilyUsedFixedDefaultSize)
        setSize(scope.fontDescription(), effectiveZoom, FontSize::fontSizeForKeyword(m_document, CSSValueXxSmall + scope.fontDescription().keywordSize() - 1, !oldFamilyUsedFixedDefaultSize));
}

void FontBuilder::setFontSizeInitial(float effectiveZoom)
{
    FontDescriptionChangeScope scope(this);

    float size = FontSize::fontSizeForKeyword(m_document, CSSValueMedium, scope.fontDescription().useFixedDefaultSize());

    if (size < 0)
        return;

    scope.fontDescription().setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
    setSize(scope.fontDescription(), effectiveZoom, size);
}

void FontBuilder::setFontSizeInherit(const FontDescription& parentFontDescription, float effectiveZoom)
{
    FontDescriptionChangeScope scope(this);

    float size = parentFontDescription.specifiedSize();

    if (size < 0)
        return;

    scope.fontDescription().setKeywordSize(parentFontDescription.keywordSize());
    setSize(scope.fontDescription(), effectiveZoom, size);
}

// FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large)
// and scale down/up to the next size level.
static float largerFontSize(float size)
{
    return size * 1.2f;
}

static float smallerFontSize(float size)
{
    return size / 1.2f;
}

// FIXME: Have to pass RenderStyles here for calc/computed values. This shouldn't be neecessary.
void FontBuilder::setFontSizeValue(CSSValue* value, RenderStyle* parentStyle, const RenderStyle* rootElementStyle, float effectiveZoom)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setKeywordSize(0);
    float parentSize = 0;
    bool parentIsAbsoluteSize = false;
    float size = 0;

    // FIXME: Find out when parentStyle could be 0?
    if (parentStyle) {
        parentSize = parentStyle->fontDescription().specifiedSize();
        parentIsAbsoluteSize = parentStyle->fontDescription().isAbsoluteSize();
    }

    if (CSSValueID valueID = primitiveValue->getValueID()) {
        switch (valueID) {
        case CSSValueXxSmall:
        case CSSValueXSmall:
        case CSSValueSmall:
        case CSSValueMedium:
        case CSSValueLarge:
        case CSSValueXLarge:
        case CSSValueXxLarge:
        case CSSValueWebkitXxxLarge:
            size = FontSize::fontSizeForKeyword(m_document, valueID, scope.fontDescription().useFixedDefaultSize());
            scope.fontDescription().setKeywordSize(valueID - CSSValueXxSmall + 1);
            break;
        case CSSValueLarger:
            size = largerFontSize(parentSize);
            break;
        case CSSValueSmaller:
            size = smallerFontSize(parentSize);
            break;
        default:
            return;
        }

        scope.fontDescription().setIsAbsoluteSize(parentIsAbsoluteSize && (valueID == CSSValueLarger || valueID == CSSValueSmaller));
    } else {
        scope.fontDescription().setIsAbsoluteSize(parentIsAbsoluteSize || !(primitiveValue->isPercentage() || primitiveValue->isFontRelativeLength()));
        if (primitiveValue->isPercentage()) {
            size = (primitiveValue->getFloatValue() * parentSize) / 100.0f;
        } else {
            // If we have viewport units the conversion will mark the parent style as having viewport units.
            bool parentHasViewportUnits = parentStyle->hasViewportUnits();
            parentStyle->setHasViewportUnits(false);
            CSSToLengthConversionData conversionData(parentStyle, rootElementStyle, m_document->renderView(), 1.0f, true);
            if (primitiveValue->isLength())
                size = primitiveValue->computeLength<float>(conversionData);
            else if (primitiveValue->isCalculatedPercentageWithLength())
                size = primitiveValue->cssCalcValue()->toCalcValue(conversionData)->evaluate(parentSize);
            else
                ASSERT_NOT_REACHED();
            m_fontSizehasViewportUnits = parentStyle->hasViewportUnits();
            parentStyle->setHasViewportUnits(parentHasViewportUnits);
        }
    }

    if (size < 0)
        return;

    // Overly large font sizes will cause crashes on some platforms (such as Windows).
    // Cap font size here to make sure that doesn't happen.
    size = std::min(maximumAllowedFontSize, size);

    setSize(scope.fontDescription(), effectiveZoom, size);
}

void FontBuilder::setWeight(FontWeight fontWeight)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setWeight(fontWeight);
}

void FontBuilder::setWeightBolder()
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setWeight(scope.fontDescription().bolderWeight());
}

void FontBuilder::setWeightLighter()
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setWeight(scope.fontDescription().lighterWeight());
}

void FontBuilder::setFontVariantLigaturesInitial()
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setCommonLigaturesState(FontDescription::NormalLigaturesState);
    scope.fontDescription().setDiscretionaryLigaturesState(FontDescription::NormalLigaturesState);
    scope.fontDescription().setHistoricalLigaturesState(FontDescription::NormalLigaturesState);
}

void FontBuilder::setFontVariantLigaturesInherit(const FontDescription& parentFontDescription)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setCommonLigaturesState(parentFontDescription.commonLigaturesState());
    scope.fontDescription().setDiscretionaryLigaturesState(parentFontDescription.discretionaryLigaturesState());
    scope.fontDescription().setHistoricalLigaturesState(parentFontDescription.historicalLigaturesState());
}

void FontBuilder::setFontVariantLigaturesValue(CSSValue* value)
{
    FontDescriptionChangeScope scope(this);

    FontDescription::LigaturesState commonLigaturesState = FontDescription::NormalLigaturesState;
    FontDescription::LigaturesState discretionaryLigaturesState = FontDescription::NormalLigaturesState;
    FontDescription::LigaturesState historicalLigaturesState = FontDescription::NormalLigaturesState;

    if (value->isValueList()) {
        CSSValueList* valueList = toCSSValueList(value);
        for (size_t i = 0; i < valueList->length(); ++i) {
            CSSValue* item = valueList->itemWithoutBoundsCheck(i);
            ASSERT(item->isPrimitiveValue());
            if (item->isPrimitiveValue()) {
                CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(item);
                switch (primitiveValue->getValueID()) {
                case CSSValueNoCommonLigatures:
                    commonLigaturesState = FontDescription::DisabledLigaturesState;
                    break;
                case CSSValueCommonLigatures:
                    commonLigaturesState = FontDescription::EnabledLigaturesState;
                    break;
                case CSSValueNoDiscretionaryLigatures:
                    discretionaryLigaturesState = FontDescription::DisabledLigaturesState;
                    break;
                case CSSValueDiscretionaryLigatures:
                    discretionaryLigaturesState = FontDescription::EnabledLigaturesState;
                    break;
                case CSSValueNoHistoricalLigatures:
                    historicalLigaturesState = FontDescription::DisabledLigaturesState;
                    break;
                case CSSValueHistoricalLigatures:
                    historicalLigaturesState = FontDescription::EnabledLigaturesState;
                    break;
                default:
                    ASSERT_NOT_REACHED();
                    break;
                }
            }
        }
    }
#if !ASSERT_DISABLED
    else {
        ASSERT_WITH_SECURITY_IMPLICATION(value->isPrimitiveValue());
        ASSERT(toCSSPrimitiveValue(value)->getValueID() == CSSValueNormal);
    }
#endif

    scope.fontDescription().setCommonLigaturesState(commonLigaturesState);
    scope.fontDescription().setDiscretionaryLigaturesState(discretionaryLigaturesState);
    scope.fontDescription().setHistoricalLigaturesState(historicalLigaturesState);
}

void FontBuilder::setScript(const String& locale)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setScript(localeToScriptCodeForFontSelection(locale));
}

void FontBuilder::setItalic(FontItalic italic)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setItalic(italic);
}

void FontBuilder::setSmallCaps(FontSmallCaps smallCaps)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setSmallCaps(smallCaps);
}

void FontBuilder::setTextRendering(TextRenderingMode textRenderingMode)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setTextRendering(textRenderingMode);
}

void FontBuilder::setKerning(FontDescription::Kerning kerning)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setKerning(kerning);
}

void FontBuilder::setFontSmoothing(FontSmoothingMode foontSmoothingMode)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setFontSmoothing(foontSmoothingMode);
}

void FontBuilder::setFeatureSettingsNormal()
{
    FontDescriptionChangeScope scope(this);

    // FIXME: Eliminate FontDescription::makeNormalFeatureSettings. It's useless.
    scope.set(scope.fontDescription().makeNormalFeatureSettings());
}

void FontBuilder::setFeatureSettingsValue(CSSValue* value)
{
    FontDescriptionChangeScope scope(this);

    CSSValueList* list = toCSSValueList(value);
    RefPtr<FontFeatureSettings> settings = FontFeatureSettings::create();
    int len = list->length();
    for (int i = 0; i < len; ++i) {
        CSSValue* item = list->itemWithoutBoundsCheck(i);
        if (!item->isFontFeatureValue())
            continue;
        CSSFontFeatureValue* feature = toCSSFontFeatureValue(item);
        settings->append(FontFeature(feature->tag(), feature->value()));
    }
    scope.fontDescription().setFeatureSettings(settings.release());
}

void FontBuilder::setSize(FontDescription& fontDescription, float effectiveZoom, float size)
{
    fontDescription.setSpecifiedSize(size);
    fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(fontDescription, effectiveZoom, size));
}

float FontBuilder::getComputedSizeFromSpecifiedSize(FontDescription& fontDescription, float effectiveZoom, float specifiedSize)
{
    float zoomFactor = 1.0f;
    if (!m_useSVGZoomRules) {
        zoomFactor = effectiveZoom;
        // FIXME: Why is this here!!!!?!
        if (LocalFrame* frame = m_document->frame())
            zoomFactor *= frame->textZoomFactor();
    }

    return FontSize::getComputedSizeFromSpecifiedSize(m_document, zoomFactor, fontDescription.isAbsoluteSize(), specifiedSize);
}

static void getFontAndGlyphOrientation(const RenderStyle* style, FontOrientation& fontOrientation, NonCJKGlyphOrientation& glyphOrientation)
{
    if (style->isHorizontalWritingMode()) {
        fontOrientation = Horizontal;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    }

    switch (style->textOrientation()) {
    case TextOrientationVerticalRight:
        fontOrientation = Vertical;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    case TextOrientationUpright:
        fontOrientation = Vertical;
        glyphOrientation = NonCJKGlyphOrientationUpright;
        return;
    case TextOrientationSideways:
        if (style->writingMode() == LeftToRightWritingMode) {
            // FIXME: This should map to sideways-left, which is not supported yet.
            fontOrientation = Vertical;
            glyphOrientation = NonCJKGlyphOrientationVerticalRight;
            return;
        }
        fontOrientation = Horizontal;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    case TextOrientationSidewaysRight:
        fontOrientation = Horizontal;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    default:
        ASSERT_NOT_REACHED();
        fontOrientation = Horizontal;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    }
}

void FontBuilder::checkForOrientationChange(RenderStyle* style)
{
    FontOrientation fontOrientation;
    NonCJKGlyphOrientation glyphOrientation;
    getFontAndGlyphOrientation(style, fontOrientation, glyphOrientation);

    FontDescriptionChangeScope scope(this);

    if (scope.fontDescription().orientation() == fontOrientation && scope.fontDescription().nonCJKGlyphOrientation() == glyphOrientation)
        return;

    scope.fontDescription().setNonCJKGlyphOrientation(glyphOrientation);
    scope.fontDescription().setOrientation(fontOrientation);
}

void FontBuilder::checkForGenericFamilyChange(RenderStyle* style, const RenderStyle* parentStyle)
{
    FontDescriptionChangeScope scope(this);

    if (scope.fontDescription().isAbsoluteSize() || !parentStyle)
        return;

    const FontDescription& parentFontDescription = parentStyle->fontDescription();
    if (scope.fontDescription().useFixedDefaultSize() == parentFontDescription.useFixedDefaultSize())
        return;

    // For now, lump all families but monospace together.
    if (scope.fontDescription().genericFamily() != FontDescription::MonospaceFamily
        && parentFontDescription.genericFamily() != FontDescription::MonospaceFamily)
        return;

    // We know the parent is monospace or the child is monospace, and that font
    // size was unspecified. We want to scale our font size as appropriate.
    // If the font uses a keyword size, then we refetch from the table rather than
    // multiplying by our scale factor.
    float size;
    if (scope.fontDescription().keywordSize()) {
        size = FontSize::fontSizeForKeyword(m_document, CSSValueXxSmall + scope.fontDescription().keywordSize() - 1, scope.fontDescription().useFixedDefaultSize());
    } else {
        Settings* settings = m_document->settings();
        float fixedScaleFactor = (settings && settings->defaultFixedFontSize() && settings->defaultFontSize())
            ? static_cast<float>(settings->defaultFixedFontSize()) / settings->defaultFontSize()
            : 1;
        size = parentFontDescription.useFixedDefaultSize() ?
            scope.fontDescription().specifiedSize() / fixedScaleFactor :
            scope.fontDescription().specifiedSize() * fixedScaleFactor;
    }

    setSize(scope.fontDescription(), style->effectiveZoom(), size);
}

void FontBuilder::checkForZoomChange(RenderStyle* style, const RenderStyle* parentStyle)
{
    FontDescriptionChangeScope scope(this);

    if (style->effectiveZoom() == parentStyle->effectiveZoom())
        return;

    setSize(scope.fontDescription(), style->effectiveZoom(), scope.fontDescription().specifiedSize());
}

// FIXME: style param should come first
void FontBuilder::createFont(PassRefPtr<FontSelector> fontSelector, const RenderStyle* parentStyle, RenderStyle* style)
{
    if (!m_fontDirty)
        return;

    checkForGenericFamilyChange(style, parentStyle);
    checkForZoomChange(style, parentStyle);
    checkForOrientationChange(style);
    style->font().update(fontSelector);
    m_fontDirty = false;
}

void FontBuilder::createFontForDocument(PassRefPtr<FontSelector> fontSelector, RenderStyle* documentStyle)
{
    FontDescription fontDescription = FontDescription();
    fontDescription.setScript(localeToScriptCodeForFontSelection(documentStyle->locale()));
    if (Settings* settings = m_document->settings()) {
        fontDescription.setUsePrinterFont(m_document->printing());
        const AtomicString& standardFont = settings->genericFontFamilySettings().standard(fontDescription.script());
        if (!standardFont.isEmpty()) {
            fontDescription.setGenericFamily(FontDescription::StandardFamily);
            fontDescription.firstFamily().setFamily(standardFont);
            fontDescription.firstFamily().appendFamily(nullptr);
        }
        fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
        int size = FontSize::fontSizeForKeyword(m_document, CSSValueMedium, false);
        fontDescription.setSpecifiedSize(size);
        fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(fontDescription, documentStyle->effectiveZoom(), size));
    } else {
        fontDescription.setUsePrinterFont(m_document->printing());
    }

    FontOrientation fontOrientation;
    NonCJKGlyphOrientation glyphOrientation;
    getFontAndGlyphOrientation(documentStyle, fontOrientation, glyphOrientation);
    fontDescription.setOrientation(fontOrientation);
    fontDescription.setNonCJKGlyphOrientation(glyphOrientation);
    documentStyle->setFontDescription(fontDescription);
    documentStyle->font().update(fontSelector);
}

}
