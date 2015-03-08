// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGInlineTextBoxPainter.h"

#include "core/dom/DocumentMarkerController.h"
#include "core/dom/RenderedDocumentMarker.h"
#include "core/editing/Editor.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/PaintInfo.h"
#include "core/layout/style/ShadowList.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "core/paint/InlinePainter.h"
#include "core/paint/InlineTextBoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"

namespace blink {

static inline bool textShouldBePainted(LayoutSVGInlineText& textRenderer)
{
    // Font::pixelSize(), returns FontDescription::computedPixelSize(), which returns "int(x + 0.5)".
    // If the absolute font size on screen is below x=0.5, don't render anything.
    return textRenderer.scaledFont().fontDescription().computedPixelSize();
}

bool SVGInlineTextBoxPainter::shouldPaintSelection() const
{
    bool isPrinting = m_svgInlineTextBox.layoutObject().document().printing();
    return !isPrinting && m_svgInlineTextBox.selectionState() != LayoutObject::SelectionNone;
}

void SVGInlineTextBoxPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.shouldPaintWithinRoot(&m_svgInlineTextBox.layoutObject()));
    ASSERT(paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection);
    ASSERT(m_svgInlineTextBox.truncation() == cNoTruncation);

    if (m_svgInlineTextBox.layoutObject().style()->visibility() != VISIBLE)
        return;

    // We're explicitly not supporting composition & custom underlines and custom highlighters -- unlike InlineTextBox.
    // If we ever need that for SVG, it's very easy to refactor and reuse the code.

    if (paintInfo.phase == PaintPhaseSelection && !shouldPaintSelection())
        return;

    LayoutSVGInlineText& textRenderer = toLayoutSVGInlineText(m_svgInlineTextBox.layoutObject());
    if (!textShouldBePainted(textRenderer))
        return;

    LayoutObject& parentRenderer = m_svgInlineTextBox.parent()->layoutObject();
    const LayoutStyle& style = parentRenderer.styleRef();

    InlineTextBoxPainter(m_svgInlineTextBox).paintDocumentMarkers(
        paintInfo.context, FloatPoint(paintOffset), style,
        textRenderer.scaledFont(), true);

    if (!m_svgInlineTextBox.textFragments().isEmpty()) {
        DrawingRecorder recorder(paintInfo.context, m_svgInlineTextBox.displayItemClient(), DisplayItem::paintPhaseToDrawingType(paintInfo.phase), paintInfo.rect);
        if (!recorder.canUseCachedDrawing())
            paintTextFragments(paintInfo, parentRenderer);
    }

    if (style.hasOutline() && parentRenderer.isLayoutInline())
        InlinePainter(toLayoutInline(parentRenderer)).paintOutline(paintInfo, paintOffset);
}

void SVGInlineTextBoxPainter::paintTextFragments(const PaintInfo& paintInfo, LayoutObject& parentRenderer)
{
    const LayoutStyle& style = parentRenderer.styleRef();
    const SVGLayoutStyle& svgStyle = style.svgStyle();

    bool hasFill = svgStyle.hasFill();
    bool hasVisibleStroke = svgStyle.hasVisibleStroke();

    const LayoutStyle* selectionStyle = &style;
    bool shouldPaintSelection = this->shouldPaintSelection();
    if (shouldPaintSelection) {
        selectionStyle = parentRenderer.getCachedPseudoStyle(SELECTION);
        if (selectionStyle) {
            const SVGLayoutStyle& svgSelectionStyle = selectionStyle->svgStyle();

            if (!hasFill)
                hasFill = svgSelectionStyle.hasFill();
            if (!hasVisibleStroke)
                hasVisibleStroke = svgSelectionStyle.hasVisibleStroke();
        } else {
            selectionStyle = &style;
        }
    }

    if (paintInfo.isRenderingClipPathAsMaskImage()) {
        hasFill = true;
        hasVisibleStroke = false;
    }

    AffineTransform fragmentTransform;
    unsigned textFragmentsSize = m_svgInlineTextBox.textFragments().size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        SVGTextFragment& fragment = m_svgInlineTextBox.textFragments().at(i);

        GraphicsContextStateSaver stateSaver(*paintInfo.context, false);
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity()) {
            stateSaver.save();
            paintInfo.context->concatCTM(fragmentTransform);
        }

        // Spec: All text decorations except line-through should be drawn before the text is filled and stroked; thus, the text is rendered on top of these decorations.
        unsigned decorations = style.textDecorationsInEffect();
        if (decorations & TextDecorationUnderline)
            paintDecoration(paintInfo, TextDecorationUnderline, fragment);
        if (decorations & TextDecorationOverline)
            paintDecoration(paintInfo, TextDecorationOverline, fragment);

        for (int i = 0; i < 3; i++) {
            switch (svgStyle.paintOrderType(i)) {
            case PT_FILL:
                if (hasFill)
                    paintText(paintInfo, style, *selectionStyle, fragment, ApplyToFillMode, shouldPaintSelection);
                break;
            case PT_STROKE:
                if (hasVisibleStroke)
                    paintText(paintInfo, style, *selectionStyle, fragment, ApplyToStrokeMode, shouldPaintSelection);
                break;
            case PT_MARKERS:
                // Markers don't apply to text
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }

        // Spec: Line-through should be drawn after the text is filled and stroked; thus, the line-through is rendered on top of the text.
        if (decorations & TextDecorationLineThrough)
            paintDecoration(paintInfo, TextDecorationLineThrough, fragment);
    }
}

void SVGInlineTextBoxPainter::paintSelectionBackground(const PaintInfo& paintInfo)
{
    if (m_svgInlineTextBox.layoutObject().style()->visibility() != VISIBLE)
        return;

    ASSERT(!m_svgInlineTextBox.layoutObject().document().printing());

    if (paintInfo.phase == PaintPhaseSelection || !shouldPaintSelection())
        return;

    Color backgroundColor = m_svgInlineTextBox.layoutObject().selectionBackgroundColor();
    if (!backgroundColor.alpha())
        return;

    LayoutSVGInlineText& textRenderer = toLayoutSVGInlineText(m_svgInlineTextBox.layoutObject());
    if (!textShouldBePainted(textRenderer))
        return;

    const LayoutStyle& style = m_svgInlineTextBox.parent()->layoutObject().styleRef();

    int startPosition, endPosition;
    m_svgInlineTextBox.selectionStartEnd(startPosition, endPosition);

    int fragmentStartPosition = 0;
    int fragmentEndPosition = 0;
    AffineTransform fragmentTransform;
    unsigned textFragmentsSize = m_svgInlineTextBox.textFragments().size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        SVGTextFragment& fragment = m_svgInlineTextBox.textFragments().at(i);

        fragmentStartPosition = startPosition;
        fragmentEndPosition = endPosition;
        if (!m_svgInlineTextBox.mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        GraphicsContextStateSaver stateSaver(*paintInfo.context);
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            paintInfo.context->concatCTM(fragmentTransform);

        paintInfo.context->setFillColor(backgroundColor);
        paintInfo.context->fillRect(m_svgInlineTextBox.selectionRectForTextFragment(fragment, fragmentStartPosition, fragmentEndPosition, style), backgroundColor);
    }
}

static inline LayoutObject* findLayoutObjectDefininingTextDecoration(InlineFlowBox* parentBox)
{
    // Lookup first render object in parent hierarchy which has text-decoration set.
    LayoutObject* renderer = 0;
    while (parentBox) {
        renderer = &parentBox->layoutObject();

        if (renderer->style() && renderer->style()->textDecoration() != TextDecorationNone)
            break;

        parentBox = parentBox->parent();
    }

    ASSERT(renderer);
    return renderer;
}

// Offset from the baseline for |decoration|. Positive offsets are above the baseline.
static inline float baselineOffsetForDecoration(TextDecoration decoration, const FontMetrics& fontMetrics, float thickness)
{
    // FIXME: For SVG Fonts we need to use the attributes defined in the <font-face> if specified.
    // Compatible with Batik/Presto.
    if (decoration == TextDecorationUnderline)
        return -thickness * 1.5f;
    if (decoration == TextDecorationOverline)
        return fontMetrics.floatAscent() - thickness;
    if (decoration == TextDecorationLineThrough)
        return fontMetrics.floatAscent() * 3 / 8.0f;

    ASSERT_NOT_REACHED();
    return 0.0f;
}

static inline float thicknessForDecoration(TextDecoration, const Font& font)
{
    // FIXME: For SVG Fonts we need to use the attributes defined in the <font-face> if specified.
    // Compatible with Batik/Presto
    return font.fontDescription().computedSize() / 20.0f;
}

void SVGInlineTextBoxPainter::paintDecoration(const PaintInfo& paintInfo, TextDecoration decoration, const SVGTextFragment& fragment)
{
    if (m_svgInlineTextBox.layoutObject().style()->textDecorationsInEffect() == TextDecorationNone)
        return;

    if (fragment.width <= 0)
        return;

    // Find out which render style defined the text-decoration, as its fill/stroke properties have to be used for drawing instead of ours.
    LayoutObject* decorationRenderer = findLayoutObjectDefininingTextDecoration(m_svgInlineTextBox.parent());
    const LayoutStyle& decorationStyle = decorationRenderer->styleRef();

    if (decorationStyle.visibility() == HIDDEN)
        return;

    float scalingFactor = 1;
    Font scaledFont;
    LayoutSVGInlineText::computeNewScaledFontForStyle(decorationRenderer, &decorationStyle, scalingFactor, scaledFont);
    ASSERT(scalingFactor);

    float thickness = thicknessForDecoration(decoration, scaledFont);
    if (thickness <= 0)
        return;

    float decorationOffset = baselineOffsetForDecoration(decoration, scaledFont.fontMetrics(), thickness);
    FloatPoint decorationOrigin(fragment.x, fragment.y - decorationOffset / scalingFactor);

    Path path;
    path.addRect(FloatRect(decorationOrigin, FloatSize(fragment.width, thickness / scalingFactor)));

    const SVGLayoutStyle& svgDecorationStyle = decorationStyle.svgStyle();

    for (int i = 0; i < 3; i++) {
        switch (svgDecorationStyle.paintOrderType(i)) {
        case PT_FILL:
            if (svgDecorationStyle.hasFill()) {
                GraphicsContextStateSaver stateSaver(*paintInfo.context, false);
                if (!SVGLayoutSupport::updateGraphicsContext(paintInfo, stateSaver, decorationStyle, *decorationRenderer, ApplyToFillMode))
                    break;
                paintInfo.context->fillPath(path);
            }
            break;
        case PT_STROKE:
            if (svgDecorationStyle.hasVisibleStroke()) {
                // FIXME: Non-scaling stroke is not applied here.
                GraphicsContextStateSaver stateSaver(*paintInfo.context, false);
                if (!SVGLayoutSupport::updateGraphicsContext(paintInfo, stateSaver, decorationStyle, *decorationRenderer, ApplyToStrokeMode))
                    break;
                paintInfo.context->strokePath(path);
            }
            break;
        case PT_MARKERS:
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

void SVGInlineTextBoxPainter::paintTextWithShadows(const PaintInfo& paintInfo, const LayoutStyle& style,
    TextRun& textRun, const SVGTextFragment& fragment, int startPosition, int endPosition,
    LayoutSVGResourceMode resourceMode)
{
    LayoutSVGInlineText& textRenderer = toLayoutSVGInlineText(m_svgInlineTextBox.layoutObject());

    float scalingFactor = textRenderer.scalingFactor();
    ASSERT(scalingFactor);

    const Font& scaledFont = textRenderer.scaledFont();
    const ShadowList* shadowList = style.textShadow();
    GraphicsContext* context = paintInfo.context;

    // Text shadows are disabled when printing. http://crbug.com/258321
    bool hasShadow = shadowList && !context->printing();

    FloatPoint textOrigin(fragment.x, fragment.y);
    FloatSize textSize(fragment.width, fragment.height);
    AffineTransform paintServerTransform;
    const AffineTransform* additionalPaintServerTransform = 0;

    GraphicsContextStateSaver stateSaver(*context, false);
    if (scalingFactor != 1) {
        textOrigin.scale(scalingFactor, scalingFactor);
        textSize.scale(scalingFactor);
        stateSaver.save();
        context->scale(1 / scalingFactor, 1 / scalingFactor);
        // Adjust the paint-server coordinate space.
        paintServerTransform.scale(scalingFactor);
        additionalPaintServerTransform = &paintServerTransform;
    }

    // FIXME: Non-scaling stroke is not applied here.

    if (!SVGLayoutSupport::updateGraphicsContext(paintInfo, stateSaver, style, m_svgInlineTextBox.parent()->layoutObject(), resourceMode, additionalPaintServerTransform))
        return;

    if (hasShadow) {
        stateSaver.saveIfNeeded();
        context->setDrawLooper(shadowList->createDrawLooper(DrawLooperBuilder::ShadowRespectsAlpha));
    }

    context->setTextDrawingMode(resourceMode == ApplyToFillMode ? TextModeFill : TextModeStroke);

    if (scalingFactor != 1 && resourceMode == ApplyToStrokeMode)
        context->setStrokeThickness(context->strokeThickness() * scalingFactor);

    TextRunPaintInfo textRunPaintInfo(textRun);
    textRunPaintInfo.from = startPosition;
    textRunPaintInfo.to = endPosition;

    float baseline = scaledFont.fontMetrics().floatAscent();
    textRunPaintInfo.bounds = FloatRect(textOrigin.x(), textOrigin.y() - baseline,
        textSize.width(), textSize.height());

    scaledFont.drawText(context, textRunPaintInfo, textOrigin);
}

void SVGInlineTextBoxPainter::paintText(const PaintInfo& paintInfo, const LayoutStyle& style,
    const LayoutStyle& selectionStyle, const SVGTextFragment& fragment,
    LayoutSVGResourceMode resourceMode, bool shouldPaintSelection)
{
    int startPosition = 0;
    int endPosition = 0;
    if (shouldPaintSelection) {
        m_svgInlineTextBox.selectionStartEnd(startPosition, endPosition);
        shouldPaintSelection = m_svgInlineTextBox.mapStartEndPositionsIntoFragmentCoordinates(fragment, startPosition, endPosition);
    }

    // Fast path if there is no selection, just draw the whole chunk part using the regular style
    TextRun textRun = m_svgInlineTextBox.constructTextRun(style, fragment);
    if (!shouldPaintSelection || startPosition >= endPosition) {
        paintTextWithShadows(paintInfo, style, textRun, fragment, 0, fragment.length, resourceMode);
        return;
    }

    // Eventually draw text using regular style until the start position of the selection
    bool paintSelectedTextOnly = paintInfo.phase == PaintPhaseSelection;
    if (startPosition > 0 && !paintSelectedTextOnly)
        paintTextWithShadows(paintInfo, style, textRun, fragment, 0, startPosition, resourceMode);

    // Draw text using selection style from the start to the end position of the selection
    if (style != selectionStyle) {
        StyleDifference diff;
        diff.setNeedsPaintInvalidationObject();
        SVGResourcesCache::clientStyleChanged(&m_svgInlineTextBox.parent()->layoutObject(), diff, selectionStyle);
    }

    paintTextWithShadows(paintInfo, selectionStyle, textRun, fragment, startPosition, endPosition, resourceMode);

    if (style != selectionStyle) {
        StyleDifference diff;
        diff.setNeedsPaintInvalidationObject();
        SVGResourcesCache::clientStyleChanged(&m_svgInlineTextBox.parent()->layoutObject(), diff, style);
    }

    // Eventually draw text using regular style from the end position of the selection to the end of the current chunk part
    if (endPosition < static_cast<int>(fragment.length) && !paintSelectedTextOnly)
        paintTextWithShadows(paintInfo, style, textRun, fragment, endPosition, fragment.length, resourceMode);
}

void SVGInlineTextBoxPainter::paintTextMatchMarker(GraphicsContext* context, const FloatPoint&, DocumentMarker* marker, const LayoutStyle& style, const Font& font)
{
    // SVG is only interested in the TextMatch markers.
    if (marker->type() != DocumentMarker::TextMatch)
        return;

    LayoutSVGInlineText& textRenderer = toLayoutSVGInlineText(m_svgInlineTextBox.layoutObject());

    FloatRect markerRect;
    AffineTransform fragmentTransform;
    for (InlineTextBox* box = textRenderer.firstTextBox(); box; box = box->nextTextBox()) {
        if (!box->isSVGInlineTextBox())
            continue;

        SVGInlineTextBox* textBox = toSVGInlineTextBox(box);

        int markerStartPosition = std::max<int>(marker->startOffset() - textBox->start(), 0);
        int markerEndPosition = std::min<int>(marker->endOffset() - textBox->start(), textBox->len());

        if (markerStartPosition >= markerEndPosition)
            continue;

        const Vector<SVGTextFragment>& fragments = textBox->textFragments();
        unsigned textFragmentsSize = fragments.size();
        for (unsigned i = 0; i < textFragmentsSize; ++i) {
            const SVGTextFragment& fragment = fragments.at(i);

            int fragmentStartPosition = markerStartPosition;
            int fragmentEndPosition = markerEndPosition;
            if (!textBox->mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
                continue;

            FloatRect fragmentRect = textBox->selectionRectForTextFragment(fragment, fragmentStartPosition, fragmentEndPosition, style);
            fragment.buildFragmentTransform(fragmentTransform);

            // Draw the marker highlight.
            if (m_svgInlineTextBox.layoutObject().frame()->editor().markedTextMatchesAreHighlighted()) {
                Color color = marker->activeMatch() ?
                    LayoutTheme::theme().platformActiveTextSearchHighlightColor() :
                    LayoutTheme::theme().platformInactiveTextSearchHighlightColor();
                GraphicsContextStateSaver stateSaver(*context);
                if (!fragmentTransform.isIdentity())
                    context->concatCTM(fragmentTransform);
                context->setFillColor(color);
                context->fillRect(fragmentRect, color);
            }

            fragmentRect = fragmentTransform.mapRect(fragmentRect);
            markerRect.unite(fragmentRect);
        }
    }

    toRenderedDocumentMarker(marker)->setRenderedRect(LayoutRect(textRenderer.localToAbsoluteQuad(markerRect).enclosingBoundingBox()));
}

} // namespace blink
