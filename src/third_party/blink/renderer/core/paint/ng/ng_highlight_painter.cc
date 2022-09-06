// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_highlight_painter.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/highlight_pseudo_marker.h"
#include "third_party/blink/renderer/core/editing/markers/styleable_marker.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/paint/document_marker_painter.h"
#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"
#include "third_party/blink/renderer/core/paint/inline_text_box_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_highlight_overlay.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

using HighlightLayerType = NGHighlightOverlay::HighlightLayerType;
using HighlightLayer = NGHighlightOverlay::HighlightLayer;
using HighlightEdge = NGHighlightOverlay::HighlightEdge;
using HighlightPart = NGHighlightOverlay::HighlightPart;

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

DocumentMarkerVector MarkersFor(Node* node,
                                bool is_ellipsis,
                                DocumentMarker::MarkerType type) {
  // TODO(yoichio): Handle first-letter
  const auto* text_node = DynamicTo<Text>(node);
  // We don't paint any marker on ellipsis.
  if (!text_node || is_ellipsis)
    return DocumentMarkerVector();

  DocumentMarkerController& controller = node->GetDocument().Markers();

  if (type == DocumentMarker::MarkerType::kCustomHighlight)
    return controller.CustomHighlightMarkersNotOverlapping(*text_node);
  return controller.MarkersFor(*text_node, DocumentMarker::MarkerTypes{type});
}

unsigned GetTextContentOffset(const Text& text, unsigned offset) {
  // TODO(yoichio): Sanitize DocumentMarker around text length.
  const Position position(text, std::min(offset, text.length()));
  const NGOffsetMapping* const offset_mapping =
      NGOffsetMapping::GetFor(position);
  DCHECK(offset_mapping);
  const absl::optional<unsigned>& ng_offset =
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
unsigned ClampOffset(unsigned offset, const NGFragmentItem& text_fragment) {
  return std::min(std::max(offset, text_fragment.StartOffset()),
                  text_fragment.EndOffset());
}

PhysicalRect MarkerRectForForeground(const NGFragmentItem& text_fragment,
                                     StringView text,
                                     unsigned start_offset,
                                     unsigned end_offset) {
  LayoutUnit start_position, end_position;
  std::tie(start_position, end_position) =
      text_fragment.LineLeftAndRightForOffsets(text, start_offset, end_offset);

  const LayoutUnit height = text_fragment.Size()
                                .ConvertToLogical(static_cast<WritingMode>(
                                    text_fragment.Style().GetWritingMode()))
                                .block_size;
  return {start_position, LayoutUnit(), end_position - start_position, height};
}

void PaintRect(GraphicsContext& context,
               const PhysicalRect& rect,
               const Color color,
               const AutoDarkMode& auto_dark_mode) {
  if (!color.Alpha())
    return;
  if (rect.size.IsEmpty())
    return;
  const gfx::Rect pixel_snapped_rect = ToPixelSnappedRect(rect);
  if (!pixel_snapped_rect.IsEmpty())
    context.FillRect(pixel_snapped_rect, color, auto_dark_mode);
}

void PaintRect(GraphicsContext& context,
               const PhysicalOffset& location,
               const PhysicalRect& rect,
               const Color color,
               const AutoDarkMode& auto_dark_mode) {
  PaintRect(context, PhysicalRect(rect.offset + location, rect.size), color,
            auto_dark_mode);
}

Color SelectionBackgroundColor(const Document& document,
                               const ComputedStyle& style,
                               Node* node,
                               Color text_color) {
  const Color color = HighlightPaintingUtils::HighlightBackgroundColor(
      document, style, node, absl::nullopt, kPseudoIdSelection);
  if (!color.Alpha())
    return Color();

  // If the text color ends up being the same as the selection background,
  // invert the selection background.
  if (text_color == color) {
    UseCounter::Count(node->GetDocument(),
                      WebFeature::kSelectionBackgroundColorInversion);
    return Color(0xff - color.Red(), 0xff - color.Green(), 0xff - color.Blue());
  }
  return color;
}

const HighlightRegistry* GetHighlightRegistry(const Node* node) {
  if (!node)
    return nullptr;
  return node->GetDocument()
      .domWindow()
      ->Supplementable<LocalDOMWindow>::RequireSupplement<HighlightRegistry>();
}

const LayoutSelectionStatus* GetSelectionStatus(
    const NGHighlightPainter::SelectionPaintState* selection) {
  if (!selection)
    return nullptr;
  return &selection->Status();
}

const DocumentMarkerVector* SelectMarkers(const HighlightLayer& layer,
                                          const DocumentMarkerVector& custom,
                                          const DocumentMarkerVector& grammar,
                                          const DocumentMarkerVector& spelling,
                                          const DocumentMarkerVector& target) {
  switch (layer.type) {
    case HighlightLayerType::kOriginating:
      NOTREACHED();
      break;
    case HighlightLayerType::kCustom:
      return &custom;
    case HighlightLayerType::kGrammar:
      return &grammar;
    case HighlightLayerType::kSpelling:
      return &spelling;
    case HighlightLayerType::kTargetText:
      return &target;
    case HighlightLayerType::kSelection:
      NOTREACHED();
      break;
    default:
      NOTREACHED();
  }

  return nullptr;
}

}  // namespace

NGHighlightPainter::SelectionPaintState::SelectionPaintState(
    const NGInlineCursor& containing_block,
    const PhysicalOffset& box_offset,
    const absl::optional<AffineTransform> writing_mode_rotation)
    : SelectionPaintState(containing_block,
                          box_offset,
                          writing_mode_rotation,
                          containing_block.Current()
                              .GetLayoutObject()
                              ->GetDocument()
                              .GetFrame()
                              ->Selection()) {}
NGHighlightPainter::SelectionPaintState::SelectionPaintState(
    const NGInlineCursor& containing_block,
    const PhysicalOffset& box_offset,
    const absl::optional<AffineTransform> writing_mode_rotation,
    const FrameSelection& frame_selection)
    : selection_status_(
          frame_selection.ComputeLayoutSelectionStatus(containing_block)),
      state_(frame_selection.ComputePaintingSelectionStateForCursor(
          containing_block.Current())),
      containing_block_(containing_block),
      box_offset_(box_offset),
      writing_mode_rotation_(writing_mode_rotation) {}

void NGHighlightPainter::SelectionPaintState::ComputeSelectionStyle(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    const PaintInfo& paint_info,
    const TextPaintStyle& text_style) {
  selection_style_ = TextPainterBase::SelectionPaintingStyle(
      document, style, node, paint_info, text_style);
  paint_selected_text_only_ =
      (paint_info.phase == PaintPhase::kSelectionDragImage);
}

void NGHighlightPainter::SelectionPaintState::ComputeSelectionRectIfNeeded() {
  if (!selection_rect_) {
    PhysicalRect physical =
        containing_block_.CurrentLocalSelectionRectForText(selection_status_);
    physical.offset += box_offset_;
    PhysicalRect rotated = writing_mode_rotation_
                               ? PhysicalRect::EnclosingRect(
                                     writing_mode_rotation_->Inverse().MapRect(
                                         gfx::RectF(physical)))
                               : physical;
    selection_rect_.emplace(SelectionRect{physical, rotated});
  }
}

const PhysicalRect&
NGHighlightPainter::SelectionPaintState::RectInPhysicalSpace() {
  ComputeSelectionRectIfNeeded();
  return selection_rect_->physical;
}

const PhysicalRect&
NGHighlightPainter::SelectionPaintState::RectInWritingModeSpace() {
  ComputeSelectionRectIfNeeded();
  return selection_rect_->rotated;
}

// Logic is copied from InlineTextBoxPainter::PaintSelection.
// |selection_start| and |selection_end| should be between
// [text_fragment.StartOffset(), text_fragment.EndOffset()].
void NGHighlightPainter::SelectionPaintState::PaintSelectionBackground(
    GraphicsContext& context,
    Node* node,
    const Document& document,
    const ComputedStyle& style,
    const absl::optional<AffineTransform>& rotation) {
  const Color color = SelectionBackgroundColor(document, style, node,
                                               selection_style_.fill_color);

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground));

  if (!rotation) {
    PaintRect(context, RectInPhysicalSpace(), color, auto_dark_mode);
    return;
  }

  // PaintRect tries to pixel-snap the given rect, but if we’re painting in a
  // non-horizontal writing mode, our context has been transformed, regressing
  // tests like <paint/invalidation/repaint-across-writing-mode-boundary>. To
  // fix this, we undo the transformation temporarily, then use the original
  // physical coordinates (before MapSelectionRectIntoRotatedSpace).
  context.ConcatCTM(rotation->Inverse());
  PaintRect(context, RectInPhysicalSpace(), color, auto_dark_mode);
  context.ConcatCTM(*rotation);
}

// Paint the selected text only.
void NGHighlightPainter::SelectionPaintState::PaintSelectedText(
    NGTextPainter& text_painter,
    unsigned length,
    const TextPaintStyle& text_style,
    DOMNodeId node_id,
    const AutoDarkMode& auto_dark_mode) {
  text_painter.PaintSelectedText(
      selection_status_.start, selection_status_.end, length, text_style,
      selection_style_, RectInWritingModeSpace(), node_id, auto_dark_mode);
}

// Paint the given text range in the given style, suppressing the text proper
// (painting shadows only) where selected.
void NGHighlightPainter::SelectionPaintState::
    PaintSuppressingTextProperWhereSelected(
        NGTextPainter& text_painter,
        unsigned start_offset,
        unsigned end_offset,
        unsigned length,
        const TextPaintStyle& text_style,
        DOMNodeId node_id,
        const AutoDarkMode& auto_dark_mode) {
  // First paint the shadows for the whole range.
  if (text_style.shadow) {
    text_painter.Paint(start_offset, end_offset, length, text_style, node_id,
                       auto_dark_mode, NGTextPainter::kShadowsOnly);
  }

  // Then paint the text proper for any unselected parts in storage order, so
  // that they’re always on top of the shadows.
  if (start_offset < selection_status_.start) {
    text_painter.Paint(start_offset, selection_status_.start, length,
                       text_style, node_id, auto_dark_mode,
                       NGTextPainter::kTextProperOnly);
  }
  if (selection_status_.end < end_offset) {
    text_painter.Paint(selection_status_.end, end_offset, length, text_style,
                       node_id, auto_dark_mode, NGTextPainter::kTextProperOnly);
  }
}

NGHighlightPainter::NGHighlightPainter(
    const NGTextFragmentPaintInfo& fragment_paint_info,
    NGTextPainter& text_painter,
    NGTextDecorationPainter& decoration_painter,
    const PaintInfo& paint_info,
    const NGInlineCursor& cursor,
    const NGFragmentItem& fragment_item,
    const absl::optional<AffineTransform> writing_mode_rotation,
    const PhysicalRect& decoration_rect,
    const PhysicalOffset& box_origin,
    const ComputedStyle& style,
    const TextPaintStyle& text_style,
    SelectionPaintState* selection,
    bool is_printing)
    : fragment_paint_info_(fragment_paint_info),
      text_painter_(text_painter),
      decoration_painter_(decoration_painter),
      paint_info_(paint_info),
      cursor_(cursor),
      fragment_item_(fragment_item),
      writing_mode_rotation_(writing_mode_rotation),
      decoration_rect_(decoration_rect),
      box_origin_(box_origin),
      style_(style),
      originating_text_style_(text_style),
      selection_(selection),
      layout_object_(fragment_item_.GetLayoutObject()),
      node_(layout_object_->GetNode()),
      foreground_auto_dark_mode_(
          PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kForeground)),
      background_auto_dark_mode_(
          PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kBackground)),
      markers_(ComputeMarkersToPaint(node_, fragment_item_.IsEllipsis())),
      skip_backgrounds_(is_printing ||
                        paint_info.phase == PaintPhase::kTextClip ||
                        paint_info.phase == PaintPhase::kSelectionDragImage) {
  if (RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled()) {
    bool is_ellipsis = fragment_item_.IsEllipsis();
    target_ = MarkersFor(node_, is_ellipsis, DocumentMarker::kTextFragment);
    spelling_ = MarkersFor(node_, is_ellipsis, DocumentMarker::kSpelling);
    grammar_ = MarkersFor(node_, is_ellipsis, DocumentMarker::kGrammar);
    custom_ = MarkersFor(node_, is_ellipsis, DocumentMarker::kCustomHighlight);
    Vector<HighlightLayer> layers = NGHighlightOverlay::ComputeLayers(
        GetHighlightRegistry(node_), fragment_paint_info_,
        GetSelectionStatus(selection_), custom_, grammar_, spelling_, target_);
    Vector<HighlightEdge> edges = NGHighlightOverlay::ComputeEdges(
        node_, GetHighlightRegistry(node_), fragment_paint_info_,
        GetSelectionStatus(selection_), custom_, grammar_, spelling_, target_);
    parts_ = NGHighlightOverlay::ComputeParts(layers, edges);

    const Document& document = layout_object_->GetDocument();
    for (wtf_size_t i = 0; i < layers.size(); i++) {
      if (layers[i].type == HighlightLayerType::kOriginating) {
        layers_.push_back(LayerPaintState{
            layers[i],
            WrapRefCounted(&style_),
            originating_text_style_,
        });
      } else {
        layers_.push_back(LayerPaintState{
            layers[i],
            HighlightPaintingUtils::HighlightPseudoStyle(
                node_, style_, layers[i].PseudoId(),
                layers[i].PseudoArgument()),
            HighlightPaintingUtils::HighlightPaintingStyle(
                document, style_, node_, layers[i].PseudoId(),
                layers_[i - 1].text_style, paint_info_,
                layers[i].PseudoArgument()),
        });
      }
      if (layers_[i].style) {
        decoration_painter_.UpdateDecorationInfo(layers_[i].decoration_info,
                                                 *layers_[i].style,
                                                 layers_[i].text_style);
      }
    }
  }
}

void NGHighlightPainter::Paint(Phase phase) {
  if (markers_.IsEmpty())
    return;

  if (skip_backgrounds_ && phase == kBackground)
    return;

  DCHECK(fragment_item_.GetNode());
  const auto& text_node = To<Text>(*fragment_item_.GetNode());
  const StringView text = cursor_.CurrentText();

  for (const DocumentMarker* marker : markers_) {
    const unsigned marker_start_offset =
        GetTextContentOffset(text_node, marker->StartOffset());
    const unsigned marker_end_offset =
        GetTextContentOffset(text_node, marker->EndOffset());
    const unsigned paint_start_offset =
        ClampOffset(marker_start_offset, fragment_item_);
    const unsigned paint_end_offset =
        ClampOffset(marker_end_offset, fragment_item_);
    if (paint_start_offset == paint_end_offset)
      continue;

    if (RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled()) {
      DCHECK(!DocumentMarker::MarkerTypes::HighlightPseudos().Contains(
          marker->GetType()));
    }

    switch (marker->GetType()) {
      case DocumentMarker::kSpelling:
      case DocumentMarker::kGrammar: {
        if (fragment_item_.GetNode()->GetDocument().Printing())
          break;
        if (phase == kBackground)
          continue;

        DocumentMarkerPainter::PaintDocumentMarker(
            paint_info_, box_origin_, style_, marker->GetType(),
            MarkerRectForForeground(fragment_item_, text, paint_start_offset,
                                    paint_end_offset),
            HighlightPaintingUtils::HighlightTextDecorationColor(
                style_, node_,
                marker->GetType() == DocumentMarker::kSpelling
                    ? kPseudoIdSpellingError
                    : kPseudoIdGrammarError));
      } break;

      case DocumentMarker::kTextMatch: {
        const Document& document = node_->GetDocument();
        if (!document.GetFrame()->GetEditor().MarkedTextMatchesAreHighlighted())
          break;
        const auto& text_match_marker = To<TextMatchMarker>(*marker);
        if (phase == kBackground) {
          Color color =
              LayoutTheme::GetTheme().PlatformTextSearchHighlightColor(
                  text_match_marker.IsActiveMatch(), style_.UsedColorScheme());
          PaintRect(paint_info_.context, PhysicalOffset(box_origin_),
                    fragment_item_.LocalRect(text, paint_start_offset,
                                             paint_end_offset),
                    color, background_auto_dark_mode_);
          break;
        }

        TextPaintStyle text_style;
        if (fragment_item_->Type() != NGFragmentItem::kSvgText) {
          text_style = DocumentMarkerPainter::ComputeTextPaintStyleFrom(
              document, node_, style_, text_match_marker, paint_info_);
        } else {
          // DocumentMarkerPainter::ComputeTextPaintStyleFrom() doesn't work
          // well with SVG <text>, which doesn't apply 'color' CSS property.
          const Color platform_matched_color =
              LayoutTheme::GetTheme().PlatformTextSearchColor(
                  text_match_marker.IsActiveMatch(), style_.UsedColorScheme());
          text_painter_.SetSvgState(
              *To<LayoutSVGInlineText>(fragment_item_->GetLayoutObject()),
              style_, platform_matched_color);
          text_style.current_color = platform_matched_color;
          text_style.stroke_width = style_.TextStrokeWidth();
          text_style.color_scheme = style_.UsedColorScheme();
        }
        text_painter_.Paint(paint_start_offset, paint_end_offset,
                            paint_end_offset - paint_start_offset, text_style,
                            kInvalidDOMNodeId, foreground_auto_dark_mode_);
      } break;

      case DocumentMarker::kComposition:
      case DocumentMarker::kActiveSuggestion:
      case DocumentMarker::kSuggestion: {
        const auto& styleable_marker = To<StyleableMarker>(*marker);
        if (phase == kBackground) {
          PaintRect(paint_info_.context, PhysicalOffset(box_origin_),
                    fragment_item_.LocalRect(text, paint_start_offset,
                                             paint_end_offset),
                    styleable_marker.BackgroundColor(),
                    background_auto_dark_mode_);
          break;
        }
        if (DocumentMarkerPainter::ShouldPaintMarkerUnderline(
                styleable_marker)) {
          const SimpleFontData* font_data = style_.GetFont().PrimaryFont();
          DocumentMarkerPainter::PaintStyleableMarkerUnderline(
              paint_info_.context, box_origin_, styleable_marker, style_,
              node_->GetDocument(),
              gfx::RectF(MarkerRectForForeground(
                  fragment_item_, text, paint_start_offset, paint_end_offset)),
              LayoutUnit(font_data->GetFontMetrics().Height()),
              fragment_item_.GetNode()->GetDocument().InDarkMode());
        }
      } break;

      case DocumentMarker::kTextFragment:
      case DocumentMarker::kCustomHighlight: {
        const auto& highlight_pseudo_marker =
            To<HighlightPseudoMarker>(*marker);
        const Document& document = node_->GetDocument();

        // Paint background
        if (phase == kBackground) {
          Color background_color =
              HighlightPaintingUtils::HighlightBackgroundColor(
                  document, style_, node_, absl::nullopt,
                  highlight_pseudo_marker.GetPseudoId(),
                  highlight_pseudo_marker.GetPseudoArgument());

          PaintRect(paint_info_.context, PhysicalOffset(box_origin_),
                    fragment_item_.LocalRect(text, paint_start_offset,
                                             paint_end_offset),
                    background_color, background_auto_dark_mode_);
          break;
        }

        DCHECK_EQ(phase, kForeground);
        Color text_color = style_.VisitedDependentColor(GetCSSPropertyColor());

        TextPaintStyle text_style;
        text_style.current_color = text_style.fill_color =
            text_style.stroke_color = text_style.emphasis_mark_color =
                text_color;
        text_style.stroke_width = style_.TextStrokeWidth();
        text_style.color_scheme = style_.UsedColorScheme();
        text_style.shadow = nullptr;

        const TextPaintStyle final_text_style =
            HighlightPaintingUtils::HighlightPaintingStyle(
                document, style_, node_, highlight_pseudo_marker.GetPseudoId(),
                text_style, paint_info_,
                highlight_pseudo_marker.GetPseudoArgument());

        scoped_refptr<const ComputedStyle> pseudo_style =
            HighlightPaintingUtils::HighlightPseudoStyle(
                node_, style_, highlight_pseudo_marker.GetPseudoId(),
                highlight_pseudo_marker.GetPseudoArgument());
        PhysicalRect decoration_rect = fragment_item_.LocalRect(
            text, paint_start_offset, paint_end_offset);
        decoration_rect.Move(PhysicalOffset(box_origin_));
        NGTextDecorationPainter decoration_painter(
            text_painter_, fragment_item_, paint_info_,
            pseudo_style ? *pseudo_style : style_, final_text_style,
            decoration_rect, selection_);

        decoration_painter.Begin(NGTextDecorationPainter::kOriginating);
        decoration_painter.PaintExceptLineThrough();

        text_painter_.Paint(paint_start_offset, paint_end_offset,
                            paint_end_offset - paint_start_offset,
                            final_text_style, kInvalidDOMNodeId,
                            foreground_auto_dark_mode_);

        decoration_painter.PaintOnlyLineThrough();
      } break;

      default:
        NOTREACHED();
        break;
    }
  }
}

void NGHighlightPainter::PaintOriginatingText(const TextPaintStyle& text_style,
                                              DOMNodeId node_id) {
  DCHECK(RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled());

  // First paint the shadows for the whole range.
  if (text_style.shadow) {
    text_painter_.Paint(fragment_paint_info_.from, fragment_paint_info_.to,
                        fragment_paint_info_.to - fragment_paint_info_.from,
                        text_style, node_id, foreground_auto_dark_mode_,
                        NGTextPainter::kShadowsOnly);
  }

  // Then paint the text proper for any unhighlighted parts in storage order,
  // so that they’re always on top of the shadows.
  for (const HighlightPart& part : parts_) {
    if (part.layer.type != HighlightLayerType::kOriginating)
      continue;

    PaintDecorationsExceptLineThrough(part);
    text_painter_.Paint(part.from, part.to, part.to - part.from, text_style,
                        node_id, foreground_auto_dark_mode_,
                        NGTextPainter::kTextProperOnly);
    PaintDecorationsOnlyLineThrough(part);
    PaintSpellingGrammarDecorations(part);
  }
}

void NGHighlightPainter::PaintHighlightOverlays(
    const TextPaintStyle& originating_text_style,
    DOMNodeId node_id,
    bool paint_marker_backgrounds,
    absl::optional<AffineTransform> rotation) {
  DCHECK(RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled());

  // |node| might not be a Text node (e.g. <br>), or it might be nullptr (e.g.
  // ::first-letter). In both cases, we should still try to paint kOriginating
  // and kSelection if necessary, but we can’t paint marker-based highlights,
  // because GetTextContentOffset requires a Text node. Markers are defined and
  // stored in terms of Text nodes anyway, so this should never be a problem.
  const auto* text_node = DynamicTo<Text>(node_);
  const Document& document = layout_object_->GetDocument();

  // For each overlay, paint its backgrounds and shadows over every highlighted
  // range in full.
  for (const LayerPaintState& layer : layers_) {
    if (layer.id.type == HighlightLayerType::kOriginating ||
        layer.id.type == HighlightLayerType::kSelection)
      continue;

    const DocumentMarkerVector* markers =
        SelectMarkers(layer.id, custom_, grammar_, spelling_, target_);

    for (const auto& marker : *markers) {
      if (layer.id.type == HighlightLayerType::kCustom) {
        // Filter custom highlight markers to one highlight at a time.
        auto* custom = To<CustomHighlightMarker>(marker.Get());
        if (custom->GetHighlightName() != layer.id.PseudoArgument())
          continue;
      }

      const unsigned content_start =
          GetTextContentOffset(*text_node, marker->StartOffset());
      const unsigned content_end =
          GetTextContentOffset(*text_node, marker->EndOffset());
      const unsigned clamped_start = ClampOffset(content_start, fragment_item_);
      const unsigned clamped_end = ClampOffset(content_end, fragment_item_);
      const unsigned length = clamped_end - clamped_start;
      if (length == 0)
        continue;

      const StringView text = cursor_.CurrentText();
      Color background_color = HighlightPaintingUtils::HighlightBackgroundColor(
          document, style_, node_, layer.text_style.fill_color,
          layer.id.PseudoId(), layer.id.PseudoArgument());

      // TODO(dazabani@igalia.com) paint rects pixel-snapped in physical space,
      // not writing-mode space (SelectionPaintState::PaintSelectionBackground)
      PaintRect(paint_info_.context, PhysicalOffset(box_origin_),
                fragment_item_.LocalRect(text, clamped_start, clamped_end),
                background_color, background_auto_dark_mode_);

      text_painter_.Paint(clamped_start, clamped_end, length, layer.text_style,
                          node_id, foreground_auto_dark_mode_,
                          TextPainterBase::kShadowsOnly);
    }
  }

  // Paint ::selection background.
  // TODO(dazabani@igalia.com) generalise ::selection painting logic to support
  // all highlights, then merge this branch into the loop above
  if (UNLIKELY(selection_)) {
    if (paint_marker_backgrounds) {
      selection_->PaintSelectionBackground(paint_info_.context, node_, document,
                                           style_, rotation);
    }
  }

  // For each overlay, paint the text proper over every highlighted range,
  // except any parts for which we’re not the topmost active highlight.
  for (const LayerPaintState& layer : layers_) {
    if (layer.id.type == HighlightLayerType::kOriginating ||
        layer.id.type == HighlightLayerType::kSelection)
      continue;

    for (const HighlightPart& part : parts_) {
      if (part.layer != layer.id)
        continue;

      const unsigned clamped_start = ClampOffset(part.from, fragment_item_);
      const unsigned clamped_end = ClampOffset(part.to, fragment_item_);

      // TODO(dazabani@igalia.com) expand range to include partial glyphs, then
      // paint with clipping (NGTextPainter::PaintSelectedText)

      PaintDecorationsExceptLineThrough(part);
      text_painter_.Paint(clamped_start, clamped_end,
                          clamped_end - clamped_start, layer.text_style,
                          node_id, foreground_auto_dark_mode_,
                          TextPainterBase::kTextProperOnly);
      PaintDecorationsOnlyLineThrough(part);
      PaintSpellingGrammarDecorations(part);
    }
  }

  // Paint ::selection foreground, including its shadows.
  // TODO(dazabani@igalia.com) generalise ::selection painting logic to support
  // all highlights, then merge this branch into the loop above
  if (UNLIKELY(selection_)) {
    for (const HighlightPart& part : parts_) {
      if (part.layer.type == HighlightLayerType::kSelection)
        PaintDecorationsExceptLineThrough(part);
    }

    selection_->PaintSelectedText(
        text_painter_, fragment_paint_info_.to - fragment_paint_info_.from,
        originating_text_style, node_id, foreground_auto_dark_mode_);

    for (const HighlightPart& part : parts_) {
      if (part.layer.type == HighlightLayerType::kSelection) {
        PaintDecorationsOnlyLineThrough(part);
        PaintSpellingGrammarDecorations(part);
      }
    }
  }
}

void NGHighlightPainter::ClipToPartDecorations(const HighlightPart& part) {
  const StringView text = cursor_.CurrentText();
  const unsigned clamped_start = ClampOffset(part.from, fragment_item_);
  const unsigned clamped_end = ClampOffset(part.to, fragment_item_);
  PhysicalRect local_rect =
      fragment_item_.LocalRect(text, clamped_start, clamped_end);
  PhysicalRect part_rect{box_origin_ + local_rect.offset, local_rect.size};
  gfx::RectF clip_rect{part_rect};

  if (writing_mode_rotation_)
    clip_rect = writing_mode_rotation_->Inverse().MapRect(clip_rect);

  // Whether it’s best to clip to selection rect on both axes or only inline
  // depends on the situation, but the latter can improve the appearance of
  // decorations. For example, we often paint overlines entirely past the
  // top edge of selection rect, and wavy underlines have similar problems.
  //
  // Sadly there’s no way to clip to a rect of infinite height, so for now,
  // let’s clip to selection rect plus its height both above and below. This
  // should be enough to avoid clipping most decorations in the wild.
  //
  // TODO(dazabani@igalia.com): take text-underline-offset and other
  // text-decoration properties into account?
  clip_rect.set_y(clip_rect.y() - clip_rect.height());
  clip_rect.set_height(3.0 * clip_rect.height());
  paint_info_.context.Clip(clip_rect);
}

void NGHighlightPainter::PaintDecorationsExceptLineThrough(
    const HighlightPart& part) {
  GraphicsContextStateSaver state_saver(paint_info_.context, false);

  for (const HighlightLayer& decoration_layer_id : part.decorations) {
    wtf_size_t decoration_layer_index = layers_.Find(decoration_layer_id);
    DCHECK_NE(decoration_layer_index, kNotFound);

    LayerPaintState& decoration_layer = layers_[decoration_layer_index];
    if (!decoration_layer.decoration_info)
      continue;

    // SVG painting currently ignores ::selection styles, and will malfunction
    // or crash if asked to paint decorations introduced by highlight pseudos.
    // TODO(crbug.com/1147859) is SVG spec ready for highlight decorations?
    if (text_painter_.GetSvgState() &&
        decoration_layer_id.type != HighlightLayerType::kOriginating) {
      continue;
    }

    if (!state_saver.Saved()) {
      state_saver.Save();
      ClipToPartDecorations(part);
    }

    if (decoration_layer_id.type == HighlightLayerType::kOriginating &&
        part.layer.type != HighlightLayerType::kOriginating) {
      wtf_size_t part_layer_index = layers_.Find(part.layer);
      decoration_layer.decoration_info->SetHighlightOverrideColor(
          layers_[part_layer_index].text_style.fill_color);
    }

    // TODO(crbug.com/1147859) order by underline-then-overline first, then by
    // highlight layer, not grouping underline and overline within each layer
    // https://github.com/w3c/csswg-drafts/issues/6022
    text_painter_.PaintDecorationsExceptLineThrough(
        fragment_item_, paint_info_, *decoration_layer.style,
        decoration_layer.text_style, *decoration_layer.decoration_info,
        decoration_rect_, &decoration_layer.has_line_through_decorations);
  }
}

void NGHighlightPainter::PaintDecorationsOnlyLineThrough(
    const HighlightPart& part) {
  GraphicsContextStateSaver state_saver(paint_info_.context, false);

  for (const HighlightLayer& decoration_layer_id : part.decorations) {
    wtf_size_t decoration_layer_index = layers_.Find(decoration_layer_id);
    DCHECK_NE(decoration_layer_index, kNotFound);

    LayerPaintState& decoration_layer = layers_[decoration_layer_index];
    if (!decoration_layer.decoration_info ||
        !decoration_layer.has_line_through_decorations)
      continue;

    // SVG painting currently ignores ::selection styles, and will malfunction
    // or crash if asked to paint decorations introduced by highlight pseudos.
    // TODO(crbug.com/1147859) is SVG spec ready for highlight decorations?
    if (text_painter_.GetSvgState() &&
        decoration_layer_id.type != HighlightLayerType::kOriginating) {
      continue;
    }

    if (!state_saver.Saved()) {
      state_saver.Save();
      ClipToPartDecorations(part);
    }

    if (decoration_layer_id.type == HighlightLayerType::kOriginating &&
        part.layer.type != HighlightLayerType::kOriginating) {
      wtf_size_t part_layer_index = layers_.Find(part.layer);
      decoration_layer.decoration_info->SetHighlightOverrideColor(
          layers_[part_layer_index].text_style.fill_color);
    }

    text_painter_.PaintDecorationsOnlyLineThrough(
        fragment_item_, paint_info_, *decoration_layer.style,
        decoration_layer.text_style, *decoration_layer.decoration_info,
        decoration_rect_);
  }
}

void NGHighlightPainter::PaintSpellingGrammarDecorations(
    const HighlightPart& part) {
  const StringView text = cursor_.CurrentText();
  const unsigned clamped_start = ClampOffset(part.from, fragment_item_);
  const unsigned clamped_end = ClampOffset(part.to, fragment_item_);
  absl::optional<PhysicalRect> marker_rect{};

  for (const HighlightLayer& decoration_layer_id : part.decorations) {
    switch (decoration_layer_id.type) {
      case HighlightLayerType::kSpelling:
      case HighlightLayerType::kGrammar: {
        wtf_size_t i = layers_.Find(decoration_layer_id);
        DCHECK_NE(i, kNotFound);
        const LayerPaintState& decoration_layer = layers_[i];

        // TODO(crbug.com/1163436): remove once UA stylesheet sets ::spelling
        // and ::grammar to text-decoration-line:{spelling,grammar}-error
        if (decoration_layer.style &&
            decoration_layer.style->TextDecorationsInEffect() !=
                TextDecorationLine::kNone)
          break;

        if (!marker_rect) {
          marker_rect = MarkerRectForForeground(fragment_item_, text,
                                                clamped_start, clamped_end);
        }

        DocumentMarkerPainter::PaintDocumentMarker(
            paint_info_, box_origin_, style_,
            decoration_layer_id.type == HighlightLayerType::kSpelling
                ? DocumentMarker::kSpelling
                : DocumentMarker::kGrammar,
            *marker_rect,
            HighlightPaintingUtils::HighlightTextDecorationColor(
                style_, node_,
                decoration_layer_id.type == HighlightLayerType::kSpelling
                    ? kPseudoIdSpellingError
                    : kPseudoIdGrammarError));
      } break;

      default:
        break;
    }
  }
}

bool NGHighlightPainter::LayerPaintState::operator==(
    const HighlightLayer& other) const {
  return id == other;
}

bool NGHighlightPainter::LayerPaintState::operator!=(
    const HighlightLayer& other) const {
  return !operator==(other);
}

}  // namespace blink
