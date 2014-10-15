/*
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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
 */

#include "config.h"

#if ENABLE(SVG_FONTS)
#include "core/rendering/svg/SVGTextRunRenderingContext.h"

#include "core/rendering/RenderObject.h"
#include "core/svg/SVGFontData.h"
#include "core/svg/SVGFontElement.h"
#include "core/svg/SVGFontFaceElement.h"
#include "core/svg/SVGGlyphElement.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/shaping/SimpleShaper.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

static inline const SVGFontData* svgFontAndFontFaceElementForFontData(const SimpleFontData* fontData, SVGFontFaceElement*& fontFace, SVGFontElement*& font)
{
    ASSERT(fontData);
    ASSERT(fontData->isCustomFont());
    ASSERT(fontData->isSVGFont());

    RefPtr<CustomFontData> customFontData = fontData->customFontData();
    const SVGFontData* svgFontData = toSVGFontData(customFontData);

    // FIXME crbug.com/359380 : The current editing impl references the font after the svg font nodes are removed.
    if (svgFontData->shouldSkipDrawing())
        return 0;

    fontFace = svgFontData->svgFontFaceElement();
    ASSERT(fontFace);

    font = fontFace->associatedFontElement();
    return svgFontData;
}

static inline RenderObject* firstParentRendererForNonTextNode(RenderObject* renderer)
{
    ASSERT(renderer);
    return renderer->isText() ? renderer->parent() : renderer;
}

static inline RenderObject* renderObjectFromRun(const TextRun& run)
{
    if (TextRun::RenderingContext* renderingContext = run.renderingContext())
        return static_cast<SVGTextRunRenderingContext*>(renderingContext)->renderer();
    return 0;
}

float SVGTextRunRenderingContext::floatWidthUsingSVGFont(const Font& font, const TextRun& run, int& charsConsumed, Glyph& glyphId) const
{
    SimpleShaper it(&font, run);
    GlyphBuffer glyphBuffer;
    charsConsumed += it.advance(run.length(), &glyphBuffer);
    glyphId = !glyphBuffer.isEmpty() ? glyphBuffer.glyphAt(0) : 0;
    return it.runWidthSoFar();
}

void SVGTextRunRenderingContext::drawSVGGlyphs(GraphicsContext* context, const TextRun& run, const SimpleFontData* fontData, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point) const
{
    SVGFontElement* fontElement = 0;
    SVGFontFaceElement* fontFaceElement = 0;

    const SVGFontData* svgFontData = svgFontAndFontFaceElementForFontData(fontData, fontFaceElement, fontElement);
    if (!fontElement || !fontFaceElement)
        return;

    // We can only paint SVGFonts if a context is available.
    RenderObject* renderObject = renderObjectFromRun(run);
    ASSERT(renderObject);

    bool isVerticalText = false;
    if (RenderObject* parentRenderObject = firstParentRendererForNonTextNode(renderObject)) {
        RenderStyle* parentRenderObjectStyle = parentRenderObject->style();
        ASSERT(parentRenderObjectStyle);
        isVerticalText = parentRenderObjectStyle->svgStyle().isVerticalWritingMode();
    }

    float scale = scaleEmToUnits(fontData->platformData().size(), fontFaceElement->unitsPerEm());

    FloatPoint glyphOrigin;
    glyphOrigin.setX(svgFontData->horizontalOriginX() * scale);
    glyphOrigin.setY(svgFontData->horizontalOriginY() * scale);

    FloatPoint currentPoint = point;
    for (int i = 0; i < numGlyphs; ++i) {
        Glyph glyph = glyphBuffer.glyphAt(from + i);
        if (!glyph)
            continue;

        float advance = glyphBuffer.advanceAt(from + i);
        SVGGlyph svgGlyph = fontElement->svgGlyphForGlyph(glyph);
        ASSERT(!svgGlyph.isPartOfLigature);
        ASSERT(svgGlyph.tableEntry == glyph);

        SVGGlyphElement::inheritUnspecifiedAttributes(svgGlyph, svgFontData);

        // FIXME: Support arbitary SVG content as glyph (currently limited to <glyph d="..."> situations).
        if (svgGlyph.pathData.isEmpty()) {
            if (isVerticalText)
                currentPoint.move(0, advance);
            else
                currentPoint.move(advance, 0);
            continue;
         }

        if (isVerticalText) {
            glyphOrigin.setX(svgGlyph.verticalOriginX * scale);
            glyphOrigin.setY(svgGlyph.verticalOriginY * scale);
         }

        AffineTransform glyphPathTransform;
        glyphPathTransform.translate(currentPoint.x() + glyphOrigin.x(), currentPoint.y() + glyphOrigin.y());
        glyphPathTransform.scale(scale, -scale);

        Path glyphPath = svgGlyph.pathData;
        glyphPath.transform(glyphPathTransform);

        if (context->textDrawingMode() == TextModeStroke)
            context->strokePath(glyphPath);
        else
            context->fillPath(glyphPath);

        if (isVerticalText)
            currentPoint.move(0, advance);
        else
            currentPoint.move(advance, 0);
    }
}

GlyphData SVGTextRunRenderingContext::glyphDataForCharacter(const Font& font, const TextRun& run, SimpleShaper& iterator, UChar32 character, bool mirror, int currentCharacter, unsigned& advanceLength)
{
    const SimpleFontData* primaryFont = font.primaryFont();
    ASSERT(primaryFont);

    pair<GlyphData, GlyphPage*> pair = font.glyphDataAndPageForCharacter(character, mirror);
    GlyphData glyphData = pair.first;

    // Check if we have the missing glyph data, in which case we can just return.
    GlyphData missingGlyphData = primaryFont->missingGlyphData();
    if (glyphData.glyph == missingGlyphData.glyph && glyphData.fontData == missingGlyphData.fontData) {
        ASSERT(glyphData.fontData);
        return glyphData;
    }

    // Save data fromt he font fallback list because we may modify it later. Do this before the
    // potential change to glyphData.fontData below.
    FontFallbackList* fontList = font.fontList();
    ASSERT(fontList);
    FontFallbackList::GlyphPagesStateSaver glyphPagesSaver(*fontList);

    // Characters enclosed by an <altGlyph> element, may not be registered in the GlyphPage.
    const SimpleFontData* originalFontData = glyphData.fontData;
    if (originalFontData && !originalFontData->isSVGFont()) {
        if (TextRun::RenderingContext* renderingContext = run.renderingContext()) {
            RenderObject* renderObject = static_cast<SVGTextRunRenderingContext*>(renderingContext)->renderer();
            RenderObject* parentRenderObject = renderObject->isText() ? renderObject->parent() : renderObject;
            ASSERT(parentRenderObject);
            if (Element* parentRenderObjectElement = toElement(parentRenderObject->node())) {
                if (isSVGAltGlyphElement(*parentRenderObjectElement))
                    glyphData.fontData = primaryFont;
            }
        }
    }

    const SimpleFontData* fontData = glyphData.fontData;
    if (fontData) {
        if (!fontData->isSVGFont())
            return glyphData;

        SVGFontElement* fontElement = 0;
        SVGFontFaceElement* fontFaceElement = 0;

        const SVGFontData* svgFontData = svgFontAndFontFaceElementForFontData(fontData, fontFaceElement, fontElement);
        if (!fontElement || !fontFaceElement)
            return glyphData;

        // If we got here, we're dealing with a glyph defined in a SVG Font.
        // The returned glyph by glyphDataAndPageForCharacter() is a glyph stored in the SVG Font glyph table.
        // This doesn't necessarily mean the glyph is suitable for rendering/measuring in this context, its
        // arabic-form/orientation/... may not match, we have to apply SVG Glyph selection to discover that.
        if (svgFontData->applySVGGlyphSelection(iterator, glyphData, mirror, currentCharacter, advanceLength))
            return glyphData;
    }

    GlyphPage* page = pair.second;
    ASSERT(page);

    // No suitable glyph found that is compatible with the requirments (same language, arabic-form, orientation etc.)
    // Even though our GlyphPage contains an entry for eg. glyph "a", it's not compatible. So we have to temporarily
    // remove the glyph data information from the GlyphPage, and retry the lookup, which handles font fallbacks correctly.
    page->setGlyphDataForCharacter(character, 0, 0);

    // Assure that the font fallback glyph selection worked, aka. the fallbackGlyphData font data is not the same as before.
    GlyphData fallbackGlyphData = font.glyphDataForCharacter(character, mirror);
    ASSERT(fallbackGlyphData.fontData != fontData);

    // Restore original state of the SVG Font glyph table and the current font fallback list,
    // to assure the next lookup of the same glyph won't immediately return the fallback glyph.
    page->setGlyphDataForCharacter(character, glyphData.glyph, originalFontData);
    ASSERT(fallbackGlyphData.fontData);
    return fallbackGlyphData;
}

}

#endif
