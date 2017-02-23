// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/InlineTextBoxPainter.h"

#include "core/editing/CompositionUnderline.h"
#include "core/editing/Editor.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutTextCombine.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/LineLayoutBox.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TextPainter.h"
#include "core/style/AppliedTextDecoration.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "wtf/Optional.h"

namespace blink {

namespace {

std::pair<unsigned, unsigned> GetMarkerPaintOffsets(
    const DocumentMarker& marker,
    const InlineTextBox& textBox) {
  const unsigned startOffset = marker.startOffset() > textBox.start()
                                   ? marker.startOffset() - textBox.start()
                                   : 0U;
  const unsigned endOffset =
      std::min(marker.endOffset() - textBox.start(), textBox.len());
  return std::make_pair(startOffset, endOffset);
}
}

enum class ResolvedUnderlinePosition { Roman, Under, Over };

static ResolvedUnderlinePosition resolveUnderlinePosition(
    const ComputedStyle& style,
    const InlineTextBox* inlineTextBox) {
  // |auto| should resolve to |under| to avoid drawing through glyphs in
  // scripts where it would not be appropriate (e.g., ideographs.)
  // However, this has performance implications. For now, we only work with
  // vertical text.
  switch (inlineTextBox->root().baselineType()) {
    default:
      NOTREACHED();
    // Fall though.
    case AlphabeticBaseline:
      switch (style.getTextUnderlinePosition()) {
        default:
          NOTREACHED();
        // Fall though.
        case TextUnderlinePositionAuto:
          return ResolvedUnderlinePosition::Roman;
        case TextUnderlinePositionUnder:
          return ResolvedUnderlinePosition::Under;
      }
      break;
    case IdeographicBaseline:
      // Compute language-appropriate default underline position.
      // https://drafts.csswg.org/css-text-decor-3/#default-stylesheet
      UScriptCode script = style.getFontDescription().script();
      if (script == USCRIPT_KATAKANA_OR_HIRAGANA || script == USCRIPT_HANGUL)
        return ResolvedUnderlinePosition::Over;
      return ResolvedUnderlinePosition::Under;
  }
}

static LineLayoutItem enclosingUnderlineObject(
    const InlineTextBox* inlineTextBox) {
  bool firstLine = inlineTextBox->isFirstLineStyle();
  for (LineLayoutItem current = inlineTextBox->parent()->getLineLayoutItem();
       ;) {
    if (current.isLayoutBlock())
      return current;
    if (!current.isLayoutInline() || current.isRubyText())
      return nullptr;

    const ComputedStyle& styleToUse = current.styleRef(firstLine);
    if (styleToUse.getTextDecoration() & TextDecorationUnderline)
      return current;

    current = current.parent();
    if (!current)
      return current;

    if (Node* node = current.node()) {
      if (isHTMLAnchorElement(node) || node->hasTagName(HTMLNames::fontTag))
        return current;
    }
  }
}

static int computeUnderlineOffsetForUnder(const ComputedStyle& style,
                                          const InlineTextBox* inlineTextBox,
                                          bool isOverline = false) {
  const RootInlineBox& root = inlineTextBox->root();
  LineLayoutItem decorationObject = enclosingUnderlineObject(inlineTextBox);
  LayoutUnit offset;
  if (style.isFlippedLinesWritingMode()) {
    LayoutUnit position = inlineTextBox->logicalTop();
    offset =
        position - root.minLogicalTopForUnderline(decorationObject, position);
  } else {
    LayoutUnit position = inlineTextBox->logicalBottom();
    offset = root.maxLogicalBottomForUnderline(decorationObject, position) -
             position;
  }
  if (isOverline)
    return std::min(-offset, LayoutUnit()).toInt();
  return (inlineTextBox->logicalHeight() + std::max(offset, LayoutUnit()))
      .toInt();
}

static int computeOverlineOffset(const ComputedStyle& style,
                                 const InlineTextBox* inlineTextBox) {
  return computeUnderlineOffsetForUnder(style, inlineTextBox, true);
}

static int computeUnderlineOffsetForRoman(const FontMetrics& fontMetrics,
                                          const float textDecorationThickness) {
  // Compute the gap between the font and the underline. Use at least one
  // pixel gap, if underline is thick then use a bigger gap.
  int gap = 0;

  // Underline position of zero means draw underline on Baseline Position,
  // in Blink we need at least 1-pixel gap to adding following check.
  // Positive underline Position means underline should be drawn above baseline
  // and negative value means drawing below baseline, negating the value as in
  // Blink downward Y-increases.

  if (fontMetrics.underlinePosition())
    gap = -fontMetrics.underlinePosition();
  else
    gap = std::max<int>(1, ceilf(textDecorationThickness / 2.f));

  // Position underline near the alphabetic baseline.
  return fontMetrics.ascent() + gap;
}

static int computeUnderlineOffset(ResolvedUnderlinePosition underlinePosition,
                                  const ComputedStyle& style,
                                  const FontMetrics& fontMetrics,
                                  const InlineTextBox* inlineTextBox,
                                  const float textDecorationThickness) {
  switch (underlinePosition) {
    default:
      NOTREACHED();
    // Fall through.
    case ResolvedUnderlinePosition::Roman:
      return computeUnderlineOffsetForRoman(fontMetrics,
                                            textDecorationThickness);
    case ResolvedUnderlinePosition::Under:
      // Position underline at the under edge of the lowest element's
      // content box.
      return computeUnderlineOffsetForUnder(style, inlineTextBox);
  }
}

static bool shouldSetDecorationAntialias(
    const Vector<AppliedTextDecoration>& decorations) {
  for (const AppliedTextDecoration& decoration : decorations) {
    TextDecorationStyle decorationStyle = decoration.style();
    if (decorationStyle == TextDecorationStyleDotted ||
        decorationStyle == TextDecorationStyleDashed)
      return true;
  }
  return false;
}

static StrokeStyle textDecorationStyleToStrokeStyle(
    TextDecorationStyle decorationStyle) {
  StrokeStyle strokeStyle = SolidStroke;
  switch (decorationStyle) {
    case TextDecorationStyleSolid:
      strokeStyle = SolidStroke;
      break;
    case TextDecorationStyleDouble:
      strokeStyle = DoubleStroke;
      break;
    case TextDecorationStyleDotted:
      strokeStyle = DottedStroke;
      break;
    case TextDecorationStyleDashed:
      strokeStyle = DashedStroke;
      break;
    case TextDecorationStyleWavy:
      strokeStyle = WavyStroke;
      break;
  }

  return strokeStyle;
}

static void adjustStepToDecorationLength(float& step,
                                         float& controlPointDistance,
                                         float length) {
  DCHECK_GT(step, 0);

  if (length <= 0)
    return;

  unsigned stepCount = static_cast<unsigned>(length / step);

  // Each Bezier curve starts at the same pixel that the previous one
  // ended. We need to subtract (stepCount - 1) pixels when calculating the
  // length covered to account for that.
  float uncoveredLength = length - (stepCount * step - (stepCount - 1));
  float adjustment = uncoveredLength / stepCount;
  step += adjustment;
  controlPointDistance += adjustment;
}

class AppliedDecorationPainter final {
  STACK_ALLOCATED();

 public:
  AppliedDecorationPainter(GraphicsContext& context,
                           FloatPoint startPoint,
                           float width,
                           const AppliedTextDecoration& decoration,
                           float thickness,
                           float doubleOffset,
                           int wavyOffsetFactor,
                           bool antialiasDecoration)
      : m_context(context),
        m_startPoint(startPoint),
        m_width(width),
        m_decoration(decoration),
        m_thickness(thickness),
        m_doubleOffset(doubleOffset),
        m_wavyOffsetFactor(wavyOffsetFactor),
        m_shouldAntialias(antialiasDecoration){};

  void paint();
  FloatRect decorationBounds();

 private:
  void strokeWavyTextDecoration();

  Path prepareWavyStrokePath();
  Path prepareDottedDashedStrokePath();

  GraphicsContext& m_context;
  FloatPoint m_startPoint;
  float m_width;
  const AppliedTextDecoration& m_decoration;
  float m_thickness;
  const float m_doubleOffset;
  const int m_wavyOffsetFactor;
  bool m_shouldAntialias;
};

Path AppliedDecorationPainter::prepareDottedDashedStrokePath() {
  // These coordinate transforms need to match what's happening in
  // GraphicsContext's drawLineForText and drawLine.
  int y = floorf(m_startPoint.y() + std::max<float>(m_thickness / 2.0f, 0.5f));
  Path strokePath;
  FloatPoint roundedStartPoint(m_startPoint.x(), y);
  FloatPoint roundedEndPoint(roundedStartPoint + FloatPoint(m_width, 0));
  m_context.adjustLineToPixelBoundaries(roundedStartPoint, roundedEndPoint,
                                        roundf(m_thickness),
                                        m_context.getStrokeStyle());
  strokePath.moveTo(roundedStartPoint);
  strokePath.addLineTo(roundedEndPoint);
  return strokePath;
}

FloatRect AppliedDecorationPainter::decorationBounds() {
  StrokeData strokeData;
  strokeData.setThickness(m_thickness);

  switch (m_decoration.style()) {
    case TextDecorationStyleDotted:
    case TextDecorationStyleDashed: {
      strokeData.setStyle(
          textDecorationStyleToStrokeStyle(m_decoration.style()));
      return prepareDottedDashedStrokePath().strokeBoundingRect(
          strokeData, Path::BoundsType::Exact);
    }
    case TextDecorationStyleWavy:
      return prepareWavyStrokePath().strokeBoundingRect(
          strokeData, Path::BoundsType::Exact);
      break;
    case TextDecorationStyleDouble:
      if (m_doubleOffset > 0) {
        return FloatRect(m_startPoint.x(), m_startPoint.y(), m_width,
                         m_doubleOffset + m_thickness);
      }
      return FloatRect(m_startPoint.x(), m_startPoint.y() + m_doubleOffset,
                       m_width, -m_doubleOffset + m_thickness);
      break;
    case TextDecorationStyleSolid:
      return FloatRect(m_startPoint.x(), m_startPoint.y(), m_width,
                       m_thickness);
    default:
      break;
  }
  NOTREACHED();
  return FloatRect();
}

void AppliedDecorationPainter::paint() {
  m_context.setStrokeStyle(
      textDecorationStyleToStrokeStyle(m_decoration.style()));
  m_context.setStrokeColor(m_decoration.color());

  switch (m_decoration.style()) {
    case TextDecorationStyleWavy:
      strokeWavyTextDecoration();
      break;
    case TextDecorationStyleDotted:
    case TextDecorationStyleDashed:
      m_context.setShouldAntialias(m_shouldAntialias);
    // Fall through
    default:
      m_context.drawLineForText(m_startPoint, m_width);

      if (m_decoration.style() == TextDecorationStyleDouble) {
        m_context.drawLineForText(m_startPoint + FloatPoint(0, m_doubleOffset),
                                  m_width);
      }
  }
}

void AppliedDecorationPainter::strokeWavyTextDecoration() {
  m_context.setShouldAntialias(true);
  m_context.strokePath(prepareWavyStrokePath());
}

/*
 * Prepare a path for a cubic Bezier curve and repeat the same pattern long the
 * the decoration's axis.  The start point (p1), controlPoint1, controlPoint2
 * and end point (p2) of the Bezier curve form a diamond shape:
 *
 *                              step
 *                         |-----------|
 *
 *                   controlPoint1
 *                         +
 *
 *
 *                  . .
 *                .     .
 *              .         .
 * (x1, y1) p1 +           .            + p2 (x2, y2) - <--- Decoration's axis
 *                          .         .               |
 *                            .     .                 |
 *                              . .                   | controlPointDistance
 *                                                    |
 *                                                    |
 *                         +                          -
 *                   controlPoint2
 *
 *             |-----------|
 *                 step
 */
Path AppliedDecorationPainter::prepareWavyStrokePath() {
  FloatPoint p1(m_startPoint +
                FloatPoint(0, m_doubleOffset * m_wavyOffsetFactor));
  FloatPoint p2(m_startPoint +
                FloatPoint(m_width, m_doubleOffset * m_wavyOffsetFactor));

  m_context.adjustLineToPixelBoundaries(p1, p2, m_thickness,
                                        m_context.getStrokeStyle());

  Path path;
  path.moveTo(p1);

  // Distance between decoration's axis and Bezier curve's control points.
  // The height of the curve is based on this distance. Use a minimum of 6
  // pixels distance since
  // the actual curve passes approximately at half of that distance, that is 3
  // pixels.
  // The minimum height of the curve is also approximately 3 pixels. Increases
  // the curve's height
  // as strockThickness increases to make the curve looks better.
  float controlPointDistance = 3 * std::max<float>(2, m_thickness);

  // Increment used to form the diamond shape between start point (p1), control
  // points and end point (p2) along the axis of the decoration. Makes the
  // curve wider as strockThickness increases to make the curve looks better.
  float step = 2 * std::max<float>(2, m_thickness);

  bool isVerticalLine = (p1.x() == p2.x());

  if (isVerticalLine) {
    DCHECK(p1.x() == p2.x());

    float xAxis = p1.x();
    float y1;
    float y2;

    if (p1.y() < p2.y()) {
      y1 = p1.y();
      y2 = p2.y();
    } else {
      y1 = p2.y();
      y2 = p1.y();
    }

    adjustStepToDecorationLength(step, controlPointDistance, y2 - y1);
    FloatPoint controlPoint1(xAxis + controlPointDistance, 0);
    FloatPoint controlPoint2(xAxis - controlPointDistance, 0);

    for (float y = y1; y + 2 * step <= y2;) {
      controlPoint1.setY(y + step);
      controlPoint2.setY(y + step);
      y += 2 * step;
      path.addBezierCurveTo(controlPoint1, controlPoint2, FloatPoint(xAxis, y));
    }
  } else {
    DCHECK(p1.y() == p2.y());

    float yAxis = p1.y();
    float x1;
    float x2;

    if (p1.x() < p2.x()) {
      x1 = p1.x();
      x2 = p2.x();
    } else {
      x1 = p2.x();
      x2 = p1.x();
    }

    adjustStepToDecorationLength(step, controlPointDistance, x2 - x1);
    FloatPoint controlPoint1(0, yAxis + controlPointDistance);
    FloatPoint controlPoint2(0, yAxis - controlPointDistance);

    for (float x = x1; x + 2 * step <= x2;) {
      controlPoint1.setX(x + step);
      controlPoint2.setX(x + step);
      x += 2 * step;
      path.addBezierCurveTo(controlPoint1, controlPoint2, FloatPoint(x, yAxis));
    }
  }
  return path;
}

static const int misspellingLineThickness = 3;

LayoutObject& InlineTextBoxPainter::inlineLayoutObject() const {
  return *LineLayoutAPIShim::layoutObjectFrom(
      m_inlineTextBox.getLineLayoutItem());
}

bool InlineTextBoxPainter::paintsMarkerHighlights(
    const LayoutObject& layoutObject) {
  return layoutObject.node() &&
         layoutObject.document().markers().hasMarkers(layoutObject.node());
}

static bool paintsCompositionMarkers(const LayoutObject& layoutObject) {
  return layoutObject.node() &&
         layoutObject.document()
                 .markers()
                 .markersFor(layoutObject.node(), DocumentMarker::Composition)
                 .size() > 0;
}

void InlineTextBoxPainter::paint(const PaintInfo& paintInfo,
                                 const LayoutPoint& paintOffset) {
  if (!shouldPaintTextBox(paintInfo))
    return;

  DCHECK(!shouldPaintSelfOutline(paintInfo.phase) &&
         !shouldPaintDescendantOutlines(paintInfo.phase));

  LayoutRect logicalVisualOverflow = m_inlineTextBox.logicalOverflowRect();
  LayoutUnit logicalStart =
      logicalVisualOverflow.x() +
      (m_inlineTextBox.isHorizontal() ? paintOffset.x() : paintOffset.y());
  LayoutUnit logicalExtent = logicalVisualOverflow.width();

  // We round the y-axis to ensure consistent line heights.
  LayoutPoint adjustedPaintOffset =
      LayoutPoint(paintOffset.x(), LayoutUnit(paintOffset.y().round()));

  if (m_inlineTextBox.isHorizontal()) {
    if (!paintInfo.cullRect().intersectsHorizontalRange(
            logicalStart, logicalStart + logicalExtent))
      return;
  } else {
    if (!paintInfo.cullRect().intersectsVerticalRange(
            logicalStart, logicalStart + logicalExtent))
      return;
  }

  bool isPrinting = paintInfo.isPrinting();

  // Determine whether or not we're selected.
  bool haveSelection = !isPrinting && paintInfo.phase != PaintPhaseTextClip &&
                       m_inlineTextBox.getSelectionState() != SelectionNone;
  if (!haveSelection && paintInfo.phase == PaintPhaseSelection) {
    // When only painting the selection, don't bother to paint if there is none.
    return;
  }

  // The text clip phase already has a LayoutObjectDrawingRecorder. Text clips
  // are initiated only in BoxPainter::paintFillLayer, which is already within a
  // LayoutObjectDrawingRecorder.
  Optional<DrawingRecorder> drawingRecorder;
  if (paintInfo.phase != PaintPhaseTextClip) {
    if (DrawingRecorder::useCachedDrawingIfPossible(
            paintInfo.context, m_inlineTextBox,
            DisplayItem::paintPhaseToDrawingType(paintInfo.phase)))
      return;
    LayoutRect paintRect(logicalVisualOverflow);
    m_inlineTextBox.logicalRectToPhysicalRect(paintRect);
    if (paintInfo.phase != PaintPhaseSelection &&
        (haveSelection || paintsMarkerHighlights(inlineLayoutObject())))
      paintRect.unite(m_inlineTextBox.localSelectionRect(
          m_inlineTextBox.start(),
          m_inlineTextBox.start() + m_inlineTextBox.len()));
    paintRect.moveBy(adjustedPaintOffset);
    drawingRecorder.emplace(
        paintInfo.context, m_inlineTextBox,
        DisplayItem::paintPhaseToDrawingType(paintInfo.phase),
        FloatRect(paintRect));
  }

  GraphicsContext& context = paintInfo.context;
  const ComputedStyle& styleToUse =
      m_inlineTextBox.getLineLayoutItem().styleRef(
          m_inlineTextBox.isFirstLineStyle());

  LayoutPoint boxOrigin(m_inlineTextBox.physicalLocation());
  boxOrigin.move(adjustedPaintOffset.x(), adjustedPaintOffset.y());
  LayoutRect boxRect(boxOrigin, LayoutSize(m_inlineTextBox.logicalWidth(),
                                           m_inlineTextBox.logicalHeight()));

  int length = m_inlineTextBox.len();
  StringView string = StringView(m_inlineTextBox.getLineLayoutItem().text(),
                                 m_inlineTextBox.start(), length);
  int maximumLength = m_inlineTextBox.getLineLayoutItem().textLength() -
                      m_inlineTextBox.start();

  StringBuilder charactersWithHyphen;
  TextRun textRun = m_inlineTextBox.constructTextRun(
      styleToUse, string, maximumLength,
      m_inlineTextBox.hasHyphen() ? &charactersWithHyphen : 0);
  if (m_inlineTextBox.hasHyphen())
    length = textRun.length();

  bool shouldRotate = false;
  LayoutTextCombine* combinedText = nullptr;
  if (!m_inlineTextBox.isHorizontal()) {
    if (styleToUse.hasTextCombine() &&
        m_inlineTextBox.getLineLayoutItem().isCombineText()) {
      combinedText = &toLayoutTextCombine(inlineLayoutObject());
      if (!combinedText->isCombined())
        combinedText = nullptr;
    }
    if (combinedText) {
      combinedText->updateFont();
      boxRect.setWidth(combinedText->inlineWidthForLayout());
      // Justfication applies to before and after the combined text as if
      // it is an ideographic character, and is prohibited inside the
      // combined text.
      if (float expansion = textRun.expansion()) {
        textRun.setExpansion(0);
        if (textRun.allowsLeadingExpansion()) {
          if (textRun.allowsTrailingExpansion())
            expansion /= 2;
          LayoutSize offset =
              LayoutSize(LayoutUnit(), LayoutUnit::fromFloatRound(expansion));
          boxOrigin.move(offset);
          boxRect.move(offset);
        }
      }
    } else {
      shouldRotate = true;
      context.concatCTM(TextPainter::rotation(boxRect, TextPainter::Clockwise));
    }
  }

  // Determine text colors.
  TextPainter::Style textStyle = TextPainter::textPaintingStyle(
      m_inlineTextBox.getLineLayoutItem(), styleToUse, paintInfo);
  TextPainter::Style selectionStyle = TextPainter::selectionPaintingStyle(
      m_inlineTextBox.getLineLayoutItem(), haveSelection, paintInfo, textStyle);
  bool paintSelectedTextOnly = (paintInfo.phase == PaintPhaseSelection);
  bool paintSelectedTextSeparately =
      !paintSelectedTextOnly && textStyle != selectionStyle;

  // Set our font.
  const Font& font = styleToUse.font();
  const SimpleFontData* fontData = font.primaryFont();
  DCHECK(fontData);

  int ascent = fontData ? fontData->getFontMetrics().ascent() : 0;
  LayoutPoint textOrigin(boxOrigin.x(), boxOrigin.y() + ascent);

  // 1. Paint backgrounds behind text if needed. Examples of such backgrounds
  // include selection and composition highlights.
  if (paintInfo.phase != PaintPhaseSelection &&
      paintInfo.phase != PaintPhaseTextClip && !isPrinting) {
    paintDocumentMarkers(paintInfo, boxOrigin, styleToUse, font,
                         DocumentMarkerPaintPhase::Background);

    const LayoutObject& textBoxLayoutObject = inlineLayoutObject();
    if (haveSelection && !paintsCompositionMarkers(textBoxLayoutObject)) {
      if (combinedText)
        paintSelection<InlineTextBoxPainter::PaintOptions::CombinedText>(
            context, boxRect, styleToUse, font, selectionStyle.fillColor,
            combinedText);
      else
        paintSelection<InlineTextBoxPainter::PaintOptions::Normal>(
            context, boxRect, styleToUse, font, selectionStyle.fillColor);
    }
  }

  // 2. Now paint the foreground, including text and decorations like
  // underline/overline (in quirks mode only).
  int selectionStart = 0;
  int selectionEnd = 0;
  if (paintSelectedTextOnly || paintSelectedTextSeparately)
    m_inlineTextBox.selectionStartEnd(selectionStart, selectionEnd);

  bool respectHyphen =
      selectionEnd == static_cast<int>(m_inlineTextBox.len()) &&
      m_inlineTextBox.hasHyphen();
  if (respectHyphen)
    selectionEnd = textRun.length();

  bool ltr = m_inlineTextBox.isLeftToRightDirection();
  bool flowIsLTR = m_inlineTextBox.getLineLayoutItem()
                       .containingBlock()
                       .style()
                       ->isLeftToRightDirection();
  if (m_inlineTextBox.truncation() != cNoTruncation) {
    // In a mixed-direction flow the ellipsis is at the start of the text
    // rather than at the end of it.
    selectionStart =
        ltr == flowIsLTR
            ? std::min<int>(selectionStart, m_inlineTextBox.truncation())
            : std::max<int>(selectionStart, m_inlineTextBox.truncation());
    selectionEnd =
        ltr == flowIsLTR
            ? std::min<int>(selectionEnd, m_inlineTextBox.truncation())
            : std::max<int>(selectionEnd, m_inlineTextBox.truncation());
    length = ltr == flowIsLTR ? m_inlineTextBox.truncation() : textRun.length();
  }

  TextPainter textPainter(context, font, textRun, textOrigin, boxRect,
                          m_inlineTextBox.isHorizontal());
  TextEmphasisPosition emphasisMarkPosition;
  bool hasTextEmphasis =
      m_inlineTextBox.getEmphasisMarkPosition(styleToUse, emphasisMarkPosition);
  if (hasTextEmphasis)
    textPainter.setEmphasisMark(styleToUse.textEmphasisMarkString(),
                                emphasisMarkPosition);
  if (combinedText)
    textPainter.setCombinedText(combinedText);
  if (m_inlineTextBox.truncation() != cNoTruncation && ltr != flowIsLTR)
    textPainter.setEllipsisOffset(m_inlineTextBox.truncation());

  if (!paintSelectedTextOnly) {
    int startOffset = 0;
    int endOffset = length;
    // Where the text and its flow have opposite directions then our offset into
    // the text given by |truncation| is at the start of the part that will be
    // visible.
    if (m_inlineTextBox.truncation() != cNoTruncation && ltr != flowIsLTR) {
      startOffset = m_inlineTextBox.truncation();
      endOffset = textRun.length();
    }

    if (paintSelectedTextSeparately && selectionStart < selectionEnd) {
      startOffset = selectionEnd;
      endOffset = selectionStart;
    }

    textPainter.paint(startOffset, endOffset, length, textStyle);
  }

  if ((paintSelectedTextOnly || paintSelectedTextSeparately) &&
      selectionStart < selectionEnd) {
    // paint only the text that is selected
    textPainter.paint(selectionStart, selectionEnd, length, selectionStyle);
  }

  // Paint decorations
  if (styleToUse.textDecorationsInEffect() != TextDecorationNone &&
      !paintSelectedTextOnly) {
    GraphicsContextStateSaver stateSaver(context, false);

    TextPainter::updateGraphicsContext(
        context, textStyle, m_inlineTextBox.isHorizontal(), stateSaver);

    if (combinedText)
      context.concatCTM(TextPainter::rotation(boxRect, TextPainter::Clockwise));
    paintDecorations(textPainter, paintInfo, boxOrigin,
                     styleToUse.appliedTextDecorations());
    if (combinedText)
      context.concatCTM(
          TextPainter::rotation(boxRect, TextPainter::Counterclockwise));
  }

  if (paintInfo.phase == PaintPhaseForeground)
    paintDocumentMarkers(paintInfo, boxOrigin, styleToUse, font,
                         DocumentMarkerPaintPhase::Foreground);

  if (shouldRotate)
    context.concatCTM(
        TextPainter::rotation(boxRect, TextPainter::Counterclockwise));
}

bool InlineTextBoxPainter::shouldPaintTextBox(const PaintInfo& paintInfo) {
  // When painting selection, we want to include a highlight when the
  // selection spans line breaks. In other cases such as invisible elements
  // or those with no text that are not line breaks, we can skip painting
  // wholesale.
  // TODO(wkorman): Constrain line break painting to appropriate paint phase.
  // This code path is only called in PaintPhaseForeground whereas we would
  // expect PaintPhaseSelection. The existing haveSelection logic in paint()
  // tests for != PaintPhaseTextClip.
  if (m_inlineTextBox.getLineLayoutItem().style()->visibility() !=
          EVisibility::kVisible ||
      m_inlineTextBox.truncation() == cFullTruncation || !m_inlineTextBox.len())
    return false;
  return true;
}

unsigned InlineTextBoxPainter::underlinePaintStart(
    const CompositionUnderline& underline) {
  DCHECK(m_inlineTextBox.truncation() != cFullTruncation);
  DCHECK(m_inlineTextBox.len());

  // Start painting at the beginning of the text or the specified underline
  // start offset, whichever is higher.
  unsigned paintStart =
      std::max(m_inlineTextBox.start(), underline.startOffset());
  // Cap the maximum paint start to (if no truncation) the last character,
  // else the last character before the truncation ellipsis.
  return std::min(paintStart, (m_inlineTextBox.truncation() == cNoTruncation)
                                  ? m_inlineTextBox.end()
                                  : m_inlineTextBox.start() +
                                        m_inlineTextBox.truncation() - 1);
}

unsigned InlineTextBoxPainter::underlinePaintEnd(
    const CompositionUnderline& underline) {
  DCHECK(m_inlineTextBox.truncation() != cFullTruncation);
  DCHECK(m_inlineTextBox.len());

  // End painting just past the end of the text or the specified underline end
  // offset, whichever is lower.
  unsigned paintEnd = std::min(
      m_inlineTextBox.end() + 1,
      underline.endOffset());  // end() points at the last char, not past it.
  // Cap the maximum paint end to (if no truncation) one past the last
  // character, else one past the last character before the truncation
  // ellipsis.
  return std::min(paintEnd,
                  (m_inlineTextBox.truncation() == cNoTruncation)
                      ? m_inlineTextBox.end() + 1
                      : m_inlineTextBox.start() + m_inlineTextBox.truncation());
}

void InlineTextBoxPainter::paintSingleCompositionBackgroundRun(
    GraphicsContext& context,
    const LayoutPoint& boxOrigin,
    const ComputedStyle& style,
    const Font& font,
    Color backgroundColor,
    int startPos,
    int endPos) {
  if (backgroundColor == Color::transparent)
    return;

  int sPos = std::max(startPos - static_cast<int>(m_inlineTextBox.start()), 0);
  int ePos = std::min(endPos - static_cast<int>(m_inlineTextBox.start()),
                      static_cast<int>(m_inlineTextBox.len()));
  if (sPos >= ePos)
    return;

  int deltaY =
      (m_inlineTextBox.getLineLayoutItem().style()->isFlippedLinesWritingMode()
           ? m_inlineTextBox.root().selectionBottom() -
                 m_inlineTextBox.logicalBottom()
           : m_inlineTextBox.logicalTop() -
                 m_inlineTextBox.root().selectionTop())
          .toInt();
  int selHeight = m_inlineTextBox.root().selectionHeight().toInt();
  FloatPoint localOrigin(boxOrigin.x().toFloat(),
                         boxOrigin.y().toFloat() - deltaY);
  context.drawHighlightForText(font, m_inlineTextBox.constructTextRun(style),
                               localOrigin, selHeight, backgroundColor, sPos,
                               ePos);
}

void InlineTextBoxPainter::paintDocumentMarkers(
    const PaintInfo& paintInfo,
    const LayoutPoint& boxOrigin,
    const ComputedStyle& style,
    const Font& font,
    DocumentMarkerPaintPhase markerPaintPhase) {
  if (!m_inlineTextBox.getLineLayoutItem().node())
    return;

  DCHECK(m_inlineTextBox.truncation() != cFullTruncation);
  DCHECK(m_inlineTextBox.len());

  DocumentMarkerVector markers =
      m_inlineTextBox.getLineLayoutItem().document().markers().markersFor(
          m_inlineTextBox.getLineLayoutItem().node());
  DocumentMarkerVector::const_iterator markerIt = markers.begin();

  // Give any document markers that touch this run a chance to draw before the
  // text has been drawn.  Note end() points at the last char, not one past it
  // like endOffset and ranges do.
  for (; markerIt != markers.end(); ++markerIt) {
    DCHECK(*markerIt);
    const DocumentMarker& marker = **markerIt;

    // Paint either the background markers or the foreground markers, but not
    // both.
    switch (marker.type()) {
      case DocumentMarker::Grammar:
      case DocumentMarker::Spelling:
        if (markerPaintPhase == DocumentMarkerPaintPhase::Background)
          continue;
        break;
      case DocumentMarker::TextMatch:
      case DocumentMarker::Composition:
        break;
      default:
        continue;
    }

    if (marker.endOffset() <= m_inlineTextBox.start()) {
      // marker is completely before this run.  This might be a marker that sits
      // before the first run we draw, or markers that were within runs we
      // skipped due to truncation.
      continue;
    }
    if (marker.startOffset() > m_inlineTextBox.end()) {
      // marker is completely after this run, bail.  A later run will paint it.
      break;
    }

    // marker intersects this run.  Paint it.
    switch (marker.type()) {
      case DocumentMarker::Spelling:
        m_inlineTextBox.paintDocumentMarker(paintInfo.context, boxOrigin,
                                            marker, style, font, false);
        break;
      case DocumentMarker::Grammar:
        m_inlineTextBox.paintDocumentMarker(paintInfo.context, boxOrigin,
                                            marker, style, font, true);
        break;
      case DocumentMarker::TextMatch:
        if (markerPaintPhase == DocumentMarkerPaintPhase::Background)
          m_inlineTextBox.paintTextMatchMarkerBackground(paintInfo, boxOrigin,
                                                         marker, style, font);
        else
          m_inlineTextBox.paintTextMatchMarkerForeground(paintInfo, boxOrigin,
                                                         marker, style, font);
        break;
      case DocumentMarker::Composition: {
        CompositionUnderline underline(marker.startOffset(), marker.endOffset(),
                                       marker.underlineColor(), marker.thick(),
                                       marker.backgroundColor());
        if (markerPaintPhase == DocumentMarkerPaintPhase::Background)
          paintSingleCompositionBackgroundRun(
              paintInfo.context, boxOrigin, style, font,
              underline.backgroundColor(), underlinePaintStart(underline),
              underlinePaintEnd(underline));
        else
          paintCompositionUnderline(paintInfo.context, boxOrigin, underline);
      } break;
      default:
        ASSERT_NOT_REACHED();
    }
  }
}

static GraphicsContext::DocumentMarkerLineStyle lineStyleForMarkerType(
    DocumentMarker::MarkerType markerType) {
  switch (markerType) {
    case DocumentMarker::Spelling:
      return GraphicsContext::DocumentMarkerSpellingLineStyle;
    case DocumentMarker::Grammar:
      return GraphicsContext::DocumentMarkerGrammarLineStyle;
    default:
      ASSERT_NOT_REACHED();
      return GraphicsContext::DocumentMarkerSpellingLineStyle;
  }
}

void InlineTextBoxPainter::paintDocumentMarker(GraphicsContext& context,
                                               const LayoutPoint& boxOrigin,
                                               const DocumentMarker& marker,
                                               const ComputedStyle& style,
                                               const Font& font,
                                               bool grammar) {
  // Never print spelling/grammar markers (5327887)
  if (m_inlineTextBox.getLineLayoutItem().document().printing())
    return;

  if (m_inlineTextBox.truncation() == cFullTruncation)
    return;

  LayoutUnit start;  // start of line to draw, relative to tx
  LayoutUnit width = m_inlineTextBox.logicalWidth();  // how much line to draw

  // Determine whether we need to measure text
  bool markerSpansWholeBox = true;
  if (m_inlineTextBox.start() <= marker.startOffset())
    markerSpansWholeBox = false;
  if ((m_inlineTextBox.end() + 1) !=
      marker.endOffset())  // end points at the last char, not past it
    markerSpansWholeBox = false;
  if (m_inlineTextBox.truncation() != cNoTruncation)
    markerSpansWholeBox = false;

  if (!markerSpansWholeBox || grammar) {
    int startPosition, endPosition;
    std::tie(startPosition, endPosition) =
        GetMarkerPaintOffsets(marker, m_inlineTextBox);

    if (m_inlineTextBox.truncation() != cNoTruncation)
      endPosition = std::min<int>(endPosition, m_inlineTextBox.truncation());

    // Calculate start & width
    int deltaY = (m_inlineTextBox.getLineLayoutItem()
                          .style()
                          ->isFlippedLinesWritingMode()
                      ? m_inlineTextBox.root().selectionBottom() -
                            m_inlineTextBox.logicalBottom()
                      : m_inlineTextBox.logicalTop() -
                            m_inlineTextBox.root().selectionTop())
                     .toInt();
    int selHeight = m_inlineTextBox.root().selectionHeight().toInt();
    LayoutPoint startPoint(boxOrigin.x(), boxOrigin.y() - deltaY);
    TextRun run = m_inlineTextBox.constructTextRun(style);

    // FIXME: Convert the document markers to float rects.
    IntRect markerRect = enclosingIntRect(font.selectionRectForText(
        run, FloatPoint(startPoint), selHeight, startPosition, endPosition));
    start = markerRect.x() - startPoint.x();
    width = LayoutUnit(markerRect.width());
  }

  // IMPORTANT: The misspelling underline is not considered when calculating the
  // text bounds, so we have to make sure to fit within those bounds.  This
  // means the top pixel(s) of the underline will overlap the bottom pixel(s) of
  // the glyphs in smaller font sizes.  The alternatives are to increase the
  // line spacing (bad!!) or decrease the underline thickness.  The overlap is
  // actually the most useful, and matches what AppKit does.  So, we generally
  // place the underline at the bottom of the text, but in larger fonts that's
  // not so good so we pin to two pixels under the baseline.
  int lineThickness = misspellingLineThickness;

  const SimpleFontData* fontData =
      m_inlineTextBox.getLineLayoutItem()
          .style(m_inlineTextBox.isFirstLineStyle())
          ->font()
          .primaryFont();
  DCHECK(fontData);
  int baseline = fontData ? fontData->getFontMetrics().ascent() : 0;
  int descent = (m_inlineTextBox.logicalHeight() - baseline).toInt();
  int underlineOffset;
  if (descent <= (lineThickness + 2)) {
    // Place the underline at the very bottom of the text in small/medium fonts.
    underlineOffset = (m_inlineTextBox.logicalHeight() - lineThickness).toInt();
  } else {
    // In larger fonts, though, place the underline up near the baseline to
    // prevent a big gap.
    underlineOffset = baseline + 2;
  }
  context.drawLineForDocumentMarker(
      FloatPoint((boxOrigin.x() + start).toFloat(),
                 (boxOrigin.y() + underlineOffset).toFloat()),
      width.toFloat(), lineStyleForMarkerType(marker.type()),
      style.effectiveZoom());
}

template <InlineTextBoxPainter::PaintOptions options>
void InlineTextBoxPainter::paintSelection(GraphicsContext& context,
                                          const LayoutRect& boxRect,
                                          const ComputedStyle& style,
                                          const Font& font,
                                          Color textColor,
                                          LayoutTextCombine* combinedText) {
  // See if we have a selection to paint at all.
  int sPos, ePos;
  m_inlineTextBox.selectionStartEnd(sPos, ePos);
  if (sPos >= ePos)
    return;

  Color c = m_inlineTextBox.getLineLayoutItem().selectionBackgroundColor();
  if (!c.alpha())
    return;

  // If the text color ends up being the same as the selection background,
  // invert the selection background.
  if (textColor == c)
    c = Color(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());

  // If the text is truncated, let the thing being painted in the truncation
  // draw its own highlight.
  unsigned start = m_inlineTextBox.start();
  int length = m_inlineTextBox.len();
  bool ltr = m_inlineTextBox.isLeftToRightDirection();
  bool flowIsLTR = m_inlineTextBox.getLineLayoutItem()
                       .containingBlock()
                       .style()
                       ->isLeftToRightDirection();
  if (m_inlineTextBox.truncation() != cNoTruncation) {
    // In a mixed-direction flow the ellipsis is at the start of the text
    // so we need to start after it. Otherwise we just need to make sure
    // the end of the text is where the ellipsis starts.
    if (ltr != flowIsLTR)
      sPos = std::max<int>(sPos, m_inlineTextBox.truncation());
    else
      length = m_inlineTextBox.truncation();
  }
  StringView string(m_inlineTextBox.getLineLayoutItem().text(), start,
                    static_cast<unsigned>(length));

  StringBuilder charactersWithHyphen;
  bool respectHyphen = ePos == length && m_inlineTextBox.hasHyphen();
  TextRun textRun = m_inlineTextBox.constructTextRun(
      style, string, m_inlineTextBox.getLineLayoutItem().textLength() -
                         m_inlineTextBox.start(),
      respectHyphen ? &charactersWithHyphen : 0);
  if (respectHyphen)
    ePos = textRun.length();

  GraphicsContextStateSaver stateSaver(context);

  if (options == InlineTextBoxPainter::PaintOptions::CombinedText) {
    DCHECK(combinedText);
    // We can't use the height of m_inlineTextBox because LayoutTextCombine's
    // inlineTextBox is horizontal within vertical flow
    combinedText->transformToInlineCoordinates(context, boxRect, true);
    context.drawHighlightForText(font, textRun, FloatPoint(boxRect.location()),
                                 boxRect.height().toInt(), c, sPos, ePos);
    return;
  }

  LayoutUnit selectionBottom = m_inlineTextBox.root().selectionBottom();
  LayoutUnit selectionTop = m_inlineTextBox.root().selectionTop();

  int deltaY = roundToInt(
      m_inlineTextBox.getLineLayoutItem().style()->isFlippedLinesWritingMode()
          ? selectionBottom - m_inlineTextBox.logicalBottom()
          : m_inlineTextBox.logicalTop() - selectionTop);
  int selHeight = std::max(0, roundToInt(selectionBottom - selectionTop));

  FloatPoint localOrigin(boxRect.x().toFloat(),
                         (boxRect.y() - deltaY).toFloat());
  LayoutRect selectionRect = LayoutRect(
      font.selectionRectForText(textRun, localOrigin, selHeight, sPos, ePos));
  // For line breaks, just painting a selection where the line break itself
  // is rendered is sufficient. Don't select it if there's an ellipsis
  // there.
  if (m_inlineTextBox.hasWrappedSelectionNewline() &&
      m_inlineTextBox.truncation() == cNoTruncation &&
      !m_inlineTextBox.isLineBreak())
    expandToIncludeNewlineForSelection(selectionRect);

  // Line breaks report themselves as having zero width for layout purposes,
  // and so will end up positioned at (0, 0), even though we paint their
  // selection highlight with character width. For RTL then, we have to
  // explicitly shift the selection rect over to paint in the right location.
  if (!m_inlineTextBox.isLeftToRightDirection() &&
      m_inlineTextBox.isLineBreak())
    selectionRect.move(-selectionRect.width(), LayoutUnit());
  if (!flowIsLTR && !ltr && m_inlineTextBox.truncation() != cNoTruncation)
    selectionRect.move(m_inlineTextBox.logicalWidth() - selectionRect.width(),
                       LayoutUnit());

  context.fillRect(FloatRect(selectionRect), c);
}

void InlineTextBoxPainter::expandToIncludeNewlineForSelection(
    LayoutRect& rect) {
  FloatRectOutsets outsets = FloatRectOutsets();
  float spaceWidth = m_inlineTextBox.newlineSpaceWidth();
  if (m_inlineTextBox.isLeftToRightDirection())
    outsets.setRight(spaceWidth);
  else
    outsets.setLeft(spaceWidth);
  rect.expand(outsets);
}

void InlineTextBoxPainter::paintDecorations(
    TextPainter& textPainter,
    const PaintInfo& paintInfo,
    const LayoutPoint& boxOrigin,
    const Vector<AppliedTextDecoration>& decorations) {
  if (m_inlineTextBox.truncation() == cFullTruncation)
    return;

  GraphicsContext& context = paintInfo.context;
  GraphicsContextStateSaver stateSaver(context);

  LayoutPoint localOrigin(boxOrigin);

  LayoutUnit width = m_inlineTextBox.logicalWidth();
  if (m_inlineTextBox.truncation() != cNoTruncation) {
    bool ltr = m_inlineTextBox.isLeftToRightDirection();
    bool flowIsLTR =
        m_inlineTextBox.getLineLayoutItem().style()->isLeftToRightDirection();
    width = LayoutUnit(m_inlineTextBox.getLineLayoutItem().width(
        ltr == flowIsLTR
            ? m_inlineTextBox.start()
            : m_inlineTextBox.start() + m_inlineTextBox.truncation(),
        ltr == flowIsLTR ? m_inlineTextBox.truncation()
                         : m_inlineTextBox.len() - m_inlineTextBox.truncation(),
        m_inlineTextBox.textPos(),
        flowIsLTR ? TextDirection::kLtr : TextDirection::kRtl,
        m_inlineTextBox.isFirstLineStyle()));
    if (!flowIsLTR)
      localOrigin.move(m_inlineTextBox.logicalWidth() - width, LayoutUnit());
  }

  LayoutObject& textBoxLayoutObject = inlineLayoutObject();

  const ComputedStyle& styleToUse =
      textBoxLayoutObject.styleRef(m_inlineTextBox.isFirstLineStyle());
  const SimpleFontData* fontData = styleToUse.font().primaryFont();
  DCHECK(fontData);
  float baseline = fontData ? fontData->getFontMetrics().floatAscent() : 0;

  // Set the thick of the line to be 10% (or something else ?)of the computed
  // font size and not less than 1px.  Using computedFontSize should take care
  // of zoom as well.

  // Update Underline thickness, in case we have Faulty Font Metrics calculating
  // underline thickness by old method.
  float textDecorationThickness = 0.0;
  int fontHeightInt = 0;
  if (fontData) {
    textDecorationThickness = fontData->getFontMetrics().underlineThickness();
    fontHeightInt = (int)(fontData->getFontMetrics().floatHeight() + 0.5);
  }
  if ((textDecorationThickness == 0.f) ||
      (textDecorationThickness >= (fontHeightInt >> 1)))
    textDecorationThickness =
        std::max(1.f, styleToUse.computedFontSize() / 10.f);

  context.setStrokeThickness(textDecorationThickness);

  bool antialiasDecoration = shouldSetDecorationAntialias(decorations);

  // Offset between lines - always non-zero, so lines never cross each other.
  float doubleOffset = textDecorationThickness + 1.f;
  bool skipIntercepts =
      styleToUse.getTextDecorationSkip() & TextDecorationSkipInk;

  // text-underline-position may flip underline and overline.
  ResolvedUnderlinePosition underlinePosition =
      resolveUnderlinePosition(styleToUse, &m_inlineTextBox);
  bool flipUnderlineAndOverline = false;
  if (underlinePosition == ResolvedUnderlinePosition::Over) {
    flipUnderlineAndOverline = true;
    underlinePosition = ResolvedUnderlinePosition::Under;
  }

  for (const AppliedTextDecoration& decoration : decorations) {
    TextDecoration lines = decoration.lines();
    if (flipUnderlineAndOverline) {
      lines = static_cast<TextDecoration>(
          lines ^ (TextDecorationUnderline | TextDecorationOverline));
    }
    if ((lines & TextDecorationUnderline) && fontData) {
      const int underlineOffset = computeUnderlineOffset(
          underlinePosition, styleToUse, fontData->getFontMetrics(),
          &m_inlineTextBox, textDecorationThickness);
      AppliedDecorationPainter decorationPainter(
          context, FloatPoint(localOrigin) + FloatPoint(0, underlineOffset),
          width.toFloat(), decoration, textDecorationThickness, doubleOffset, 1,
          antialiasDecoration);
      if (skipIntercepts) {
        textPainter.clipDecorationsStripe(
            -baseline + decorationPainter.decorationBounds().y() -
                FloatPoint(localOrigin).y(),
            decorationPainter.decorationBounds().height(),
            textDecorationThickness);
      }
      decorationPainter.paint();
    }
    if (lines & TextDecorationOverline) {
      const int overlineOffset =
          computeOverlineOffset(styleToUse, &m_inlineTextBox);
      AppliedDecorationPainter decorationPainter(
          context, FloatPoint(localOrigin) + FloatPoint(0, overlineOffset),
          width.toFloat(), decoration, textDecorationThickness, -doubleOffset,
          1, antialiasDecoration);
      if (skipIntercepts) {
        textPainter.clipDecorationsStripe(
            -baseline + decorationPainter.decorationBounds().y() -
                FloatPoint(localOrigin).y(),
            decorationPainter.decorationBounds().height(),
            textDecorationThickness);
      }
      decorationPainter.paint();
    }
    if (lines & TextDecorationLineThrough) {
      const float lineThroughOffset = 2 * baseline / 3;
      AppliedDecorationPainter decorationPainter(
          context, FloatPoint(localOrigin) + FloatPoint(0, lineThroughOffset),
          width.toFloat(), decoration, textDecorationThickness, doubleOffset, 0,
          antialiasDecoration);
      // No skip: ink for line-through,
      // compare https://github.com/w3c/csswg-drafts/issues/711
      decorationPainter.paint();
    }
  }
}

void InlineTextBoxPainter::paintCompositionUnderline(
    GraphicsContext& context,
    const LayoutPoint& boxOrigin,
    const CompositionUnderline& underline) {
  if (underline.color() == Color::transparent)
    return;

  if (m_inlineTextBox.truncation() == cFullTruncation)
    return;

  unsigned paintStart = underlinePaintStart(underline);
  unsigned paintEnd = underlinePaintEnd(underline);
  DCHECK_LT(paintStart, paintEnd);

  // start of line to draw
  float start =
      paintStart == m_inlineTextBox.start()
          ? 0
          : m_inlineTextBox.getLineLayoutItem().width(
                m_inlineTextBox.start(), paintStart - m_inlineTextBox.start(),
                m_inlineTextBox.textPos(),
                m_inlineTextBox.isLeftToRightDirection() ? TextDirection::kLtr
                                                         : TextDirection::kRtl,
                m_inlineTextBox.isFirstLineStyle());
  // how much line to draw
  float width;
  bool ltr = m_inlineTextBox.isLeftToRightDirection();
  bool flowIsLTR =
      m_inlineTextBox.getLineLayoutItem().style()->isLeftToRightDirection();
  if (paintStart == m_inlineTextBox.start() &&
      paintEnd == m_inlineTextBox.end() + 1) {
    width = m_inlineTextBox.logicalWidth().toFloat();
  } else {
    unsigned paintFrom = ltr == flowIsLTR ? paintStart : paintEnd;
    unsigned paintLength =
        ltr == flowIsLTR
            ? paintEnd - paintStart
            : m_inlineTextBox.start() + m_inlineTextBox.len() - paintEnd;
    width = m_inlineTextBox.getLineLayoutItem().width(
        paintFrom, paintLength, LayoutUnit(m_inlineTextBox.textPos() + start),
        flowIsLTR ? TextDirection::kLtr : TextDirection::kRtl,
        m_inlineTextBox.isFirstLineStyle());
  }
  // In RTL mode, start and width are computed from the right end of the text
  // box: starting at |logicalWidth| - |start| and continuing left by |width| to
  // |logicalWidth| - |start| - |width|. We will draw that line, but backwards:
  // |logicalWidth| - |start| - |width| to |logicalWidth| - |start|.
  if (!flowIsLTR)
    start = m_inlineTextBox.logicalWidth().toFloat() - width - start;

  // Thick marked text underlines are 2px thick as long as there is room for the
  // 2px line under the baseline.  All other marked text underlines are 1px
  // thick.  If there's not enough space the underline will touch or overlap
  // characters.
  int lineThickness = 1;
  const SimpleFontData* fontData =
      m_inlineTextBox.getLineLayoutItem()
          .style(m_inlineTextBox.isFirstLineStyle())
          ->font()
          .primaryFont();
  DCHECK(fontData);
  int baseline = fontData ? fontData->getFontMetrics().ascent() : 0;
  if (underline.thick() && m_inlineTextBox.logicalHeight() - baseline >= 2)
    lineThickness = 2;

  // We need to have some space between underlines of subsequent clauses,
  // because some input methods do not use different underline styles for those.
  // We make each line shorter, which has a harmless side effect of shortening
  // the first and last clauses, too.
  start += 1;
  width -= 2;

  context.setStrokeColor(underline.color());
  context.setStrokeThickness(lineThickness);
  context.drawLineForText(
      FloatPoint(
          boxOrigin.x() + start,
          (boxOrigin.y() + m_inlineTextBox.logicalHeight() - lineThickness)
              .toFloat()),
      width);
}

void InlineTextBoxPainter::paintTextMatchMarkerForeground(
    const PaintInfo& paintInfo,
    const LayoutPoint& boxOrigin,
    const DocumentMarker& marker,
    const ComputedStyle& style,
    const Font& font) {
  if (!inlineLayoutObject().frame()->editor().markedTextMatchesAreHighlighted())
    return;

  const auto paintOffsets = GetMarkerPaintOffsets(marker, m_inlineTextBox);
  TextRun run = m_inlineTextBox.constructTextRun(style);

  Color textColor =
      LayoutTheme::theme().platformTextSearchColor(marker.activeMatch());
  if (style.visitedDependentColor(CSSPropertyColor) == textColor)
    return;

  const SimpleFontData* fontData = font.primaryFont();
  DCHECK(fontData);
  if (!fontData)
    return;

  TextPainter::Style textStyle;
  textStyle.currentColor = textStyle.fillColor = textStyle.strokeColor =
      textStyle.emphasisMarkColor = textColor;
  textStyle.strokeWidth = style.textStrokeWidth();
  textStyle.shadow = 0;

  LayoutRect boxRect(boxOrigin, LayoutSize(m_inlineTextBox.logicalWidth(),
                                           m_inlineTextBox.logicalHeight()));
  LayoutPoint textOrigin(boxOrigin.x(),
                         boxOrigin.y() + fontData->getFontMetrics().ascent());
  TextPainter textPainter(paintInfo.context, font, run, textOrigin, boxRect,
                          m_inlineTextBox.isHorizontal());

  textPainter.paint(paintOffsets.first, paintOffsets.second,
                    m_inlineTextBox.len(), textStyle);
}

void InlineTextBoxPainter::paintTextMatchMarkerBackground(
    const PaintInfo& paintInfo,
    const LayoutPoint& boxOrigin,
    const DocumentMarker& marker,
    const ComputedStyle& style,
    const Font& font) {
  if (!LineLayoutAPIShim::layoutObjectFrom(m_inlineTextBox.getLineLayoutItem())
           ->frame()
           ->editor()
           .markedTextMatchesAreHighlighted())
    return;

  const auto paintOffsets = GetMarkerPaintOffsets(marker, m_inlineTextBox);
  TextRun run = m_inlineTextBox.constructTextRun(style);

  Color color = LayoutTheme::theme().platformTextSearchHighlightColor(
      marker.activeMatch());
  GraphicsContext& context = paintInfo.context;
  GraphicsContextStateSaver stateSaver(context);

  LayoutRect boxRect(boxOrigin, LayoutSize(m_inlineTextBox.logicalWidth(),
                                           m_inlineTextBox.logicalHeight()));
  context.clip(FloatRect(boxRect));
  context.drawHighlightForText(font, run, FloatPoint(boxOrigin),
                               boxRect.height().toInt(), color,
                               paintOffsets.first, paintOffsets.second);
}

}  // namespace blink
