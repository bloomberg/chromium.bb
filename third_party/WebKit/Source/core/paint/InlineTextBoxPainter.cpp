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
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintRecorder.h"
#include "platform/wtf/Optional.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace blink {

namespace {

std::pair<unsigned, unsigned> GetMarkerPaintOffsets(
    const DocumentMarker& marker,
    const InlineTextBox& text_box) {
  const unsigned start_offset = marker.StartOffset() > text_box.Start()
                                    ? marker.StartOffset() - text_box.Start()
                                    : 0U;
  const unsigned end_offset =
      std::min(marker.EndOffset() - text_box.Start(), text_box.Len());
  return std::make_pair(start_offset, end_offset);
}
}

enum class ResolvedUnderlinePosition { kRoman, kUnder, kOver };

static ResolvedUnderlinePosition ResolveUnderlinePosition(
    const ComputedStyle& style,
    FontBaseline baseline_type) {
  // |auto| should resolve to |under| to avoid drawing through glyphs in
  // scripts where it would not be appropriate (e.g., ideographs.)
  // However, this has performance implications. For now, we only work with
  // vertical text.
  switch (baseline_type) {
    case kAlphabeticBaseline:
      switch (style.GetTextUnderlinePosition()) {
        case kTextUnderlinePositionAuto:
          return ResolvedUnderlinePosition::kRoman;
        case kTextUnderlinePositionUnder:
          return ResolvedUnderlinePosition::kUnder;
      }
      break;
    case kIdeographicBaseline:
      // Compute language-appropriate default underline position.
      // https://drafts.csswg.org/css-text-decor-3/#default-stylesheet
      UScriptCode script = style.GetFontDescription().GetScript();
      if (script == USCRIPT_KATAKANA_OR_HIRAGANA || script == USCRIPT_HANGUL)
        return ResolvedUnderlinePosition::kOver;
      return ResolvedUnderlinePosition::kUnder;
  }
  NOTREACHED();
  return ResolvedUnderlinePosition::kRoman;
}

static LineLayoutItem EnclosingUnderlineObject(
    const InlineTextBox* inline_text_box) {
  bool first_line = inline_text_box->IsFirstLineStyle();
  for (LineLayoutItem current = inline_text_box->Parent()->GetLineLayoutItem();
       ;) {
    if (current.IsLayoutBlock())
      return current;
    if (!current.IsLayoutInline() || current.IsRubyText())
      return nullptr;

    const ComputedStyle& style_to_use = current.StyleRef(first_line);
    if (style_to_use.GetTextDecoration() & kTextDecorationUnderline)
      return current;

    current = current.Parent();
    if (!current)
      return current;

    if (Node* node = current.GetNode()) {
      if (isHTMLAnchorElement(node) || node->HasTagName(HTMLNames::fontTag))
        return current;
    }
  }
}

static int ComputeUnderlineOffsetForUnder(
    const ComputedStyle& style,
    const InlineTextBox* inline_text_box,
    LineLayoutItem decorating_box,
    float text_decoration_thickness,
    LineVerticalPositionType position_type) {
  const RootInlineBox& root = inline_text_box->Root();
  FontBaseline baseline_type = root.BaselineType();
  LayoutUnit offset = inline_text_box->OffsetTo(position_type, baseline_type);

  // Compute offset to the farthest position of the decorating box.
  LayoutUnit logical_top = inline_text_box->LogicalTop();
  LayoutUnit position = logical_top + offset;
  LayoutUnit farthest = root.FarthestPositionForUnderline(
      decorating_box, position_type, baseline_type, position);
  // Round() looks more logical but Floor() produces better results in
  // positive/negative offsets, in horizontal/vertical flows, on Win/Mac/Linux.
  int offset_int = (farthest - logical_top).Floor();

  // Gaps are not needed for TextTop because it generally has internal
  // leadings.
  if (position_type == LineVerticalPositionType::TextTop)
    return offset_int;
  return !IsLineOverSide(position_type) ? offset_int + 1 : offset_int - 1;
}

static int ComputeUnderlineOffsetForRoman(
    const FontMetrics& font_metrics,
    const float text_decoration_thickness) {
  // Compute the gap between the font and the underline. Use at least one
  // pixel gap, if underline is thick then use a bigger gap.
  int gap = 0;

  // Underline position of zero means draw underline on Baseline Position,
  // in Blink we need at least 1-pixel gap to adding following check.
  // Positive underline Position means underline should be drawn above baseline
  // and negative value means drawing below baseline, negating the value as in
  // Blink downward Y-increases.

  if (font_metrics.UnderlinePosition())
    gap = -font_metrics.UnderlinePosition();
  else
    gap = std::max<int>(1, ceilf(text_decoration_thickness / 2.f));

  // Position underline near the alphabetic baseline.
  return font_metrics.Ascent() + gap;
}

static int ComputeUnderlineOffset(ResolvedUnderlinePosition underline_position,
                                  const ComputedStyle& style,
                                  const FontMetrics& font_metrics,
                                  const InlineTextBox* inline_text_box,
                                  LineLayoutItem decorating_box,
                                  const float text_decoration_thickness) {
  switch (underline_position) {
    default:
      NOTREACHED();
    // Fall through.
    case ResolvedUnderlinePosition::kRoman:
      return ComputeUnderlineOffsetForRoman(font_metrics,
                                            text_decoration_thickness);
    case ResolvedUnderlinePosition::kUnder:
      // Position underline at the under edge of the lowest element's
      // content box.
      return ComputeUnderlineOffsetForUnder(
          style, inline_text_box, decorating_box, text_decoration_thickness,
          LineVerticalPositionType::BottomOfEmHeight);
  }
}

static bool ShouldSetDecorationAntialias(
    const Vector<AppliedTextDecoration>& decorations) {
  for (const AppliedTextDecoration& decoration : decorations) {
    TextDecorationStyle decoration_style = decoration.Style();
    if (decoration_style == kTextDecorationStyleDotted ||
        decoration_style == kTextDecorationStyleDashed)
      return true;
  }
  return false;
}

static StrokeStyle TextDecorationStyleToStrokeStyle(
    TextDecorationStyle decoration_style) {
  StrokeStyle stroke_style = kSolidStroke;
  switch (decoration_style) {
    case kTextDecorationStyleSolid:
      stroke_style = kSolidStroke;
      break;
    case kTextDecorationStyleDouble:
      stroke_style = kDoubleStroke;
      break;
    case kTextDecorationStyleDotted:
      stroke_style = kDottedStroke;
      break;
    case kTextDecorationStyleDashed:
      stroke_style = kDashedStroke;
      break;
    case kTextDecorationStyleWavy:
      stroke_style = kWavyStroke;
      break;
  }

  return stroke_style;
}

static void AdjustStepToDecorationLength(float& step,
                                         float& control_point_distance,
                                         float length) {
  DCHECK_GT(step, 0);

  if (length <= 0)
    return;

  unsigned step_count = static_cast<unsigned>(length / step);

  // Each Bezier curve starts at the same pixel that the previous one
  // ended. We need to subtract (stepCount - 1) pixels when calculating the
  // length covered to account for that.
  float uncovered_length = length - (step_count * step - (step_count - 1));
  float adjustment = uncovered_length / step_count;
  step += adjustment;
  control_point_distance += adjustment;
}

// Holds text decoration painting values to be computed once and subsequently
// use multiple times to handle decoration paint order correctly. See also
// https://www.w3.org/TR/css-text-decor-3/#painting-order
struct DecorationInfo final {
  STACK_ALLOCATED();

  LayoutUnit width;
  FloatPoint local_origin;
  bool antialias;
  float baseline;
  const ComputedStyle* style;
  const SimpleFontData* font_data;
  float thickness;
  float double_offset;
  FontBaseline baseline_type;
  ResolvedUnderlinePosition underline_position;
  // Decorating box: https://drafts.csswg.org/css-text-decor-3/#decorating-box
  LineLayoutItem decorating_box;
};

class AppliedDecorationPainter final {
  STACK_ALLOCATED();

 public:
  AppliedDecorationPainter(GraphicsContext& context,
                           const DecorationInfo& decoration_info,
                           float start_point_y_offset,
                           const AppliedTextDecoration& decoration,
                           float double_offset,
                           int wavy_offset_factor)
      : context_(context),
        start_point_(decoration_info.local_origin +
                     FloatPoint(0, start_point_y_offset)),
        decoration_info_(decoration_info),
        decoration_(decoration),
        double_offset_(double_offset),
        wavy_offset_factor_(wavy_offset_factor){};

  void Paint();
  FloatRect DecorationBounds();

 private:
  void StrokeWavyTextDecoration();

  Path PrepareWavyStrokePath();
  Path PrepareDottedDashedStrokePath();

  GraphicsContext& context_;
  const FloatPoint start_point_;
  const DecorationInfo& decoration_info_;
  const AppliedTextDecoration& decoration_;
  const float double_offset_;
  const int wavy_offset_factor_;
};

Path AppliedDecorationPainter::PrepareDottedDashedStrokePath() {
  // These coordinate transforms need to match what's happening in
  // GraphicsContext's drawLineForText and drawLine.
  int y = floorf(start_point_.Y() +
                 std::max<float>(decoration_info_.thickness / 2.0f, 0.5f));
  Path stroke_path;
  FloatPoint rounded_start_point(start_point_.X(), y);
  FloatPoint rounded_end_point(rounded_start_point +
                               FloatPoint(decoration_info_.width, 0));
  context_.AdjustLineToPixelBoundaries(rounded_start_point, rounded_end_point,
                                       roundf(decoration_info_.thickness),
                                       context_.GetStrokeStyle());
  stroke_path.MoveTo(rounded_start_point);
  stroke_path.AddLineTo(rounded_end_point);
  return stroke_path;
}

FloatRect AppliedDecorationPainter::DecorationBounds() {
  StrokeData stroke_data;
  stroke_data.SetThickness(decoration_info_.thickness);

  switch (decoration_.Style()) {
    case kTextDecorationStyleDotted:
    case kTextDecorationStyleDashed: {
      stroke_data.SetStyle(
          TextDecorationStyleToStrokeStyle(decoration_.Style()));
      return PrepareDottedDashedStrokePath().StrokeBoundingRect(
          stroke_data, Path::BoundsType::kExact);
    }
    case kTextDecorationStyleWavy:
      return PrepareWavyStrokePath().StrokeBoundingRect(
          stroke_data, Path::BoundsType::kExact);
      break;
    case kTextDecorationStyleDouble:
      if (double_offset_ > 0) {
        return FloatRect(start_point_.X(), start_point_.Y(),
                         decoration_info_.width,
                         double_offset_ + decoration_info_.thickness);
      }
      return FloatRect(start_point_.X(), start_point_.Y() + double_offset_,
                       decoration_info_.width,
                       -double_offset_ + decoration_info_.thickness);
      break;
    case kTextDecorationStyleSolid:
      return FloatRect(start_point_.X(), start_point_.Y(),
                       decoration_info_.width, decoration_info_.thickness);
    default:
      break;
  }
  NOTREACHED();
  return FloatRect();
}

void AppliedDecorationPainter::Paint() {
  context_.SetStrokeStyle(
      TextDecorationStyleToStrokeStyle(decoration_.Style()));
  context_.SetStrokeColor(decoration_.GetColor());

  switch (decoration_.Style()) {
    case kTextDecorationStyleWavy:
      StrokeWavyTextDecoration();
      break;
    case kTextDecorationStyleDotted:
    case kTextDecorationStyleDashed:
      context_.SetShouldAntialias(decoration_info_.antialias);
    // Fall through
    default:
      context_.DrawLineForText(start_point_, decoration_info_.width);

      if (decoration_.Style() == kTextDecorationStyleDouble) {
        context_.DrawLineForText(start_point_ + FloatPoint(0, double_offset_),
                                 decoration_info_.width);
      }
  }
}

void AppliedDecorationPainter::StrokeWavyTextDecoration() {
  context_.SetShouldAntialias(true);
  context_.StrokePath(PrepareWavyStrokePath());
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
Path AppliedDecorationPainter::PrepareWavyStrokePath() {
  FloatPoint p1(start_point_ +
                FloatPoint(0, double_offset_ * wavy_offset_factor_));
  FloatPoint p2(
      start_point_ +
      FloatPoint(decoration_info_.width, double_offset_ * wavy_offset_factor_));

  context_.AdjustLineToPixelBoundaries(p1, p2, decoration_info_.thickness,
                                       context_.GetStrokeStyle());

  Path path;
  path.MoveTo(p1);

  // Distance between decoration's axis and Bezier curve's control points.
  // The height of the curve is based on this distance. Use a minimum of 6
  // pixels distance since
  // the actual curve passes approximately at half of that distance, that is 3
  // pixels.
  // The minimum height of the curve is also approximately 3 pixels. Increases
  // the curve's height
  // as strockThickness increases to make the curve looks better.
  float control_point_distance =
      3 * std::max<float>(2, decoration_info_.thickness);

  // Increment used to form the diamond shape between start point (p1), control
  // points and end point (p2) along the axis of the decoration. Makes the
  // curve wider as strockThickness increases to make the curve looks better.
  float step = 2 * std::max<float>(2, decoration_info_.thickness);

  bool is_vertical_line = (p1.X() == p2.X());

  if (is_vertical_line) {
    DCHECK(p1.X() == p2.X());

    float x_axis = p1.X();
    float y1;
    float y2;

    if (p1.Y() < p2.Y()) {
      y1 = p1.Y();
      y2 = p2.Y();
    } else {
      y1 = p2.Y();
      y2 = p1.Y();
    }

    AdjustStepToDecorationLength(step, control_point_distance, y2 - y1);
    FloatPoint control_point1(x_axis + control_point_distance, 0);
    FloatPoint control_point2(x_axis - control_point_distance, 0);

    for (float y = y1; y + 2 * step <= y2;) {
      control_point1.SetY(y + step);
      control_point2.SetY(y + step);
      y += 2 * step;
      path.AddBezierCurveTo(control_point1, control_point2,
                            FloatPoint(x_axis, y));
    }
  } else {
    DCHECK(p1.Y() == p2.Y());

    float y_axis = p1.Y();
    float x1;
    float x2;

    if (p1.X() < p2.X()) {
      x1 = p1.X();
      x2 = p2.X();
    } else {
      x1 = p2.X();
      x2 = p1.X();
    }

    AdjustStepToDecorationLength(step, control_point_distance, x2 - x1);
    FloatPoint control_point1(0, y_axis + control_point_distance);
    FloatPoint control_point2(0, y_axis - control_point_distance);

    for (float x = x1; x + 2 * step <= x2;) {
      control_point1.SetX(x + step);
      control_point2.SetX(x + step);
      x += 2 * step;
      path.AddBezierCurveTo(control_point1, control_point2,
                            FloatPoint(x, y_axis));
    }
  }
  return path;
}

static const int kMisspellingLineThickness = 3;

LayoutObject& InlineTextBoxPainter::InlineLayoutObject() const {
  return *LineLayoutAPIShim::LayoutObjectFrom(
      inline_text_box_.GetLineLayoutItem());
}

bool InlineTextBoxPainter::PaintsMarkerHighlights(
    const LayoutObject& layout_object) {
  return layout_object.GetNode() &&
         layout_object.GetDocument().Markers().HasMarkers(
             layout_object.GetNode());
}

static bool PaintsCompositionMarkers(const LayoutObject& layout_object) {
  return layout_object.GetNode() &&
         layout_object.GetDocument()
                 .Markers()
                 .MarkersFor(layout_object.GetNode(),
                             DocumentMarker::kComposition)
                 .size() > 0;
}

static void PrepareContextForDecoration(GraphicsContext& context,
                                        GraphicsContextStateSaver& state_saver,
                                        bool is_horizontal,
                                        const TextPainter::Style& text_style,
                                        const LayoutTextCombine* combined_text,
                                        const LayoutRect& box_rect) {
  TextPainter::UpdateGraphicsContext(context, text_style, is_horizontal,
                                     state_saver);
  if (combined_text)
    context.ConcatCTM(TextPainter::Rotation(box_rect, TextPainter::kClockwise));
}

static void RestoreContextFromDecoration(GraphicsContext& context,
                                         const LayoutTextCombine* combined_text,
                                         const LayoutRect& box_rect) {
  if (combined_text) {
    context.ConcatCTM(
        TextPainter::Rotation(box_rect, TextPainter::kCounterclockwise));
  }
}

static float ComputeDecorationThickness(const ComputedStyle* style,
                                        const SimpleFontData* font_data) {
  // Set the thick of the line to be 10% (or something else ?)of the computed
  // font size and not less than 1px.  Using computedFontSize should take care
  // of zoom as well.

  // Update Underline thickness, in case we have Faulty Font Metrics calculating
  // underline thickness by old method.
  float text_decoration_thickness = 0.0;
  int font_height_int = 0;
  if (font_data) {
    text_decoration_thickness =
        font_data->GetFontMetrics().UnderlineThickness();
    font_height_int = font_data->GetFontMetrics().Height();
  }
  if ((text_decoration_thickness == 0.f) ||
      (text_decoration_thickness >= (font_height_int >> 1))) {
    text_decoration_thickness = std::max(1.f, style->ComputedFontSize() / 10.f);
  }
  return text_decoration_thickness;
}

static void ComputeDecorationInfo(
    DecorationInfo& decoration_info,
    const InlineTextBox& box,
    const LayoutPoint& box_origin,
    const Vector<AppliedTextDecoration>& decorations) {
  LayoutPoint local_origin = LayoutPoint(box_origin);
  LayoutUnit width = box.LogicalWidth();
  if (box.Truncation() != kCNoTruncation) {
    bool ltr = box.IsLeftToRightDirection();
    bool flow_is_ltr =
        box.GetLineLayoutItem().Style()->IsLeftToRightDirection();
    width = LayoutUnit(box.GetLineLayoutItem().Width(
        ltr == flow_is_ltr ? box.Start() : box.Start() + box.Truncation(),
        ltr == flow_is_ltr ? box.Truncation() : box.Len() - box.Truncation(),
        box.TextPos(), flow_is_ltr ? TextDirection::kLtr : TextDirection::kRtl,
        box.IsFirstLineStyle()));
    if (!flow_is_ltr) {
      local_origin.Move(box.LogicalWidth() - width, LayoutUnit());
    }
  }
  decoration_info.width = width;
  decoration_info.local_origin = FloatPoint(local_origin);

  decoration_info.antialias = ShouldSetDecorationAntialias(decorations);

  decoration_info.style =
      LineLayoutAPIShim::LayoutObjectFrom(box.GetLineLayoutItem())
          ->Style(box.IsFirstLineStyle());
  decoration_info.baseline_type = box.Root().BaselineType();
  decoration_info.underline_position = ResolveUnderlinePosition(
      *decoration_info.style, decoration_info.baseline_type);

  decoration_info.font_data = decoration_info.style->GetFont().PrimaryFont();
  DCHECK(decoration_info.font_data);
  decoration_info.baseline =
      decoration_info.font_data
          ? decoration_info.font_data->GetFontMetrics().FloatAscent()
          : 0;

  if (decoration_info.underline_position == ResolvedUnderlinePosition::kRoman) {
    decoration_info.thickness = ComputeDecorationThickness(
        decoration_info.style, decoration_info.font_data);
  } else {
    // Compute decorating box. Position and thickness are computed from the
    // decorating box.
    // Only for non-Roman for now for the performance implications.
    // https:// drafts.csswg.org/css-text-decor-3/#decorating-box
    decoration_info.decorating_box = EnclosingUnderlineObject(&box);
    if (decoration_info.decorating_box) {
      decoration_info.thickness = ComputeDecorationThickness(
          decoration_info.decorating_box.Style(),
          decoration_info.decorating_box.Style()->GetFont().PrimaryFont());
    } else {
      decoration_info.thickness = ComputeDecorationThickness(
          decoration_info.style, decoration_info.font_data);
    }
  }

  // Offset between lines - always non-zero, so lines never cross each other.
  decoration_info.double_offset = decoration_info.thickness + 1.f;
}

static void PaintDecorationsExceptLineThrough(
    TextPainter& text_painter,
    bool& has_line_through_decoration,
    const InlineTextBox& box,
    const DecorationInfo& decoration_info,
    const PaintInfo& paint_info,
    const Vector<AppliedTextDecoration>& decorations) {
  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  context.SetStrokeThickness(decoration_info.thickness);
  bool skip_intercepts =
      decoration_info.style->GetTextDecorationSkip() & kTextDecorationSkipInk;

  // text-underline-position may flip underline and overline.
  ResolvedUnderlinePosition underline_position =
      decoration_info.underline_position;
  bool flip_underline_and_overline = false;
  if (underline_position == ResolvedUnderlinePosition::kOver) {
    flip_underline_and_overline = true;
    underline_position = ResolvedUnderlinePosition::kUnder;
  }

  for (const AppliedTextDecoration& decoration : decorations) {
    TextDecoration lines = decoration.Lines();
    if (flip_underline_and_overline) {
      lines = static_cast<TextDecoration>(
          lines ^ (kTextDecorationUnderline | kTextDecorationOverline));
    }
    if ((lines & kTextDecorationUnderline) && decoration_info.font_data) {
      const int underline_offset = ComputeUnderlineOffset(
          underline_position, *decoration_info.style,
          decoration_info.font_data->GetFontMetrics(), &box,
          decoration_info.decorating_box, decoration_info.thickness);
      AppliedDecorationPainter decoration_painter(
          context, decoration_info, underline_offset, decoration,
          decoration_info.double_offset, 1);
      if (skip_intercepts) {
        text_painter.ClipDecorationsStripe(
            -decoration_info.baseline +
                decoration_painter.DecorationBounds().Y() -
                decoration_info.local_origin.Y(),
            decoration_painter.DecorationBounds().Height(),
            decoration_info.thickness);
      }
      decoration_painter.Paint();
    }
    if (lines & kTextDecorationOverline) {
      const int overline_offset = ComputeUnderlineOffsetForUnder(
          *decoration_info.style, &box, decoration_info.decorating_box,
          decoration_info.thickness,
          flip_underline_and_overline ? LineVerticalPositionType::TopOfEmHeight
                                      : LineVerticalPositionType::TextTop);
      AppliedDecorationPainter decoration_painter(
          context, decoration_info, overline_offset, decoration,
          -decoration_info.double_offset, 1);
      if (skip_intercepts) {
        text_painter.ClipDecorationsStripe(
            -decoration_info.baseline +
                decoration_painter.DecorationBounds().Y() -
                decoration_info.local_origin.Y(),
            decoration_painter.DecorationBounds().Height(),
            decoration_info.thickness);
      }
      decoration_painter.Paint();
    }
    // We could instead build a vector of the TextDecoration instances needing
    // line-through but this is a rare case so better to avoid vector overhead.
    has_line_through_decoration |= ((lines & kTextDecorationLineThrough) != 0);
  }
}

static void PaintDecorationsOnlyLineThrough(
    TextPainter& text_painter,
    const DecorationInfo& decoration_info,
    const PaintInfo& paint_info,
    const Vector<AppliedTextDecoration>& decorations) {
  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  context.SetStrokeThickness(decoration_info.thickness);
  for (const AppliedTextDecoration& decoration : decorations) {
    TextDecoration lines = decoration.Lines();
    if (lines & kTextDecorationLineThrough) {
      const float line_through_offset = 2 * decoration_info.baseline / 3;
      AppliedDecorationPainter decoration_painter(
          context, decoration_info, line_through_offset, decoration,
          decoration_info.double_offset, 0);
      // No skip: ink for line-through,
      // compare https://github.com/w3c/csswg-drafts/issues/711
      decoration_painter.Paint();
    }
  }
}

void InlineTextBoxPainter::Paint(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  if (!ShouldPaintTextBox(paint_info))
    return;

  DCHECK(!ShouldPaintSelfOutline(paint_info.phase) &&
         !ShouldPaintDescendantOutlines(paint_info.phase));

  LayoutRect logical_visual_overflow = inline_text_box_.LogicalOverflowRect();
  LayoutUnit logical_start =
      logical_visual_overflow.X() +
      (inline_text_box_.IsHorizontal() ? paint_offset.X() : paint_offset.Y());
  LayoutUnit logical_extent = logical_visual_overflow.Width();

  // We round the y-axis to ensure consistent line heights.
  LayoutPoint adjusted_paint_offset =
      LayoutPoint(paint_offset.X(), LayoutUnit(paint_offset.Y().Round()));

  if (inline_text_box_.IsHorizontal()) {
    if (!paint_info.GetCullRect().IntersectsHorizontalRange(
            logical_start, logical_start + logical_extent))
      return;
  } else {
    if (!paint_info.GetCullRect().IntersectsVerticalRange(
            logical_start, logical_start + logical_extent))
      return;
  }

  bool is_printing = paint_info.IsPrinting();

  // Determine whether or not we're selected.
  bool have_selection = !is_printing &&
                        paint_info.phase != kPaintPhaseTextClip &&
                        inline_text_box_.GetSelectionState() != SelectionNone;
  if (!have_selection && paint_info.phase == kPaintPhaseSelection) {
    // When only painting the selection, don't bother to paint if there is none.
    return;
  }

  // The text clip phase already has a LayoutObjectDrawingRecorder. Text clips
  // are initiated only in BoxPainter::paintFillLayer, which is already within a
  // LayoutObjectDrawingRecorder.
  Optional<DrawingRecorder> drawing_recorder;
  if (paint_info.phase != kPaintPhaseTextClip) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, inline_text_box_,
            DisplayItem::PaintPhaseToDrawingType(paint_info.phase)))
      return;
    LayoutRect paint_rect(logical_visual_overflow);
    inline_text_box_.LogicalRectToPhysicalRect(paint_rect);
    if (paint_info.phase != kPaintPhaseSelection &&
        (have_selection || PaintsMarkerHighlights(InlineLayoutObject())))
      paint_rect.Unite(inline_text_box_.LocalSelectionRect(
          inline_text_box_.Start(),
          inline_text_box_.Start() + inline_text_box_.Len()));
    paint_rect.MoveBy(adjusted_paint_offset);
    drawing_recorder.emplace(
        paint_info.context, inline_text_box_,
        DisplayItem::PaintPhaseToDrawingType(paint_info.phase),
        FloatRect(paint_rect));
  }

  GraphicsContext& context = paint_info.context;
  const ComputedStyle& style_to_use =
      inline_text_box_.GetLineLayoutItem().StyleRef(
          inline_text_box_.IsFirstLineStyle());

  LayoutPoint box_origin(inline_text_box_.PhysicalLocation());
  box_origin.Move(adjusted_paint_offset.X(), adjusted_paint_offset.Y());
  LayoutRect box_rect(box_origin, LayoutSize(inline_text_box_.LogicalWidth(),
                                             inline_text_box_.LogicalHeight()));

  int length = inline_text_box_.Len();
  StringView string = StringView(inline_text_box_.GetLineLayoutItem().GetText(),
                                 inline_text_box_.Start(), length);
  int maximum_length = inline_text_box_.GetLineLayoutItem().TextLength() -
                       inline_text_box_.Start();

  StringBuilder characters_with_hyphen;
  TextRun text_run = inline_text_box_.ConstructTextRun(
      style_to_use, string, maximum_length,
      inline_text_box_.HasHyphen() ? &characters_with_hyphen : 0);
  if (inline_text_box_.HasHyphen())
    length = text_run.length();

  bool should_rotate = false;
  LayoutTextCombine* combined_text = nullptr;
  if (!inline_text_box_.IsHorizontal()) {
    if (style_to_use.HasTextCombine() &&
        inline_text_box_.GetLineLayoutItem().IsCombineText()) {
      combined_text = &ToLayoutTextCombine(InlineLayoutObject());
      if (!combined_text->IsCombined())
        combined_text = nullptr;
    }
    if (combined_text) {
      combined_text->UpdateFont();
      box_rect.SetWidth(combined_text->InlineWidthForLayout());
      // Justfication applies to before and after the combined text as if
      // it is an ideographic character, and is prohibited inside the
      // combined text.
      if (float expansion = text_run.Expansion()) {
        text_run.SetExpansion(0);
        if (text_run.AllowsLeadingExpansion()) {
          if (text_run.AllowsTrailingExpansion())
            expansion /= 2;
          LayoutSize offset =
              LayoutSize(LayoutUnit(), LayoutUnit::FromFloatRound(expansion));
          box_origin.Move(offset);
          box_rect.Move(offset);
        }
      }
    } else {
      should_rotate = true;
      context.ConcatCTM(
          TextPainter::Rotation(box_rect, TextPainter::kClockwise));
    }
  }

  // Determine text colors.
  TextPainter::Style text_style = TextPainter::TextPaintingStyle(
      inline_text_box_.GetLineLayoutItem(), style_to_use, paint_info);
  TextPainter::Style selection_style = TextPainter::SelectionPaintingStyle(
      inline_text_box_.GetLineLayoutItem(), have_selection, paint_info,
      text_style);
  bool paint_selected_text_only = (paint_info.phase == kPaintPhaseSelection);
  bool paint_selected_text_separately =
      !paint_selected_text_only && text_style != selection_style;

  // Set our font.
  const Font& font = style_to_use.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);

  int ascent = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  LayoutPoint text_origin(box_origin.X(), box_origin.Y() + ascent);

  // 1. Paint backgrounds behind text if needed. Examples of such backgrounds
  // include selection and composition highlights.
  if (paint_info.phase != kPaintPhaseSelection &&
      paint_info.phase != kPaintPhaseTextClip && !is_printing) {
    PaintDocumentMarkers(paint_info, box_origin, style_to_use, font,
                         DocumentMarkerPaintPhase::kBackground);

    const LayoutObject& text_box_layout_object = InlineLayoutObject();
    if (have_selection && !PaintsCompositionMarkers(text_box_layout_object)) {
      if (combined_text)
        PaintSelection<InlineTextBoxPainter::PaintOptions::kCombinedText>(
            context, box_rect, style_to_use, font, selection_style.fill_color,
            combined_text);
      else
        PaintSelection<InlineTextBoxPainter::PaintOptions::kNormal>(
            context, box_rect, style_to_use, font, selection_style.fill_color);
    }
  }

  // 2. Now paint the foreground, including text and decorations.
  int selection_start = 0;
  int selection_end = 0;
  if (paint_selected_text_only || paint_selected_text_separately)
    inline_text_box_.SelectionStartEnd(selection_start, selection_end);

  bool respect_hyphen =
      selection_end == static_cast<int>(inline_text_box_.Len()) &&
      inline_text_box_.HasHyphen();
  if (respect_hyphen)
    selection_end = text_run.length();

  bool ltr = inline_text_box_.IsLeftToRightDirection();
  bool flow_is_ltr = inline_text_box_.GetLineLayoutItem()
                         .ContainingBlock()
                         .Style()
                         ->IsLeftToRightDirection();
  if (inline_text_box_.Truncation() != kCNoTruncation) {
    // In a mixed-direction flow the ellipsis is at the start of the text
    // rather than at the end of it.
    selection_start =
        ltr == flow_is_ltr
            ? std::min<int>(selection_start, inline_text_box_.Truncation())
            : std::max<int>(selection_start, inline_text_box_.Truncation());
    selection_end =
        ltr == flow_is_ltr
            ? std::min<int>(selection_end, inline_text_box_.Truncation())
            : std::max<int>(selection_end, inline_text_box_.Truncation());
    length =
        ltr == flow_is_ltr ? inline_text_box_.Truncation() : text_run.length();
  }

  TextPainter text_painter(context, font, text_run, text_origin, box_rect,
                           inline_text_box_.IsHorizontal());
  TextEmphasisPosition emphasis_mark_position;
  bool has_text_emphasis = inline_text_box_.GetEmphasisMarkPosition(
      style_to_use, emphasis_mark_position);
  if (has_text_emphasis)
    text_painter.SetEmphasisMark(style_to_use.TextEmphasisMarkString(),
                                 emphasis_mark_position);
  if (combined_text)
    text_painter.SetCombinedText(combined_text);
  if (inline_text_box_.Truncation() != kCNoTruncation && ltr != flow_is_ltr)
    text_painter.SetEllipsisOffset(inline_text_box_.Truncation());

  if (!paint_selected_text_only) {
    // Paint text decorations except line-through.
    DecorationInfo decoration_info;
    bool has_line_through_decoration = false;
    if (style_to_use.TextDecorationsInEffect() != kTextDecorationNone &&
        inline_text_box_.Truncation() != kCFullTruncation) {
      ComputeDecorationInfo(decoration_info, inline_text_box_, box_origin,
                            style_to_use.AppliedTextDecorations());
      GraphicsContextStateSaver state_saver(context, false);
      PrepareContextForDecoration(context, state_saver,
                                  inline_text_box_.IsHorizontal(), text_style,
                                  combined_text, box_rect);
      PaintDecorationsExceptLineThrough(
          text_painter, has_line_through_decoration, inline_text_box_,
          decoration_info, paint_info, style_to_use.AppliedTextDecorations());
      RestoreContextFromDecoration(context, combined_text, box_rect);
    }

    int start_offset = 0;
    int end_offset = length;
    // Where the text and its flow have opposite directions then our offset into
    // the text given by |truncation| is at the start of the part that will be
    // visible.
    if (inline_text_box_.Truncation() != kCNoTruncation && ltr != flow_is_ltr) {
      start_offset = inline_text_box_.Truncation();
      end_offset = text_run.length();
    }

    if (paint_selected_text_separately && selection_start < selection_end) {
      start_offset = selection_end;
      end_offset = selection_start;
    }

    text_painter.Paint(start_offset, end_offset, length, text_style);

    // Paint line-through decoration if needed.
    if (has_line_through_decoration) {
      GraphicsContextStateSaver state_saver(context, false);
      PrepareContextForDecoration(context, state_saver,
                                  inline_text_box_.IsHorizontal(), text_style,
                                  combined_text, box_rect);
      PaintDecorationsOnlyLineThrough(text_painter, decoration_info, paint_info,
                                      style_to_use.AppliedTextDecorations());
      RestoreContextFromDecoration(context, combined_text, box_rect);
    }
  }

  if ((paint_selected_text_only || paint_selected_text_separately) &&
      selection_start < selection_end) {
    // paint only the text that is selected
    text_painter.Paint(selection_start, selection_end, length, selection_style);
  }

  if (paint_info.phase == kPaintPhaseForeground)
    PaintDocumentMarkers(paint_info, box_origin, style_to_use, font,
                         DocumentMarkerPaintPhase::kForeground);

  if (should_rotate)
    context.ConcatCTM(
        TextPainter::Rotation(box_rect, TextPainter::kCounterclockwise));
}

bool InlineTextBoxPainter::ShouldPaintTextBox(const PaintInfo& paint_info) {
  // When painting selection, we want to include a highlight when the
  // selection spans line breaks. In other cases such as invisible elements
  // or those with no text that are not line breaks, we can skip painting
  // wholesale.
  // TODO(wkorman): Constrain line break painting to appropriate paint phase.
  // This code path is only called in PaintPhaseForeground whereas we would
  // expect PaintPhaseSelection. The existing haveSelection logic in paint()
  // tests for != PaintPhaseTextClip.
  if (inline_text_box_.GetLineLayoutItem().Style()->Visibility() !=
          EVisibility::kVisible ||
      inline_text_box_.Truncation() == kCFullTruncation ||
      !inline_text_box_.Len())
    return false;
  return true;
}

unsigned InlineTextBoxPainter::UnderlinePaintStart(
    const CompositionUnderline& underline) {
  DCHECK(inline_text_box_.Truncation() != kCFullTruncation);
  DCHECK(inline_text_box_.Len());

  // Start painting at the beginning of the text or the specified underline
  // start offset, whichever is higher.
  unsigned paint_start =
      std::max(inline_text_box_.Start(), underline.StartOffset());
  // Cap the maximum paint start to (if no truncation) the last character,
  // else the last character before the truncation ellipsis.
  return std::min(paint_start, (inline_text_box_.Truncation() == kCNoTruncation)
                                   ? inline_text_box_.end()
                                   : inline_text_box_.Start() +
                                         inline_text_box_.Truncation() - 1);
}

unsigned InlineTextBoxPainter::UnderlinePaintEnd(
    const CompositionUnderline& underline) {
  DCHECK(inline_text_box_.Truncation() != kCFullTruncation);
  DCHECK(inline_text_box_.Len());

  // End painting just past the end of the text or the specified underline end
  // offset, whichever is lower.
  unsigned paint_end = std::min(
      inline_text_box_.end() + 1,
      underline.EndOffset());  // end() points at the last char, not past it.
  // Cap the maximum paint end to (if no truncation) one past the last
  // character, else one past the last character before the truncation
  // ellipsis.
  return std::min(paint_end, (inline_text_box_.Truncation() == kCNoTruncation)
                                 ? inline_text_box_.end() + 1
                                 : inline_text_box_.Start() +
                                       inline_text_box_.Truncation());
}

void InlineTextBoxPainter::PaintSingleCompositionBackgroundRun(
    GraphicsContext& context,
    const LayoutPoint& box_origin,
    const ComputedStyle& style,
    const Font& font,
    Color background_color,
    int start_pos,
    int end_pos) {
  if (background_color == Color::kTransparent)
    return;

  int s_pos =
      std::max(start_pos - static_cast<int>(inline_text_box_.Start()), 0);
  int e_pos = std::min(end_pos - static_cast<int>(inline_text_box_.Start()),
                       static_cast<int>(inline_text_box_.Len()));
  if (s_pos >= e_pos)
    return;

  int delta_y =
      (inline_text_box_.GetLineLayoutItem().Style()->IsFlippedLinesWritingMode()
           ? inline_text_box_.Root().SelectionBottom() -
                 inline_text_box_.LogicalBottom()
           : inline_text_box_.LogicalTop() -
                 inline_text_box_.Root().SelectionTop())
          .ToInt();
  int sel_height = inline_text_box_.Root().SelectionHeight().ToInt();
  FloatPoint local_origin(box_origin.X().ToFloat(),
                          box_origin.Y().ToFloat() - delta_y);
  context.DrawHighlightForText(font, inline_text_box_.ConstructTextRun(style),
                               local_origin, sel_height, background_color,
                               s_pos, e_pos);
}

void InlineTextBoxPainter::PaintDocumentMarkers(
    const PaintInfo& paint_info,
    const LayoutPoint& box_origin,
    const ComputedStyle& style,
    const Font& font,
    DocumentMarkerPaintPhase marker_paint_phase) {
  if (!inline_text_box_.GetLineLayoutItem().GetNode())
    return;

  DCHECK(inline_text_box_.Truncation() != kCFullTruncation);
  DCHECK(inline_text_box_.Len());

  DocumentMarkerVector markers =
      inline_text_box_.GetLineLayoutItem().GetDocument().Markers().MarkersFor(
          inline_text_box_.GetLineLayoutItem().GetNode());
  DocumentMarkerVector::const_iterator marker_it = markers.begin();

  // Give any document markers that touch this run a chance to draw before the
  // text has been drawn.  Note end() points at the last char, not one past it
  // like endOffset and ranges do.
  for (; marker_it != markers.end(); ++marker_it) {
    DCHECK(*marker_it);
    const DocumentMarker& marker = **marker_it;

    // Paint either the background markers or the foreground markers, but not
    // both.
    switch (marker.GetType()) {
      case DocumentMarker::kGrammar:
      case DocumentMarker::kSpelling:
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground)
          continue;
        break;
      case DocumentMarker::kTextMatch:
      case DocumentMarker::kComposition:
        break;
      default:
        continue;
    }

    if (marker.EndOffset() <= inline_text_box_.Start()) {
      // marker is completely before this run.  This might be a marker that sits
      // before the first run we draw, or markers that were within runs we
      // skipped due to truncation.
      continue;
    }
    if (marker.StartOffset() > inline_text_box_.end()) {
      // marker is completely after this run, bail.  A later run will paint it.
      break;
    }

    // marker intersects this run.  Paint it.
    switch (marker.GetType()) {
      case DocumentMarker::kSpelling:
        inline_text_box_.PaintDocumentMarker(paint_info.context, box_origin,
                                             marker, style, font, false);
        break;
      case DocumentMarker::kGrammar:
        inline_text_box_.PaintDocumentMarker(paint_info.context, box_origin,
                                             marker, style, font, true);
        break;
      case DocumentMarker::kTextMatch:
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground)
          inline_text_box_.PaintTextMatchMarkerBackground(
              paint_info, box_origin, marker, style, font);
        else
          inline_text_box_.PaintTextMatchMarkerForeground(
              paint_info, box_origin, marker, style, font);
        break;
      case DocumentMarker::kComposition: {
        CompositionUnderline underline(marker.StartOffset(), marker.EndOffset(),
                                       marker.UnderlineColor(), marker.Thick(),
                                       marker.BackgroundColor());
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground)
          PaintSingleCompositionBackgroundRun(
              paint_info.context, box_origin, style, font,
              underline.BackgroundColor(), UnderlinePaintStart(underline),
              UnderlinePaintEnd(underline));
        else
          PaintCompositionUnderline(paint_info.context, box_origin, underline);
      } break;
      default:
        NOTREACHED();
    }
  }
}

namespace {

#if !OS(MACOSX)

sk_sp<PaintRecord> RecordMarker(DocumentMarker::MarkerType marker_type) {
  SkColor color = (marker_type == DocumentMarker::kGrammar)
                      ? SkColorSetRGB(0xC0, 0xC0, 0xC0)
                      : SK_ColorRED;

  // Record the path equivalent to this legacy pattern:
  //   X o   o X o   o X
  //     o X o   o X o

  static const float kW = 4;
  static const float kH = 2;

  // Adjust the phase such that f' == 0 is "pixel"-centered
  // (for optimal rasterization at native rez).
  SkPath path;
  path.moveTo(kW * -3 / 8, kH * 3 / 4);
  path.cubicTo(kW * -1 / 8, kH * 3 / 4,
               kW * -1 / 8, kH * 1 / 4,
               kW *  1 / 8, kH * 1 / 4);
  path.cubicTo(kW * 3 / 8, kH * 1 / 4,
               kW * 3 / 8, kH * 3 / 4,
               kW * 5 / 8, kH * 3 / 4);
  path.cubicTo(kW * 7 / 8, kH * 3 / 4,
               kW * 7 / 8, kH * 1 / 4,
               kW * 9 / 8, kH * 1 / 4);

  PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  flags.setStyle(PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kH * 1 / 2);

  PaintRecorder recorder;
  recorder.beginRecording(kW, kH);
  recorder.getRecordingCanvas()->drawPath(path, flags);

  return recorder.finishRecordingAsPicture();
}

#else  // OS(MACOSX)

sk_sp<PaintRecord> RecordMarker(DocumentMarker::MarkerType marker_type) {
  SkColor color = (marker_type == DocumentMarker::kGrammar)
                      ? SkColorSetRGB(0x6B, 0x6B, 0x6B)
                      : SkColorSetRGB(0xFB, 0x2D, 0x1D);

  // Match the artwork used by the Mac.
  static const float kW = 4;
  static const float kH = 3;
  static const float kR = 1.5f;

  // top->bottom translucent gradient.
  const SkColor colors[2] = {
      SkColorSetARGB(0x48,
                     SkColorGetR(color),
                     SkColorGetG(color),
                     SkColorGetB(color)),
      color
  };
  const SkPoint pts[2] = {
      SkPoint::Make(0, 0),
      SkPoint::Make(0, 2 * kR)
  };

  PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  flags.setShader(SkGradientShader::MakeLinear(
      pts, colors, nullptr, ARRAY_SIZE(colors), SkShader::kClamp_TileMode));
  PaintRecorder recorder;
  recorder.beginRecording(kW, kH);
  recorder.getRecordingCanvas()->drawCircle(kR, kR, kR, flags);

  return recorder.finishRecordingAsPicture();
}

#endif  // OS(MACOSX)

void DrawDocumentMarker(GraphicsContext& context,
                        const FloatPoint& pt,
                        float width,
                        DocumentMarker::MarkerType marker_type,
                        float zoom) {
  DCHECK(marker_type == DocumentMarker::kSpelling ||
         marker_type == DocumentMarker::kGrammar);

  DEFINE_STATIC_LOCAL(PaintRecord*, spelling_marker,
                      (RecordMarker(DocumentMarker::kSpelling).release()));
  DEFINE_STATIC_LOCAL(PaintRecord*, grammar_marker,
                      (RecordMarker(DocumentMarker::kGrammar).release()));
  const auto& marker = marker_type == DocumentMarker::kSpelling
                           ? spelling_marker
                           : grammar_marker;

  // Position already includes zoom and device scale factor.
  SkScalar origin_x = WebCoreFloatToSkScalar(pt.X());
  SkScalar origin_y = WebCoreFloatToSkScalar(pt.Y());

#if OS(MACOSX)
  // Make sure to draw only complete dots, and finish inside the marked text.
  width -= fmodf(width, marker->cullRect().width() * zoom);
#else
  // Offset it vertically by 1 so that there's some space under the text.
  origin_y += 1;
#endif

  const auto rect = SkRect::MakeWH(width, marker->cullRect().height() * zoom);
  const auto local_matrix = SkMatrix::MakeScale(zoom, zoom);

  PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(WrapSkShader(MakePaintShaderRecord(
      sk_ref_sp(marker), SkShader::kRepeat_TileMode, SkShader::kClamp_TileMode,
      &local_matrix, nullptr)));

  // Apply the origin translation as a global transform.  This ensures that the
  // shader local matrix depends solely on zoom => Skia can reuse the same
  // cached tile for all markers at a given zoom level.
  GraphicsContextStateSaver saver(context);
  context.Translate(origin_x, origin_y);
  context.DrawRect(rect, flags);
}

}  // anonymous ns

void InlineTextBoxPainter::PaintDocumentMarker(GraphicsContext& context,
                                               const LayoutPoint& box_origin,
                                               const DocumentMarker& marker,
                                               const ComputedStyle& style,
                                               const Font& font,
                                               bool grammar) {
  // Never print spelling/grammar markers (5327887)
  if (inline_text_box_.GetLineLayoutItem().GetDocument().Printing())
    return;

  if (inline_text_box_.Truncation() == kCFullTruncation)
    return;

  LayoutUnit start;  // start of line to draw, relative to tx
  LayoutUnit width = inline_text_box_.LogicalWidth();  // how much line to draw

  // Determine whether we need to measure text
  bool marker_spans_whole_box = true;
  if (inline_text_box_.Start() <= marker.StartOffset())
    marker_spans_whole_box = false;
  if ((inline_text_box_.end() + 1) !=
      marker.EndOffset())  // end points at the last char, not past it
    marker_spans_whole_box = false;
  if (inline_text_box_.Truncation() != kCNoTruncation)
    marker_spans_whole_box = false;

  if (!marker_spans_whole_box || grammar) {
    int start_position, end_position;
    std::tie(start_position, end_position) =
        GetMarkerPaintOffsets(marker, inline_text_box_);

    if (inline_text_box_.Truncation() != kCNoTruncation)
      end_position = std::min<int>(end_position, inline_text_box_.Truncation());

    // Calculate start & width
    int delta_y = (inline_text_box_.GetLineLayoutItem()
                           .Style()
                           ->IsFlippedLinesWritingMode()
                       ? inline_text_box_.Root().SelectionBottom() -
                             inline_text_box_.LogicalBottom()
                       : inline_text_box_.LogicalTop() -
                             inline_text_box_.Root().SelectionTop())
                      .ToInt();
    int sel_height = inline_text_box_.Root().SelectionHeight().ToInt();
    LayoutPoint start_point(box_origin.X(), box_origin.Y() - delta_y);
    TextRun run = inline_text_box_.ConstructTextRun(style);

    // FIXME: Convert the document markers to float rects.
    IntRect marker_rect = EnclosingIntRect(
        font.SelectionRectForText(run, FloatPoint(start_point), sel_height,
                                  start_position, end_position));
    start = marker_rect.X() - start_point.X();
    width = LayoutUnit(marker_rect.Width());
  }

  // IMPORTANT: The misspelling underline is not considered when calculating the
  // text bounds, so we have to make sure to fit within those bounds.  This
  // means the top pixel(s) of the underline will overlap the bottom pixel(s) of
  // the glyphs in smaller font sizes.  The alternatives are to increase the
  // line spacing (bad!!) or decrease the underline thickness.  The overlap is
  // actually the most useful, and matches what AppKit does.  So, we generally
  // place the underline at the bottom of the text, but in larger fonts that's
  // not so good so we pin to two pixels under the baseline.
  int line_thickness = kMisspellingLineThickness;

  const SimpleFontData* font_data =
      inline_text_box_.GetLineLayoutItem()
          .Style(inline_text_box_.IsFirstLineStyle())
          ->GetFont()
          .PrimaryFont();
  DCHECK(font_data);
  int baseline = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  int descent = (inline_text_box_.LogicalHeight() - baseline).ToInt();
  int underline_offset;
  if (descent <= (line_thickness + 2)) {
    // Place the underline at the very bottom of the text in small/medium fonts.
    underline_offset =
        (inline_text_box_.LogicalHeight() - line_thickness).ToInt();
  } else {
    // In larger fonts, though, place the underline up near the baseline to
    // prevent a big gap.
    underline_offset = baseline + 2;
  }
  DrawDocumentMarker(context,
                     FloatPoint((box_origin.X() + start).ToFloat(),
                                (box_origin.Y() + underline_offset).ToFloat()),
                     width.ToFloat(), marker.GetType(), style.EffectiveZoom());
}

template <InlineTextBoxPainter::PaintOptions options>
void InlineTextBoxPainter::PaintSelection(GraphicsContext& context,
                                          const LayoutRect& box_rect,
                                          const ComputedStyle& style,
                                          const Font& font,
                                          Color text_color,
                                          LayoutTextCombine* combined_text) {
  // See if we have a selection to paint at all.
  int s_pos, e_pos;
  inline_text_box_.SelectionStartEnd(s_pos, e_pos);
  if (s_pos >= e_pos)
    return;

  Color c = inline_text_box_.GetLineLayoutItem().SelectionBackgroundColor();
  if (!c.Alpha())
    return;

  // If the text color ends up being the same as the selection background,
  // invert the selection background.
  if (text_color == c)
    c = Color(0xff - c.Red(), 0xff - c.Green(), 0xff - c.Blue());

  // If the text is truncated, let the thing being painted in the truncation
  // draw its own highlight.
  unsigned start = inline_text_box_.Start();
  int length = inline_text_box_.Len();
  bool ltr = inline_text_box_.IsLeftToRightDirection();
  bool flow_is_ltr = inline_text_box_.GetLineLayoutItem()
                         .ContainingBlock()
                         .Style()
                         ->IsLeftToRightDirection();
  if (inline_text_box_.Truncation() != kCNoTruncation) {
    // In a mixed-direction flow the ellipsis is at the start of the text
    // so we need to start after it. Otherwise we just need to make sure
    // the end of the text is where the ellipsis starts.
    if (ltr != flow_is_ltr)
      s_pos = std::max<int>(s_pos, inline_text_box_.Truncation());
    else
      length = inline_text_box_.Truncation();
  }
  StringView string(inline_text_box_.GetLineLayoutItem().GetText(), start,
                    static_cast<unsigned>(length));

  StringBuilder characters_with_hyphen;
  bool respect_hyphen = e_pos == length && inline_text_box_.HasHyphen();
  TextRun text_run = inline_text_box_.ConstructTextRun(
      style, string,
      inline_text_box_.GetLineLayoutItem().TextLength() -
          inline_text_box_.Start(),
      respect_hyphen ? &characters_with_hyphen : 0);
  if (respect_hyphen)
    e_pos = text_run.length();

  GraphicsContextStateSaver state_saver(context);

  if (options == InlineTextBoxPainter::PaintOptions::kCombinedText) {
    DCHECK(combined_text);
    // We can't use the height of m_inlineTextBox because LayoutTextCombine's
    // inlineTextBox is horizontal within vertical flow
    combined_text->TransformToInlineCoordinates(context, box_rect, true);
    context.DrawHighlightForText(font, text_run,
                                 FloatPoint(box_rect.Location()),
                                 box_rect.Height().ToInt(), c, s_pos, e_pos);
    return;
  }

  LayoutUnit selection_bottom = inline_text_box_.Root().SelectionBottom();
  LayoutUnit selection_top = inline_text_box_.Root().SelectionTop();

  int delta_y = RoundToInt(
      inline_text_box_.GetLineLayoutItem().Style()->IsFlippedLinesWritingMode()
          ? selection_bottom - inline_text_box_.LogicalBottom()
          : inline_text_box_.LogicalTop() - selection_top);
  int sel_height = std::max(0, RoundToInt(selection_bottom - selection_top));

  FloatPoint local_origin(box_rect.X().ToFloat(),
                          (box_rect.Y() - delta_y).ToFloat());
  LayoutRect selection_rect = LayoutRect(font.SelectionRectForText(
      text_run, local_origin, sel_height, s_pos, e_pos));
  // For line breaks, just painting a selection where the line break itself
  // is rendered is sufficient. Don't select it if there's an ellipsis
  // there.
  if (inline_text_box_.HasWrappedSelectionNewline() &&
      inline_text_box_.Truncation() == kCNoTruncation &&
      !inline_text_box_.IsLineBreak())
    ExpandToIncludeNewlineForSelection(selection_rect);

  // Line breaks report themselves as having zero width for layout purposes,
  // and so will end up positioned at (0, 0), even though we paint their
  // selection highlight with character width. For RTL then, we have to
  // explicitly shift the selection rect over to paint in the right location.
  if (!inline_text_box_.IsLeftToRightDirection() &&
      inline_text_box_.IsLineBreak())
    selection_rect.Move(-selection_rect.Width(), LayoutUnit());
  if (!flow_is_ltr && !ltr && inline_text_box_.Truncation() != kCNoTruncation)
    selection_rect.Move(
        inline_text_box_.LogicalWidth() - selection_rect.Width(), LayoutUnit());

  context.FillRect(FloatRect(selection_rect), c);
}

void InlineTextBoxPainter::ExpandToIncludeNewlineForSelection(
    LayoutRect& rect) {
  FloatRectOutsets outsets = FloatRectOutsets();
  float space_width = inline_text_box_.NewlineSpaceWidth();
  if (inline_text_box_.IsLeftToRightDirection())
    outsets.SetRight(space_width);
  else
    outsets.SetLeft(space_width);
  rect.Expand(outsets);
}

void InlineTextBoxPainter::PaintCompositionUnderline(
    GraphicsContext& context,
    const LayoutPoint& box_origin,
    const CompositionUnderline& underline) {
  if (underline.GetColor() == Color::kTransparent)
    return;

  if (inline_text_box_.Truncation() == kCFullTruncation)
    return;

  unsigned paint_start = UnderlinePaintStart(underline);
  unsigned paint_end = UnderlinePaintEnd(underline);
  DCHECK_LT(paint_start, paint_end);

  // start of line to draw
  float start =
      paint_start == inline_text_box_.Start()
          ? 0
          : inline_text_box_.GetLineLayoutItem().Width(
                inline_text_box_.Start(),
                paint_start - inline_text_box_.Start(),
                inline_text_box_.TextPos(),
                inline_text_box_.IsLeftToRightDirection() ? TextDirection::kLtr
                                                          : TextDirection::kRtl,
                inline_text_box_.IsFirstLineStyle());
  // how much line to draw
  float width;
  bool ltr = inline_text_box_.IsLeftToRightDirection();
  bool flow_is_ltr =
      inline_text_box_.GetLineLayoutItem().Style()->IsLeftToRightDirection();
  if (paint_start == inline_text_box_.Start() &&
      paint_end == inline_text_box_.end() + 1) {
    width = inline_text_box_.LogicalWidth().ToFloat();
  } else {
    unsigned paint_from = ltr == flow_is_ltr ? paint_start : paint_end;
    unsigned paint_length =
        ltr == flow_is_ltr
            ? paint_end - paint_start
            : inline_text_box_.Start() + inline_text_box_.Len() - paint_end;
    width = inline_text_box_.GetLineLayoutItem().Width(
        paint_from, paint_length,
        LayoutUnit(inline_text_box_.TextPos() + start),
        flow_is_ltr ? TextDirection::kLtr : TextDirection::kRtl,
        inline_text_box_.IsFirstLineStyle());
  }
  // In RTL mode, start and width are computed from the right end of the text
  // box: starting at |logicalWidth| - |start| and continuing left by |width| to
  // |logicalWidth| - |start| - |width|. We will draw that line, but backwards:
  // |logicalWidth| - |start| - |width| to |logicalWidth| - |start|.
  if (!flow_is_ltr)
    start = inline_text_box_.LogicalWidth().ToFloat() - width - start;

  // Thick marked text underlines are 2px thick as long as there is room for the
  // 2px line under the baseline.  All other marked text underlines are 1px
  // thick.  If there's not enough space the underline will touch or overlap
  // characters.
  int line_thickness = 1;
  const SimpleFontData* font_data =
      inline_text_box_.GetLineLayoutItem()
          .Style(inline_text_box_.IsFirstLineStyle())
          ->GetFont()
          .PrimaryFont();
  DCHECK(font_data);
  int baseline = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  if (underline.Thick() && inline_text_box_.LogicalHeight() - baseline >= 2)
    line_thickness = 2;

  // We need to have some space between underlines of subsequent clauses,
  // because some input methods do not use different underline styles for those.
  // We make each line shorter, which has a harmless side effect of shortening
  // the first and last clauses, too.
  start += 1;
  width -= 2;

  context.SetStrokeColor(underline.GetColor());
  context.SetStrokeThickness(line_thickness);
  context.DrawLineForText(
      FloatPoint(
          box_origin.X() + start,
          (box_origin.Y() + inline_text_box_.LogicalHeight() - line_thickness)
              .ToFloat()),
      width);
}

void InlineTextBoxPainter::PaintTextMatchMarkerForeground(
    const PaintInfo& paint_info,
    const LayoutPoint& box_origin,
    const DocumentMarker& marker,
    const ComputedStyle& style,
    const Font& font) {
  if (!InlineLayoutObject()
           .GetFrame()
           ->GetEditor()
           .MarkedTextMatchesAreHighlighted())
    return;

  const auto paint_offsets = GetMarkerPaintOffsets(marker, inline_text_box_);
  TextRun run = inline_text_box_.ConstructTextRun(style);

  Color text_color =
      LayoutTheme::GetTheme().PlatformTextSearchColor(marker.IsActiveMatch());
  if (style.VisitedDependentColor(CSSPropertyColor) == text_color)
    return;

  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  TextPainter::Style text_style;
  text_style.current_color = text_style.fill_color = text_style.stroke_color =
      text_style.emphasis_mark_color = text_color;
  text_style.stroke_width = style.TextStrokeWidth();
  text_style.shadow = 0;

  LayoutRect box_rect(box_origin, LayoutSize(inline_text_box_.LogicalWidth(),
                                             inline_text_box_.LogicalHeight()));
  LayoutPoint text_origin(
      box_origin.X(), box_origin.Y() + font_data->GetFontMetrics().Ascent());
  TextPainter text_painter(paint_info.context, font, run, text_origin, box_rect,
                           inline_text_box_.IsHorizontal());

  text_painter.Paint(paint_offsets.first, paint_offsets.second,
                     inline_text_box_.Len(), text_style);
}

void InlineTextBoxPainter::PaintTextMatchMarkerBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& box_origin,
    const DocumentMarker& marker,
    const ComputedStyle& style,
    const Font& font) {
  if (!LineLayoutAPIShim::LayoutObjectFrom(inline_text_box_.GetLineLayoutItem())
           ->GetFrame()
           ->GetEditor()
           .MarkedTextMatchesAreHighlighted())
    return;

  const auto paint_offsets = GetMarkerPaintOffsets(marker, inline_text_box_);
  TextRun run = inline_text_box_.ConstructTextRun(style);

  Color color = LayoutTheme::GetTheme().PlatformTextSearchHighlightColor(
      marker.IsActiveMatch());
  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);

  LayoutRect box_rect(box_origin, LayoutSize(inline_text_box_.LogicalWidth(),
                                             inline_text_box_.LogicalHeight()));
  context.Clip(FloatRect(box_rect));
  context.DrawHighlightForText(font, run, FloatPoint(box_origin),
                               box_rect.Height().ToInt(), color,
                               paint_offsets.first, paint_offsets.second);
}

}  // namespace blink
