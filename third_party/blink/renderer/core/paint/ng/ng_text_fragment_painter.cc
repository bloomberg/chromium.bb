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
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/paint/document_marker_painter.h"
#include "third_party/blink/renderer/core/paint/inline_text_box_painter.h"
#include "third_party/blink/renderer/core/paint/list_marker_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/selection_painting_utils.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

namespace {

Color SelectionBackgroundColor(const Document& document,
                               const ComputedStyle& style,
                               Node* node,
                               Color text_color) {
  const Color color =
      SelectionPaintingUtils::SelectionBackgroundColor(document, style, node);
  if (!color.Alpha())
    return Color();

  // If the text color ends up being the same as the selection background,
  // invert the selection background.
  if (text_color == color)
    return Color(0xff - color.Red(), 0xff - color.Green(), 0xff - color.Blue());
  return color;
}

// TODO(yosin): Remove |AsDisplayItemClient| once the transition to
// |NGFragmentItem| is done. http://crbug.com/982194
inline const NGFragmentItem& AsDisplayItemClient(const NGInlineCursor& cursor) {
  return *cursor.CurrentItem();
}

inline const NGPaintFragment& AsDisplayItemClient(
    const NGTextPainterCursor& cursor) {
  return cursor.PaintFragment();
}

inline PhysicalRect ComputeBoxRect(const NGInlineCursor& cursor,
                                   const PhysicalOffset& paint_offset,
                                   const PhysicalOffset& parent_offset) {
  PhysicalRect box_rect = cursor.CurrentItem()->RectInContainerBlock();
  box_rect.offset.left += paint_offset.left;
  // We round the y-axis to ensure consistent line heights.
  box_rect.offset.top =
      LayoutUnit((paint_offset.top + parent_offset.top).Round()) +
      (box_rect.offset.top - parent_offset.top);
  return box_rect;
}

inline PhysicalRect ComputeBoxRect(const NGTextPainterCursor& cursor,
                                   const PhysicalOffset& paint_offset,
                                   const PhysicalOffset& parent_offset) {
  PhysicalRect box_rect = cursor.PaintFragment().Rect();
  // We round the y-axis to ensure consistent line heights.
  PhysicalOffset adjusted_paint_offset(paint_offset.left,
                                       LayoutUnit(paint_offset.top.Round()));
  box_rect.offset += adjusted_paint_offset;
  return box_rect;
}

inline const NGInlineCursor& InlineCursorForBlockFlow(
    const NGInlineCursor& cursor,
    base::Optional<NGInlineCursor>* storage) {
  if (*storage)
    return **storage;
  *storage = cursor;
  (*storage)->ExpandRootToContainingBlock();
  return **storage;
}

inline const NGInlineCursor& InlineCursorForBlockFlow(
    const NGTextPainterCursor& cursor,
    base::Optional<NGInlineCursor>* storage) {
  if (*storage)
    return **storage;
  storage->emplace(cursor.RootPaintFragment());
  (*storage)->MoveTo(cursor.PaintFragment());
  return **storage;
}

// TODO(yosin): Remove |GetTextFragmentPaintInfo| once the transition to
// |NGFragmentItem| is done. http://crbug.com/982194
inline NGTextFragmentPaintInfo GetTextFragmentPaintInfo(
    const NGInlineCursor& cursor) {
  return cursor.CurrentItem()->TextPaintInfo(cursor.Items());
}

inline NGTextFragmentPaintInfo GetTextFragmentPaintInfo(
    const NGTextPainterCursor& cursor) {
  return cursor.CurrentItem()->PaintInfo();
}

// TODO(yosin): Remove |GetLineLeftAndRightForOffsets| once the transition to
// |NGFragmentItem| is done. http://crbug.com/982194
inline std::pair<LayoutUnit, LayoutUnit> GetLineLeftAndRightForOffsets(
    const NGFragmentItem& text_item,
    StringView text,
    unsigned start_offset,
    unsigned end_offset) {
  return text_item.LineLeftAndRightForOffsets(text, start_offset, end_offset);
}

inline std::pair<LayoutUnit, LayoutUnit> GetLineLeftAndRightForOffsets(
    const NGPhysicalTextFragment& text_fragment,
    StringView text,
    unsigned start_offset,
    unsigned end_offset) {
  return text_fragment.LineLeftAndRightForOffsets(start_offset, end_offset);
}

// TODO(yosin): Remove |ComputeLayoutSelectionStatus| once the transition to
// |NGFragmentItem| is done. http://crbug.com/982194
inline LayoutSelectionStatus ComputeLayoutSelectionStatus(
    const NGInlineCursor& cursor) {
  return cursor.Current()
      .GetLayoutObject()
      ->GetDocument()
      .GetFrame()
      ->Selection()
      .ComputeLayoutSelectionStatus(cursor);
}

// TODO(yosin): Remove |ComputeLocalRect| once the transition to
// |NGFragmentItem| is done. http://crbug.com/982194
inline PhysicalRect ComputeLocalRect(const NGFragmentItem& text_item,
                                     StringView text,
                                     unsigned start_offset,
                                     unsigned end_offset) {
  return text_item.LocalRect(text, start_offset, end_offset);
}

inline PhysicalRect ComputeLocalRect(
    const NGPhysicalTextFragment& text_fragment,
    StringView text,
    unsigned start_offset,
    unsigned end_offset) {
  return text_fragment.LocalRect(start_offset, end_offset);
}

DocumentMarkerVector ComputeMarkersToPaint(Node* node, bool is_ellipsis) {
  // TODO(yoichio): Handle first-letter
  auto* text_node = DynamicTo<Text>(node);
  if (!text_node)
    return DocumentMarkerVector();
  // We don't paint any marker on ellipsis.
  if (is_ellipsis)
    return DocumentMarkerVector();

  DocumentMarkerController& document_marker_controller =
      node->GetDocument().Markers();
  return document_marker_controller.ComputeMarkersToPaint(*text_node);
}

unsigned GetTextContentOffset(const Text& text, unsigned offset) {
  // TODO(yoichio): Sanitize DocumentMarker around text length.
  const Position position(text, std::min(offset, text.length()));
  const NGOffsetMapping* const offset_mapping =
      NGOffsetMapping::GetFor(position);
  DCHECK(offset_mapping);
  const base::Optional<unsigned>& ng_offset =
      offset_mapping->GetTextContentOffset(position);
  DCHECK(ng_offset.has_value());
  return ng_offset.value();
}

// ClampOffset modifies |offset| fixed in a range of |text_fragment| start/end
// offsets.
// |offset| points not each character but each span between character.
// With that concept, we can clear catch what is inside start / end.
// Suppose we have "foo_bar"('_' is a space).
// There are 8 offsets for that:
//  f o o _ b a r
// 0 1 2 3 4 5 6 7
// If "bar" is a TextFragment. That start(), end() {4, 7} correspond this
// offset. If a marker has StartOffset / EndOffset as {2, 6},
// ClampOffset returns{ 4,6 }, which represents "ba" on "foo_bar".
template <typename TextItem>
unsigned ClampOffset(unsigned offset, const TextItem& text_fragment) {
  return std::min(std::max(offset, text_fragment.StartOffset()),
                  text_fragment.EndOffset());
}

void PaintRect(GraphicsContext& context,
               const PhysicalRect& rect,
               const Color color) {
  if (!color.Alpha())
    return;
  if (rect.size.IsEmpty())
    return;
  const IntRect pixel_snapped_rect = PixelSnappedIntRect(rect);
  if (!pixel_snapped_rect.IsEmpty())
    context.FillRect(pixel_snapped_rect, color);
}

void PaintRect(GraphicsContext& context,
               const PhysicalOffset& location,
               const PhysicalRect& rect,
               const Color color) {
  PaintRect(context, PhysicalRect(rect.offset + location, rect.size), color);
}

template <typename TextItem>
PhysicalRect MarkerRectForForeground(const TextItem& text_fragment,
                                     StringView text,
                                     unsigned start_offset,
                                     unsigned end_offset) {
  LayoutUnit start_position, end_position;
  std::tie(start_position, end_position) = GetLineLeftAndRightForOffsets(
      text_fragment, text, start_offset, end_offset);

  const LayoutUnit height = text_fragment.Size()
                                .ConvertToLogical(static_cast<WritingMode>(
                                    text_fragment.Style().GetWritingMode()))
                                .block_size;
  return {start_position, LayoutUnit(), end_position - start_position, height};
}

// Copied from InlineTextBoxPainter
template <typename TextItem>
void PaintDocumentMarkers(GraphicsContext& context,
                          const TextItem& text_fragment,
                          StringView text,
                          const DocumentMarkerVector& markers_to_paint,
                          const PhysicalOffset& box_origin,
                          const ComputedStyle& style,
                          DocumentMarkerPaintPhase marker_paint_phase,
                          NGTextPainter* text_painter) {
  if (markers_to_paint.IsEmpty())
    return;

  DCHECK(text_fragment.GetNode());
  const auto& text_node = To<Text>(*text_fragment.GetNode());
  for (const DocumentMarker* marker : markers_to_paint) {
    const unsigned marker_start_offset =
        GetTextContentOffset(text_node, marker->StartOffset());
    const unsigned marker_end_offset =
        GetTextContentOffset(text_node, marker->EndOffset());
    const unsigned paint_start_offset =
        ClampOffset(marker_start_offset, text_fragment);
    const unsigned paint_end_offset =
        ClampOffset(marker_end_offset, text_fragment);
    if (paint_start_offset == paint_end_offset)
      continue;

    switch (marker->GetType()) {
      case DocumentMarker::kSpelling:
      case DocumentMarker::kGrammar: {
        if (context.Printing())
          break;
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground)
          continue;
        DocumentMarkerPainter::PaintDocumentMarker(
            context, box_origin, style, marker->GetType(),
            MarkerRectForForeground(text_fragment, text, paint_start_offset,
                                    paint_end_offset));
      } break;

      case DocumentMarker::kTextFragment:
      case DocumentMarker::kTextMatch: {
        if (marker->GetType() == DocumentMarker::kTextMatch &&
            !text_fragment.GetNode()
                 ->GetDocument()
                 .GetFrame()
                 ->GetEditor()
                 .MarkedTextMatchesAreHighlighted())
          break;
        const auto& text_marker = To<TextMarkerBase>(*marker);
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground) {
          const Color color =
              LayoutTheme::GetTheme().PlatformTextSearchHighlightColor(
                  text_marker.IsActiveMatch(),
                  text_fragment.GetNode()->GetDocument().InForcedColorsMode(),
                  style.UsedColorScheme());
          PaintRect(context, PhysicalOffset(box_origin),
                    ComputeLocalRect(text_fragment, text, paint_start_offset,
                                     paint_end_offset),
                    color);
          break;
        }

        const TextPaintStyle text_style =
            DocumentMarkerPainter::ComputeTextPaintStyleFrom(
                style, text_marker,
                text_fragment.GetNode()->GetDocument().InForcedColorsMode());
        if (text_style.current_color == Color::kTransparent)
          break;
        text_painter->Paint(paint_start_offset, paint_end_offset,
                            paint_end_offset - paint_start_offset, text_style,
                            kInvalidDOMNodeId);
      } break;

      case DocumentMarker::kComposition:
      case DocumentMarker::kActiveSuggestion:
      case DocumentMarker::kSuggestion: {
        const auto& styleable_marker = To<StyleableMarker>(*marker);
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground) {
          PaintRect(context, PhysicalOffset(box_origin),
                    ComputeLocalRect(text_fragment, text, paint_start_offset,
                                     paint_end_offset),
                    styleable_marker.BackgroundColor());
          break;
        }
        const SimpleFontData* font_data = style.GetFont().PrimaryFont();
        DocumentMarkerPainter::PaintStyleableMarkerUnderline(
            context, box_origin, styleable_marker, style,
            FloatRect(MarkerRectForForeground(
                text_fragment, text, paint_start_offset, paint_end_offset)),
            LayoutUnit(font_data->GetFontMetrics().Height()));
      } break;

      default:
        NOTREACHED();
        break;
    }
  }
}

class SelectionPaintState {
  STACK_ALLOCATED();

 public:
  explicit SelectionPaintState(const NGInlineCursor& containing_block)
      : selection_status_(ComputeLayoutSelectionStatus(containing_block)),
        containing_block_(containing_block) {}

  const LayoutSelectionStatus& Status() const { return selection_status_; }

  bool ShouldPaintSelectedTextOnly() const { return paint_selected_text_only_; }

  bool ShouldPaintSelectedTextSeparately() const {
    return paint_selected_text_separately_;
  }

  bool IsSelectionRectComputed() const { return selection_rect_.has_value(); }

  void ComputeSelectionStyle(const Document& document,
                             const ComputedStyle& style,
                             Node* node,
                             const PaintInfo& paint_info,
                             const TextPaintStyle& text_style) {
    selection_style_ = TextPainterBase::SelectionPaintingStyle(
        document, style, node, /*have_selection*/ true, paint_info, text_style);
    paint_selected_text_only_ =
        (paint_info.phase == PaintPhase::kSelectionDragImage);
    paint_selected_text_separately_ =
        !paint_selected_text_only_ && text_style != selection_style_;
  }

  void ComputeSelectionRect(const PhysicalOffset& box_offset) {
    DCHECK(!selection_rect_);
    selection_rect_ =
        ComputeLocalSelectionRectForText(containing_block_, selection_status_);
    selection_rect_->offset += box_offset;
  }

  // Logic is copied from InlineTextBoxPainter::PaintSelection.
  // |selection_start| and |selection_end| should be between
  // [text_fragment.StartOffset(), text_fragment.EndOffset()].
  void PaintSelectionBackground(GraphicsContext& context,
                                Node* node,
                                const Document& document,
                                const ComputedStyle& style) {
    const Color color = SelectionBackgroundColor(document, style, node,
                                                 selection_style_.fill_color);
    PaintRect(context, *selection_rect_, color);
  }

  // Paint the selected text only.
  void PaintSelectedText(NGTextPainter& text_painter,
                         unsigned length,
                         const TextPaintStyle& text_style,
                         DOMNodeId node_id) {
    text_painter.PaintSelectedText(selection_status_.start,
                                   selection_status_.end, length, text_style,
                                   selection_style_, *selection_rect_, node_id);
  }

  // Paint the text except selected parts. Does nothing if all is selected.
  void PaintBeforeAndAfterSelectedText(NGTextPainter& text_painter,
                                       unsigned start_offset,
                                       unsigned end_offset,
                                       unsigned length,
                                       const TextPaintStyle& text_style,
                                       DOMNodeId node_id) {
    if (start_offset < selection_status_.start) {
      text_painter.Paint(start_offset, selection_status_.start, length,
                         text_style, node_id);
    }
    if (selection_status_.end < end_offset) {
      text_painter.Paint(selection_status_.end, end_offset, length, text_style,
                         node_id);
    }
  }

 private:
  LayoutSelectionStatus selection_status_;
  TextPaintStyle selection_style_;
  base::Optional<PhysicalRect> selection_rect_;
  const NGInlineCursor& containing_block_;
  bool paint_selected_text_only_;
  bool paint_selected_text_separately_;
};

}  // namespace

StringView NGTextPainterCursor::CurrentText() const {
  return CurrentItem()->Text();
}

const NGPaintFragment& NGTextPainterCursor::RootPaintFragment() const {
  if (!root_paint_fragment_)
    root_paint_fragment_ = paint_fragment_.Root();
  return *root_paint_fragment_;
}

template <typename Cursor>
void NGTextFragmentPainter<Cursor>::PaintSymbol(
    const LayoutObject* layout_object,
    const ComputedStyle& style,
    const PhysicalSize box_size,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  PhysicalRect marker_rect(
      LayoutListMarker::RelativeSymbolMarkerRect(style, box_size.width));
  marker_rect.Move(paint_offset);
  IntRect rect = PixelSnappedIntRect(marker_rect);

  ListMarkerPainter::PaintSymbol(paint_info, layout_object, style, rect);
}

// This is copied from InlineTextBoxPainter::PaintSelection() but lacks of
// ltr, expanding new line wrap or so which uses InlineTextBox functions.
template <typename Cursor>
void NGTextFragmentPainter<Cursor>::Paint(const PaintInfo& paint_info,
                                          const PhysicalOffset& paint_offset) {
  const auto& text_item = *cursor_.CurrentItem();
  // We can skip painting if the fragment (including selection) is invisible.
  if (!text_item.TextLength())
    return;
  const IntRect visual_rect = AsDisplayItemClient(cursor_).VisualRect();
  if (visual_rect.IsEmpty())
    return;

  if (!text_item.TextShapeResult() &&
      // A line break's selection tint is still visible.
      !text_item.IsLineBreak())
    return;

  const NGTextFragmentPaintInfo& fragment_paint_info =
      GetTextFragmentPaintInfo(cursor_);
  const LayoutObject* layout_object = text_item.GetLayoutObject();
  const ComputedStyle& style = text_item.Style();
  const Document& document = layout_object->GetDocument();
  const bool is_printing = paint_info.IsPrinting();

  // Determine whether or not we're selected.
  base::Optional<SelectionPaintState> selection;
  if (UNLIKELY(!is_printing && paint_info.phase != PaintPhase::kTextClip &&
               layout_object->IsSelected())) {
    const NGInlineCursor& root_inline_cursor =
        InlineCursorForBlockFlow(cursor_, &inline_cursor_for_block_flow_);
    selection.emplace(root_inline_cursor);
    if (!selection->Status().HasValidRange())
      selection.reset();
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

  // The text clip phase already has a DrawingRecorder. Text clips are initiated
  // only in BoxPainterBase::PaintFillLayer, which is already within a
  // DrawingRecorder.
  base::Optional<DrawingRecorder> recorder;
  if (paint_info.phase != PaintPhase::kTextClip) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, AsDisplayItemClient(cursor_), paint_info.phase))
      return;
    recorder.emplace(paint_info.context, AsDisplayItemClient(cursor_),
                     paint_info.phase);
  }

  PhysicalRect box_rect = ComputeBoxRect(cursor_, paint_offset, parent_offset_);

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
    PaintSymbol(layout_object, style, box_rect.size, paint_info,
                box_rect.offset);
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
  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);

  base::Optional<GraphicsContextStateSaver> state_saver;

  // 1. Paint backgrounds behind text if needed. Examples of such backgrounds
  // include selection and composition highlights.
  // Since NGPaintFragment::ComputeLocalSelectionRectForText() returns
  // PhysicalRect rather than LogicalRect, we should paint selection
  // before GraphicsContext flip.
  // TODO(yoichio): Make NGPhysicalTextFragment::LocalRect and
  // NGPaintFragment::ComputeLocalSelectionRectForText logical so that we can
  // paint selection in same flipped dimension as NGTextPainter.
  const DocumentMarkerVector& markers_to_paint =
      ComputeMarkersToPaint(node, text_item.IsEllipsis());
  if (paint_info.phase != PaintPhase::kSelectionDragImage &&
      paint_info.phase != PaintPhase::kTextClip && !is_printing) {
    PaintDocumentMarkers(context, text_item, cursor_.CurrentText(),
                         markers_to_paint, box_rect.offset, style,
                         DocumentMarkerPaintPhase::kBackground, nullptr);
    if (UNLIKELY(selection)) {
      selection->ComputeSelectionRect(box_rect.offset);
      selection->PaintSelectionBackground(context, node, document, style);
    }
  }

  const WritingMode writing_mode = style.GetWritingMode();
  const bool is_horizontal = IsHorizontalWritingMode(writing_mode);
  if (!is_horizontal) {
    state_saver.emplace(context);
    // Because we rotate the GraphicsContext to match the logical direction,
    // transpose the |box_rect| to match to it.
    box_rect.size = PhysicalSize(box_rect.Height(), box_rect.Width());
    context.ConcatCTM(TextPainterBase::Rotation(
        box_rect, writing_mode != WritingMode::kSidewaysLr
                      ? TextPainterBase::kClockwise
                      : TextPainterBase::kCounterclockwise));
  }

  // 2. Now paint the foreground, including text and decorations.
  int ascent = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  PhysicalOffset text_origin(box_rect.offset.left,
                             box_rect.offset.top + ascent);
  NGTextPainter text_painter(context, font, fragment_paint_info, visual_rect,
                             text_origin, box_rect, is_horizontal);

  if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone) {
    text_painter.SetEmphasisMark(style.TextEmphasisMarkString(),
                                 style.GetTextEmphasisPosition());
  }

  DOMNodeId node_id = kInvalidDOMNodeId;
  if (node) {
    if (auto* layout_text = ToLayoutTextOrNull(node->GetLayoutObject()))
      node_id = layout_text->EnsureNodeId();
  }

  const unsigned length = fragment_paint_info.to - fragment_paint_info.from;
  if (!selection || !selection->ShouldPaintSelectedTextOnly()) {
    // Paint text decorations except line-through.
    DecorationInfo decoration_info;
    bool has_line_through_decoration = false;
    if (style.TextDecorationsInEffect() != TextDecoration::kNone &&
        // Ellipsis should not have text decorations. This is not defined, but 4
        // impls do this.
        !text_item.IsEllipsis()) {
      PhysicalOffset local_origin = box_rect.offset;
      LayoutUnit width = box_rect.Width();
      const NGPhysicalBoxFragment* decorating_box = nullptr;
      const ComputedStyle* decorating_box_style =
          decorating_box ? &decorating_box->Style() : nullptr;

      text_painter.ComputeDecorationInfo(
          decoration_info, box_rect.offset, local_origin, width,
          style.GetFontBaseline(), style, decorating_box_style);

      NGTextDecorationOffset decoration_offset(
          *decoration_info.style, text_item.Style(), decorating_box);
      text_painter.PaintDecorationsExceptLineThrough(
          decoration_offset, decoration_info, paint_info,
          style.AppliedTextDecorations(), text_style,
          &has_line_through_decoration);
    }

    unsigned start_offset = fragment_paint_info.from;
    unsigned end_offset = fragment_paint_info.to;

    if (UNLIKELY(selection && selection->ShouldPaintSelectedTextSeparately())) {
      selection->PaintBeforeAndAfterSelectedText(
          text_painter, start_offset, end_offset, length, text_style, node_id);
    } else {
      text_painter.Paint(start_offset, end_offset, length, text_style, node_id);
    }

    // Paint line-through decoration if needed.
    if (has_line_through_decoration) {
      text_painter.PaintDecorationsOnlyLineThrough(
          decoration_info, paint_info, style.AppliedTextDecorations(),
          text_style);
    }
  }

  if (UNLIKELY(selection && (selection->ShouldPaintSelectedTextOnly() ||
                             selection->ShouldPaintSelectedTextSeparately()))) {
    // Paint only the text that is selected.
    if (!selection->IsSelectionRectComputed())
      selection->ComputeSelectionRect(box_rect.offset);
    selection->PaintSelectedText(text_painter, length, text_style, node_id);
  }

  if (paint_info.phase != PaintPhase::kForeground)
    return;
  PaintDocumentMarkers(context, text_item, cursor_.CurrentText(),
                       markers_to_paint, box_rect.offset, style,
                       DocumentMarkerPaintPhase::kForeground, &text_painter);
}

template class NGTextFragmentPainter<NGTextPainterCursor>;
template class NGTextFragmentPainter<NGInlineCursor>;

}  // namespace blink
