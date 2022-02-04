// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_fragment_painter.h"

#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_run.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_text.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/paint/document_marker_painter.h"
#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"
#include "third_party/blink/renderer/core/paint/inline_text_box_painter.h"
#include "third_party/blink/renderer/core/paint/list_marker_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_highlight_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/selection_bounds_recorder.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

namespace {

inline const DisplayItemClient& AsDisplayItemClient(
    const NGInlineCursor& cursor,
    bool for_selection) {
  if (UNLIKELY(for_selection)) {
    if (const auto* selection_client =
            cursor.Current().GetSelectionDisplayItemClient())
      return *selection_client;
  }
  return *cursor.Current().GetDisplayItemClient();
}

inline PhysicalRect BoxInPhysicalSpace(
    const NGInlineCursor& cursor,
    const PhysicalOffset& paint_offset,
    const PhysicalOffset& parent_offset,
    const LayoutNGTextCombine* text_combine) {
  PhysicalRect box_rect;
  if (const auto* svg_data = cursor.CurrentItem()->SvgFragmentData())
    box_rect = PhysicalRect::FastAndLossyFromRectF(svg_data->rect);
  else
    box_rect = cursor.CurrentItem()->RectInContainerFragment();
  box_rect.offset.left += paint_offset.left;
  // We round the y-axis to ensure consistent line heights.
  box_rect.offset.top =
      LayoutUnit((paint_offset.top + parent_offset.top).Round()) +
      (box_rect.offset.top - parent_offset.top);
  if (text_combine) {
    box_rect.offset.left =
        text_combine->AdjustTextLeftForPaint(box_rect.offset.left);
  }
  return box_rect;
}

inline PhysicalRect BoxInWritingModeSpace(const PhysicalRect& physical_box,
                                          bool is_horizontal) {
  PhysicalRect result = physical_box;
  if (!is_horizontal)
    result.size = PhysicalSize(result.Height(), result.Width());
  return result;
}

inline const NGInlineCursor& InlineCursorForBlockFlow(
    const NGInlineCursor& cursor,
    absl::optional<NGInlineCursor>* storage) {
  if (*storage)
    return **storage;
  *storage = cursor;
  (*storage)->ExpandRootToContainingBlock();
  return **storage;
}

// Check if text-emphasis and ruby annotation text are on different sides.
// See InlineTextBox::GetEmphasisMarkPosition().
//
// TODO(layout-dev): The current behavior is compatible with the legacy layout.
// However, the specification asks to draw emphasis marks over ruby annotation
// text.
// https://drafts.csswg.org/css-text-decor-4/#text-emphasis-position-property
bool ShouldPaintEmphasisMark(const ComputedStyle& style,
                             const LayoutObject& layout_object) {
  if (style.GetTextEmphasisMark() == TextEmphasisMark::kNone)
    return false;
  // Note: We set text-emphasis-style:none for combined text and we paint
  // emphasis mark at left/right side of |LayoutNGTextCombine|.
  DCHECK(!IsA<LayoutNGTextCombine>(layout_object.Parent()));
  const LayoutObject* containing_block = layout_object.ContainingBlock();
  if (!containing_block || !containing_block->IsRubyBase())
    return true;
  const LayoutObject* parent = containing_block->Parent();
  if (!parent || !parent->IsRubyRun())
    return true;
  const LayoutRubyText* ruby_text = To<LayoutRubyRun>(parent)->RubyText();
  if (!ruby_text)
    return true;
  if (!NGInlineCursor(*ruby_text))
    return true;
  const LineLogicalSide ruby_logical_side =
      parent->StyleRef().GetRubyPosition() == RubyPosition::kBefore
          ? LineLogicalSide::kOver
          : LineLogicalSide::kUnder;
  return ruby_logical_side != style.GetTextEmphasisLineLogicalSide();
}

}  // namespace

void NGTextFragmentPainter::PaintSymbol(const LayoutObject* layout_object,
                                        const ComputedStyle& style,
                                        const PhysicalSize box_size,
                                        const PaintInfo& paint_info,
                                        const PhysicalOffset& paint_offset) {
  PhysicalRect marker_rect(
      ListMarker::RelativeSymbolMarkerRect(style, box_size.width));
  marker_rect.Move(paint_offset);
  ListMarkerPainter::PaintSymbol(paint_info, layout_object, style,
                                 marker_rect.ToLayoutRect());
}

// This is copied from InlineTextBoxPainter::PaintSelection() but lacks of
// ltr, expanding new line wrap or so which uses InlineTextBox functions.
void NGTextFragmentPainter::Paint(const PaintInfo& paint_info,
                                  const PhysicalOffset& paint_offset) {
  const auto& text_item = *cursor_.CurrentItem();
  // We can skip painting if the fragment (including selection) is invisible.
  if (!text_item.TextLength())
    return;

  if (!text_item.TextShapeResult() &&
      // A line break's selection tint is still visible.
      !text_item.IsLineBreak())
    return;

  const ComputedStyle& style = text_item.Style();
  if (style.Visibility() != EVisibility::kVisible)
    return;

  const NGTextFragmentPaintInfo& fragment_paint_info =
      cursor_.Current()->TextPaintInfo(cursor_.Items());
  const LayoutObject* layout_object = text_item.GetLayoutObject();
  const Document& document = layout_object->GetDocument();
  const bool is_printing = document.Printing();
  // Don't paint selections when rendering a mask, clip-path (as a mask),
  // pattern or feImage (element reference.)
  const bool is_rendering_resource = paint_info.IsRenderingResourceSubtree();
  const auto* const text_combine =
      DynamicTo<LayoutNGTextCombine>(layout_object->Parent());
  const PhysicalRect physical_box =
      BoxInPhysicalSpace(cursor_, paint_offset, parent_offset_, text_combine);
#if DCHECK_IS_ON()
  if (UNLIKELY(text_combine))
    LayoutNGTextCombine::AssertStyleIsValid(style);
#endif

  // Determine whether or not we’ll need a writing-mode rotation, but don’t
  // actually rotate until we reach the steps that need it.
  absl::optional<AffineTransform> rotation;
  const WritingMode writing_mode = style.GetWritingMode();
  const bool is_horizontal = IsHorizontalWritingMode(writing_mode);
  const PhysicalRect rotated_box =
      BoxInWritingModeSpace(physical_box, is_horizontal);
  if (!is_horizontal)
    rotation.emplace(TextPainterBase::Rotation(rotated_box, writing_mode));

  // Determine whether or not we're selected.
  NGHighlightPainter::SelectionPaintState* selection = nullptr;
  absl::optional<NGHighlightPainter::SelectionPaintState>
      selection_for_bounds_recording;
  if (UNLIKELY(!is_printing && !is_rendering_resource &&
               paint_info.phase != PaintPhase::kTextClip &&
               layout_object->IsSelected())) {
    const NGInlineCursor& root_inline_cursor =
        InlineCursorForBlockFlow(cursor_, &inline_cursor_for_block_flow_);

    // Empty selections might be the boundary of the document selection, and
    // thus need to get recorded. We only need to paint the selection if it
    // has a valid range.
    selection_for_bounds_recording.emplace(root_inline_cursor,
                                           physical_box.offset, rotation);
    if (selection_for_bounds_recording->Status().HasValidRange())
      selection = &selection_for_bounds_recording.value();
  }
  if (!selection) {
    // When only painting the selection drag image, don't bother to paint if
    // there is none.
    if (paint_info.phase == PaintPhase::kSelectionDragImage)
      return;

    // Flow controls (line break, tab, <wbr>) need only selection painting.
    if (text_item.IsFlowControl())
      return;
  }

  gfx::Rect visual_rect;
  const auto* const svg_inline_text =
      DynamicTo<LayoutSVGInlineText>(layout_object);
  float scaling_factor = 1.0f;
  if (UNLIKELY(svg_inline_text)) {
    DCHECK_EQ(text_item.Type(), NGFragmentItem::kSvgText);
    scaling_factor = svg_inline_text->ScalingFactor();
    DCHECK_NE(scaling_factor, 0.0f);
    visual_rect = gfx::ToEnclosingRect(
        svg_inline_text->Parent()->VisualRectInLocalSVGCoordinates());
  } else {
    DCHECK_NE(text_item.Type(), NGFragmentItem::kSvgText);
    PhysicalRect ink_overflow = text_item.SelfInkOverflow();
    ink_overflow.Move(physical_box.offset);
    visual_rect = ToEnclosingRect(ink_overflow);
  }

  // Ensure the selection bounds are recorded on the paint chunk regardless of
  // whether the display item that contains the actual selection painting is
  // reused.
  absl::optional<SelectionBoundsRecorder> selection_recorder;
  if (UNLIKELY(selection_for_bounds_recording &&
               paint_info.phase == PaintPhase::kForeground && !is_printing)) {
    if (SelectionBoundsRecorder::ShouldRecordSelection(
            cursor_.Current().GetLayoutObject()->GetFrame()->Selection(),
            selection_for_bounds_recording->State())) {
      selection_recorder.emplace(
          selection_for_bounds_recording->State(),
          selection_for_bounds_recording->RectInPhysicalSpace(),
          paint_info.context.GetPaintController(),
          cursor_.Current().ResolvedDirection(), style.GetWritingMode(),
          *cursor_.Current().GetLayoutObject());
    }
  }

  // This is declared after selection_recorder so that this will be destructed
  // before selection_recorder to ensure the selection is painted before
  // selection_recorder records the selection bounds.
  absl::optional<DrawingRecorder> recorder;
  const auto& display_item_client =
      AsDisplayItemClient(cursor_, selection != nullptr);
  // Text clips are initiated only in BoxPainterBase::PaintFillLayer, which is
  // already within a DrawingRecorder.
  if (paint_info.phase != PaintPhase::kTextClip) {
    if (LIKELY(!paint_info.context.InDrawingRecorder())) {
      if (DrawingRecorder::UseCachedDrawingIfPossible(
              paint_info.context, display_item_client, paint_info.phase)) {
        return;
      }
      recorder.emplace(paint_info.context, display_item_client,
                       paint_info.phase, visual_rect);
    }
  }

  if (UNLIKELY(text_item.IsSymbolMarker())) {
    // The NGInlineItem of marker might be Split(). To avoid calling PaintSymbol
    // multiple times, only call it the first time. For an outside marker, this
    // is when StartOffset is 0. But for an inside marker, the first StartOffset
    // can be greater due to leading bidi control characters like U+202A/U+202B,
    // U+202D/U+202E, U+2066/U+2067 or U+2068.
    DCHECK_LT(fragment_paint_info.from, fragment_paint_info.text.length());
    for (unsigned i = 0; i < fragment_paint_info.from; ++i) {
      if (!Character::IsBidiControl(fragment_paint_info.text.CodepointAt(i)))
        return;
    }
    PaintSymbol(layout_object, style, physical_box.size, paint_info,
                physical_box.offset);
    return;
  }

  GraphicsContext& context = paint_info.context;

  // Determine text colors.

  Node* node = layout_object->GetNode();
  TextPaintStyle text_style =
      TextPainterBase::TextPaintingStyle(document, style, paint_info);
  if (UNLIKELY(selection)) {
    selection->ComputeSelectionStyle(document, style, node, paint_info,
                                     text_style);
  }

  // Set our font.
  const Font& font =
      UNLIKELY(text_combine && text_combine->UsesCompressedFont())
          ? text_combine->CompressedFont()
          : text_item.ScaledFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);

  const bool paint_marker_backgrounds =
      paint_info.phase != PaintPhase::kSelectionDragImage &&
      paint_info.phase != PaintPhase::kTextClip && !is_printing;
  GraphicsContextStateSaver state_saver(context, /*save_and_restore=*/false);
  const int ascent = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  PhysicalOffset text_origin(
      physical_box.offset.left,
      UNLIKELY(text_combine)
          ? text_combine->AdjustTextTopForPaint(physical_box.offset.top)
          : physical_box.offset.top + ascent);

  NGTextPainter text_painter(context, font, fragment_paint_info, visual_rect,
                             text_origin, physical_box, is_horizontal);
  NGHighlightPainter highlight_painter(
      text_painter, paint_info, cursor_, *cursor_.CurrentItem(),
      physical_box.offset, style, selection, is_printing);

  if (svg_inline_text) {
    NGTextPainter::SvgTextPaintState& svg_state = text_painter.SetSvgState(
        *svg_inline_text, style, text_item.StyleVariant(),
        paint_info.IsRenderingClipPathAsMaskImage());

    if (scaling_factor != 1.0f) {
      state_saver.SaveIfNeeded();
      context.Scale(1 / scaling_factor, 1 / scaling_factor);
      svg_state.EnsureShaderTransform().Scale(scaling_factor);
    }
    if (text_item.HasSvgTransformForPaint()) {
      state_saver.SaveIfNeeded();
      const auto fragment_transform = text_item.BuildSvgTransformForPaint();
      context.ConcatCTM(fragment_transform);
      DCHECK(fragment_transform.IsInvertible());
      svg_state.EnsureShaderTransform().PreMultiply(
          fragment_transform.Inverse());
    }
  }

  // 1. Paint backgrounds for document markers that don’t participate in the CSS
  // highlight overlay system, such as composition highlights. They use physical
  // coordinates, so are painted before GraphicsContext rotation.
  highlight_painter.Paint(NGHighlightPainter::kBackground);

  if (rotation) {
    state_saver.SaveIfNeeded();
    context.ConcatCTM(*rotation);
    if (NGTextPainter::SvgTextPaintState* state = text_painter.GetSvgState()) {
      DCHECK(rotation->IsInvertible());
      state->EnsureShaderTransform().PreMultiply(rotation->Inverse());
    }
  }

  if (UNLIKELY(highlight_painter.Selection())) {
    PhysicalRect physical_selection =
        highlight_painter.Selection()->RectInPhysicalSpace();
    if (scaling_factor != 1.0f) {
      physical_selection.offset.Scale(1 / scaling_factor);
      physical_selection.size.Scale(1 / scaling_factor);
    }

    // We need to use physical coordinates when invalidating.
    if (paint_marker_backgrounds && recorder) {
      recorder->UniteVisualRect(ToEnclosingRect(physical_selection));
    }
  }

  NGTextDecorationPainter decoration_painter(
      text_painter, text_item, paint_info, style, text_style, rotated_box,
      highlight_painter.Selection());

  // 2. Now paint the foreground, including text and decorations.
  // TODO(dazabani@igalia.com): suppress text proper where one or more highlight
  // overlays are active, but paint shadows in full <https://crbug.com/1147859>
  if (ShouldPaintEmphasisMark(style, *layout_object)) {
    text_painter.SetEmphasisMark(style.TextEmphasisMarkString(),
                                 style.GetTextEmphasisPosition());
  }

  DOMNodeId node_id = kInvalidDOMNodeId;
  if (node) {
    if (auto* layout_text = DynamicTo<LayoutText>(node->GetLayoutObject()))
      node_id = layout_text->EnsureNodeId();
  }

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kForeground));

  const unsigned length = fragment_paint_info.to - fragment_paint_info.from;
  const unsigned start_offset = fragment_paint_info.from;
  const unsigned end_offset = fragment_paint_info.to;

  if (LIKELY(!highlight_painter.Selection())) {
    decoration_painter.Begin(NGTextDecorationPainter::kOriginating);
    decoration_painter.PaintExceptLineThrough();
    text_painter.Paint(start_offset, end_offset, length, text_style, node_id,
                       auto_dark_mode);
    decoration_painter.PaintOnlyLineThrough();
  } else if (!highlight_painter.Selection()->ShouldPaintSelectedTextOnly()) {
    decoration_painter.Begin(NGTextDecorationPainter::kOriginating);
    decoration_painter.PaintExceptLineThrough();
    highlight_painter.Selection()->PaintSuppressingTextProperWhereSelected(
        text_painter, start_offset, end_offset, length, text_style, node_id,
        auto_dark_mode);
    decoration_painter.PaintOnlyLineThrough();
  }

  // 3. Paint CSS highlight overlays, such as ::selection and ::target-text.
  // For each overlay, we paint its background, then its shadows, then the text
  // with any decorations it defines, and all of the ::selection overlay parts
  // are painted over any ::target-text overlay parts, and so on. The text
  // proper (as opposed to shadows) is only painted by the topmost overlay
  // applying to a piece of text (if any), and suppressed everywhere else.
  // TODO(dazabani@igalia.com): implement this for the other highlight pseudos
  if (UNLIKELY(highlight_painter.Selection())) {
    if (paint_marker_backgrounds) {
      highlight_painter.Selection()->PaintSelectionBackground(
          context, node, document, style, rotation);
    }
  }

  if (paint_info.phase == PaintPhase::kForeground) {
    highlight_painter.Paint(NGHighlightPainter::kForeground);
  }

  if (UNLIKELY(highlight_painter.Selection())) {
    // Paint only the text that is selected.
    decoration_painter.Begin(NGTextDecorationPainter::kSelection);
    decoration_painter.PaintExceptLineThrough();
    highlight_painter.Selection()->PaintSelectedText(
        text_painter, length, text_style, node_id, auto_dark_mode);
    decoration_painter.PaintOnlyLineThrough();
  }
}

}  // namespace blink
