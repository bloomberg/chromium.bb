// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_HIGHLIGHT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_HIGHLIGHT_PAINTER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/layout/api/selection_state.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/ng/ng_highlight_overlay.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
class FrameSelection;
class LayoutObject;
class NGFragmentItem;
class NGTextPainter;
class NGTextDecorationPainter;
class NGInlineCursor;
class Node;
struct LayoutSelectionStatus;
struct NGTextFragmentPaintInfo;
struct PaintInfo;
struct PhysicalOffset;

// Highlight overlay painter for LayoutNG. Operates on NGFragmentItem that
// IsText(). Delegates to NGTextPainter to paint the text itself.
class CORE_EXPORT NGHighlightPainter {
  STACK_ALLOCATED();

 public:
  class SelectionPaintState {
    STACK_ALLOCATED();

   public:
    explicit SelectionPaintState(
        const NGInlineCursor& containing_block,
        const PhysicalOffset& box_offset,
        const absl::optional<AffineTransform> writing_mode_rotation = {});
    explicit SelectionPaintState(
        const NGInlineCursor& containing_block,
        const PhysicalOffset& box_offset,
        const absl::optional<AffineTransform> writing_mode_rotation,
        const FrameSelection&);

    const LayoutSelectionStatus& Status() const { return selection_status_; }

    const TextPaintStyle& GetSelectionStyle() const { return selection_style_; }

    SelectionState State() const { return state_; }

    bool ShouldPaintSelectedTextOnly() const {
      return paint_selected_text_only_;
    }

    void ComputeSelectionStyle(const Document& document,
                               const ComputedStyle& style,
                               Node* node,
                               const PaintInfo& paint_info,
                               const TextPaintStyle& text_style);

    // When painting text fragments in a vertical writing-mode, we sometimes
    // need to rotate the canvas into a line-relative coordinate space. Paint
    // ops done while rotated need coordinates in this rotated space, but ops
    // done outside of these rotations need the original physical rect.
    const PhysicalRect& RectInPhysicalSpace();
    const PhysicalRect& RectInWritingModeSpace();

    void PaintSelectionBackground(
        GraphicsContext& context,
        Node* node,
        const Document& document,
        const ComputedStyle& style,
        const absl::optional<AffineTransform>& rotation);

    void PaintSelectedText(NGTextPainter& text_painter,
                           unsigned length,
                           const TextPaintStyle& text_style,
                           DOMNodeId node_id,
                           const AutoDarkMode& auto_dark_mode);

    void PaintSuppressingTextProperWhereSelected(
        NGTextPainter& text_painter,
        unsigned start_offset,
        unsigned end_offset,
        unsigned length,
        const TextPaintStyle& text_style,
        DOMNodeId node_id,
        const AutoDarkMode& auto_dark_mode);

   private:
    struct SelectionRect {
      PhysicalRect physical;
      PhysicalRect rotated;
    };

    // Lazy init |selection_rect_| only when needed, such as when we need to
    // record selection bounds or actually paint the selection. There are many
    // subtle conditions where we won’t ever need this field.
    void ComputeSelectionRectIfNeeded();

    const LayoutSelectionStatus selection_status_;
    const SelectionState state_;
    const NGInlineCursor& containing_block_;
    const PhysicalOffset& box_offset_;
    const absl::optional<AffineTransform> writing_mode_rotation_;
    absl::optional<SelectionRect> selection_rect_;
    TextPaintStyle selection_style_;
    bool paint_selected_text_only_;
  };

  NGHighlightPainter(
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
      SelectionPaintState*,
      bool is_printing);

  enum Phase { kBackground, kForeground };
  void Paint(Phase phase);

  // HighlightOverlayPainting feature only
  void PaintOriginatingText(const TextPaintStyle&, DOMNodeId);
  void PaintHighlightOverlays(const TextPaintStyle&,
                              DOMNodeId,
                              bool paint_marker_backgrounds,
                              absl::optional<AffineTransform> rotation);
  void ClipToPartDecorations(const NGHighlightOverlay::HighlightPart&);
  void PaintDecorationsExceptLineThrough(
      const NGHighlightOverlay::HighlightPart&);
  void PaintDecorationsOnlyLineThrough(
      const NGHighlightOverlay::HighlightPart&);
  void PaintSpellingGrammarDecorations(
      const NGHighlightOverlay::HighlightPart&);
  wtf_size_t LayerCount() { return layers_.size(); }

  SelectionPaintState* Selection() { return selection_; }

 private:
  struct LayerPaintState {
    LayerPaintState(NGHighlightOverlay::HighlightLayer id,
                    scoped_refptr<const ComputedStyle> style,
                    TextPaintStyle text_style)
        : id(id), style(std::move(style)), text_style(text_style) {}

    // Equality on HighlightLayer id only, for Vector::Find.
    bool operator==(const LayerPaintState&) const = delete;
    bool operator!=(const LayerPaintState&) const = delete;
    bool operator==(const NGHighlightOverlay::HighlightLayer&) const;
    bool operator!=(const NGHighlightOverlay::HighlightLayer&) const;

    const NGHighlightOverlay::HighlightLayer id;
    const scoped_refptr<const ComputedStyle> style;
    const TextPaintStyle text_style;
    absl::optional<TextDecorationInfo> decoration_info{};
    bool has_line_through_decorations{false};
  };

  const NGTextFragmentPaintInfo& fragment_paint_info_;
  NGTextPainter& text_painter_;
  NGTextDecorationPainter& decoration_painter_;
  const PaintInfo& paint_info_;
  const NGInlineCursor& cursor_;
  const NGFragmentItem& fragment_item_;
  const absl::optional<AffineTransform> writing_mode_rotation_;
  const PhysicalRect& decoration_rect_;
  const PhysicalOffset& box_origin_;
  const ComputedStyle& style_;
  const TextPaintStyle& originating_text_style_;
  SelectionPaintState* selection_;
  const LayoutObject* layout_object_;
  Node* node_;
  const AutoDarkMode foreground_auto_dark_mode_;
  const AutoDarkMode background_auto_dark_mode_;
  const DocumentMarkerVector markers_;
  DocumentMarkerVector target_;
  DocumentMarkerVector spelling_;
  DocumentMarkerVector grammar_;
  DocumentMarkerVector custom_;
  Vector<LayerPaintState> layers_;
  Vector<NGHighlightOverlay::HighlightPart> parts_;
  const bool skip_backgrounds_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_HIGHLIGHT_PAINTER_H_
