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

#include "core/CSSValueKeywords.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/TextAutosizer.h"
#include "platform/FontFamilyNames.h"
#include "platform/fonts/FontDescription.h"
#include "platform/text/LocaleToScriptMapping.h"

namespace blink {

// FIXME: This scoping class is a short-term fix to minimize the changes in
// Font-constructing logic.
class FontDescriptionChangeScope {
    STACK_ALLOCATED();
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
    RawPtrWillBeMember<FontBuilder> m_fontBuilder;
    FontDescription m_fontDescription;
};

FontBuilder::FontBuilder()
    : m_document(nullptr)
    , m_style(0)
    , m_fontDirty(false)
{
}

void FontBuilder::initForStyleResolve(const Document& document, RenderStyle* style)
{
    ASSERT(document.frame());
    m_document = &document;
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
    setFamilyDescription(scope.fontDescription(), FontBuilder::initialFamilyDescription());
    setSize(scope.fontDescription(), FontBuilder::initialSize());
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

    // Handle the zoom factor.
    fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(fontDescription, effectiveZoom, fontDescription.specifiedSize()));
    scope.set(fontDescription);
}

FontFamily FontBuilder::standardFontFamily() const
{
    FontFamily family;
    family.setFamily(standardFontFamilyName());
    return family;
}

AtomicString FontBuilder::standardFontFamilyName() const
{
    ASSERT(m_document);
    Settings* settings = m_document->settings();
    if (settings)
        return settings->genericFontFamilySettings().standard();
    return AtomicString();
}

AtomicString FontBuilder::genericFontFamilyName(FontDescription::GenericFamilyType genericFamily) const
{
    switch (genericFamily) {
    default:
        ASSERT_NOT_REACHED();
    case FontDescription::NoFamily:
        return AtomicString();
    case FontDescription::StandardFamily:
        return standardFontFamilyName();
    case FontDescription::SerifFamily:
        return FontFamilyNames::webkit_serif;
    case FontDescription::SansSerifFamily:
        return FontFamilyNames::webkit_sans_serif;
    case FontDescription::MonospaceFamily:
        return FontFamilyNames::webkit_monospace;
    case FontDescription::CursiveFamily:
        return FontFamilyNames::webkit_cursive;
    case FontDescription::FantasyFamily:
        return FontFamilyNames::webkit_fantasy;
    case FontDescription::PictographFamily:
        return FontFamilyNames::webkit_pictograph;
    }
}

void FontBuilder::setFamilyDescription(const FontDescription::FamilyDescription& familyDescription)
{
    FontDescriptionChangeScope scope(this);

    setFamilyDescription(scope.fontDescription(), familyDescription);
}

void FontBuilder::setWeight(FontWeight fontWeight)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setWeight(fontWeight);
}

void FontBuilder::setSize(const FontDescription::Size& size)
{
    FontDescriptionChangeScope scope(this);

    setSize(scope.fontDescription(), size);
}

void FontBuilder::setStretch(FontStretch fontStretch)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setStretch(fontStretch);
}

void FontBuilder::setScript(const String& locale)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setLocale(locale);
    scope.fontDescription().setScript(localeToScriptCodeForFontSelection(locale));
}

void FontBuilder::setStyle(FontStyle italic)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setStyle(italic);
}

void FontBuilder::setVariant(FontVariant smallCaps)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setVariant(smallCaps);
}

void FontBuilder::setVariantLigatures(const FontDescription::VariantLigatures& ligatures)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setVariantLigatures(ligatures);
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

void FontBuilder::setFeatureSettings(PassRefPtr<FontFeatureSettings> settings)
{
    FontDescriptionChangeScope scope(this);

    scope.fontDescription().setFeatureSettings(settings);
}

void FontBuilder::setFamilyDescription(FontDescription& fontDescription, const FontDescription::FamilyDescription& familyDescription)
{
    FixedPitchFontType oldFixedPitchFontType = fontDescription.fixedPitchFontType();

    bool isInitial = familyDescription.genericFamily == FontDescription::StandardFamily && familyDescription.family.familyIsEmpty();

    fontDescription.setGenericFamily(familyDescription.genericFamily);
    fontDescription.setFamily(isInitial ? standardFontFamily() : familyDescription.family);

    if (fontDescription.keywordSize() && fontDescription.fixedPitchFontType() != oldFixedPitchFontType)
        setSize(fontDescription, FontDescription::Size(fontDescription.keywordSize(), 0.0f, false));
}

void FontBuilder::setSize(FontDescription& fontDescription, const FontDescription::Size& size)
{
    float specifiedSize = size.value;

    if (!specifiedSize && size.keyword)
        specifiedSize = FontSize::fontSizeForKeyword(m_document, size.keyword, fontDescription.fixedPitchFontType());

    if (specifiedSize < 0)
        return;

    // Overly large font sizes will cause crashes on some platforms (such as Windows).
    // Cap font size here to make sure that doesn't happen.
    specifiedSize = std::min(maximumAllowedFontSize, specifiedSize);

    fontDescription.setKeywordSize(size.keyword);
    fontDescription.setSpecifiedSize(specifiedSize);
    fontDescription.setIsAbsoluteSize(size.isAbsolute);
}

float FontBuilder::getComputedSizeFromSpecifiedSize(FontDescription& fontDescription, float effectiveZoom, float specifiedSize)
{
    float zoomFactor = effectiveZoom;
    // FIXME: Why is this here!!!!?!
    if (LocalFrame* frame = m_document->frame())
        zoomFactor *= frame->textZoomFactor();

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
    if (scope.fontDescription().fixedPitchFontType() == parentFontDescription.fixedPitchFontType())
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
        size = FontSize::fontSizeForKeyword(m_document, scope.fontDescription().keywordSize(), scope.fontDescription().fixedPitchFontType());
    } else {
        Settings* settings = m_document->settings();
        float fixedScaleFactor = (settings && settings->defaultFixedFontSize() && settings->defaultFontSize())
            ? static_cast<float>(settings->defaultFixedFontSize()) / settings->defaultFontSize()
            : 1;
        size = parentFontDescription.fixedPitchFontType() == FixedPitchFont ?
            scope.fontDescription().specifiedSize() / fixedScaleFactor :
            scope.fontDescription().specifiedSize() * fixedScaleFactor;
    }

    scope.fontDescription().setSpecifiedSize(size);
    updateComputedSize(scope.fontDescription(), style);
}

void FontBuilder::updateComputedSize(RenderStyle* style, const RenderStyle* parentStyle)
{
    FontDescriptionChangeScope scope(this);
    updateComputedSize(scope.fontDescription(), style);
}

void FontBuilder::updateComputedSize(FontDescription& fontDescription, RenderStyle* style)
{
    float computedSize = getComputedSizeFromSpecifiedSize(fontDescription, style->effectiveZoom(), fontDescription.specifiedSize());
    float multiplier = style->textAutosizingMultiplier();
    if (multiplier > 1)
        computedSize = TextAutosizer::computeAutosizedFontSize(computedSize, multiplier);
    fontDescription.setComputedSize(computedSize);
}

// FIXME: style param should come first
void FontBuilder::createFont(PassRefPtrWillBeRawPtr<FontSelector> fontSelector, const RenderStyle* parentStyle, RenderStyle* style)
{
    if (!m_fontDirty)
        return;

    updateComputedSize(style, parentStyle);
    checkForGenericFamilyChange(style, parentStyle);
    checkForOrientationChange(style);
    style->font().update(fontSelector);
    m_fontDirty = false;
}

void FontBuilder::createFontForDocument(PassRefPtrWillBeRawPtr<FontSelector> fontSelector, RenderStyle* documentStyle)
{
    FontDescription fontDescription = FontDescription();
    fontDescription.setLocale(documentStyle->locale());
    fontDescription.setScript(localeToScriptCodeForFontSelection(documentStyle->locale()));

    setFamilyDescription(fontDescription, FontBuilder::initialFamilyDescription());
    setSize(fontDescription, FontDescription::Size(FontSize::initialKeywordSize(), 0.0f, false));
    updateComputedSize(fontDescription, documentStyle);

    FontOrientation fontOrientation;
    NonCJKGlyphOrientation glyphOrientation;
    getFontAndGlyphOrientation(documentStyle, fontOrientation, glyphOrientation);
    fontDescription.setOrientation(fontOrientation);
    fontDescription.setNonCJKGlyphOrientation(glyphOrientation);
    documentStyle->setFontDescription(fontDescription);
    documentStyle->font().update(fontSelector);
}

}
