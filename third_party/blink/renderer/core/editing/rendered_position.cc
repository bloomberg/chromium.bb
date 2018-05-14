/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/rendered_position.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/editing/inline_box_traversal.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_selection.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

// static
RenderedPosition RenderedPosition::Create(
    const VisiblePositionInFlatTree& position) {
  if (position.IsNull())
    return RenderedPosition();
  InlineBoxPosition box_position =
      ComputeInlineBoxPosition(position.ToPositionWithAffinity());
  if (!box_position.inline_box)
    return RenderedPosition();

  const InlineBox* box = box_position.inline_box;
  const int offset = box_position.offset_in_box;

  // When at bidi boundary, ensure that |inline_box_| belongs to the higher-
  // level bidi run.

  // For example, abc FED |ghi should be changed into abc FED| ghi
  if (offset == box->CaretLeftmostOffset()) {
    const InlineBox* prev_box = box->PrevLeafChildIgnoringLineBreak();
    if (prev_box && prev_box->BidiLevel() > box->BidiLevel()) {
      return RenderedPosition(prev_box, prev_box->CaretRightmostOffset(),
                              BidiBoundaryType::kRightBoundary);
    }
    BidiBoundaryType type =
        prev_box && prev_box->BidiLevel() == box->BidiLevel()
            ? BidiBoundaryType::kNotBoundary
            : BidiBoundaryType::kLeftBoundary;
    return RenderedPosition(box, offset, type);
  }

  // For example, abc| FED ghi should be changed into abc |FED ghi
  if (offset == box->CaretRightmostOffset()) {
    const InlineBox* next_box = box->NextLeafChildIgnoringLineBreak();
    if (next_box && next_box->BidiLevel() > box->BidiLevel()) {
      return RenderedPosition(next_box, next_box->CaretLeftmostOffset(),
                              BidiBoundaryType::kLeftBoundary);
    }
    BidiBoundaryType type =
        next_box && next_box->BidiLevel() == box->BidiLevel()
            ? BidiBoundaryType::kNotBoundary
            : BidiBoundaryType::kRightBoundary;
    return RenderedPosition(box, offset, type);
  }

  return RenderedPosition(box, offset, BidiBoundaryType::kNotBoundary);
}

bool RenderedPosition::IsPossiblyOtherBoundaryOf(
    const RenderedPosition& other) const {
  DCHECK(other.AtBidiBoundary());
  if (!AtBidiBoundary())
    return false;
  if (bidi_boundary_type_ == other.bidi_boundary_type_)
    return false;
  return inline_box_->BidiLevel() >= other.inline_box_->BidiLevel();
}

bool RenderedPosition::BidiRunContains(const RenderedPosition& other) const {
  DCHECK(AtBidiBoundary());
  DCHECK(!other.IsNull());
  UBiDiLevel level = inline_box_->BidiLevel();
  if (level > other.inline_box_->BidiLevel())
    return false;
  const InlineBox& boundary_of_other =
      bidi_boundary_type_ == BidiBoundaryType::kLeftBoundary
          ? InlineBoxTraversal::
                FindLeftBoundaryOfEntireBidiRunIgnoringLineBreak(
                    *other.inline_box_, level)
          : InlineBoxTraversal::
                FindRightBoundaryOfEntireBidiRunIgnoringLineBreak(
                    *other.inline_box_, level);
  return inline_box_ == &boundary_of_other;
}

PositionInFlatTree RenderedPosition::GetPosition() const {
  DCHECK(AtBidiBoundary());
  return PositionInFlatTree::EditingPositionOf(
      inline_box_->GetLineLayoutItem().GetNode(), offset_);
}

// Note: If the layout object has a scrolling contents layer, the selection
// will be relative to that.
static GraphicsLayer* GetGraphicsLayerBacking(
    const LayoutObject& layout_object) {
  const LayoutBoxModelObject& paint_invalidation_container =
      layout_object.ContainerForPaintInvalidation();
  DCHECK(paint_invalidation_container.Layer());
  if (paint_invalidation_container.Layer()->GetCompositingState() ==
      kNotComposited)
    return nullptr;
  return paint_invalidation_container.Layer()->GraphicsLayerBacking(
      &layout_object);
}

// Convert a local point into the coordinate system of backing coordinates.
static FloatPoint LocalToInvalidationBackingPoint(
    const LayoutPoint& local_point,
    const LayoutObject& layout_object) {
  const LayoutBoxModelObject& paint_invalidation_container =
      layout_object.ContainerForPaintInvalidation();
  DCHECK(paint_invalidation_container.Layer());

  FloatPoint container_point = layout_object.LocalToAncestorPoint(
      FloatPoint(local_point), &paint_invalidation_container,
      kTraverseDocumentBoundaries);

  // A layoutObject can have no invalidation backing if it is from a detached
  // frame, or when forced compositing is disabled.
  if (paint_invalidation_container.Layer()->GetCompositingState() ==
      kNotComposited)
    return container_point;

  PaintLayer::MapPointInPaintInvalidationContainerToBacking(
      paint_invalidation_container, container_point);

  if (GraphicsLayer* graphics_layer = GetGraphicsLayerBacking(layout_object))
    container_point.Move(-graphics_layer->OffsetFromLayoutObject());

  // Ensure the coordinates are in the scrolling contents space, if the object
  // is a scroller.
  if (paint_invalidation_container.UsesCompositedScrolling()) {
    container_point.Move(paint_invalidation_container.Layer()
                             ->GetScrollableArea()
                             ->GetScrollOffset());
  }

  return container_point;
}

std::pair<LayoutPoint, LayoutPoint> static GetLocalSelectionStartpoints(
    const LocalCaretRect& local_caret_rect) {
  const LayoutRect rect = local_caret_rect.rect;
  if (local_caret_rect.layout_object->Style()->IsHorizontalWritingMode())
    return {rect.MinXMinYCorner(), rect.MinXMaxYCorner()};

  // When text is vertical, it looks better for the start handle baseline to
  // be at the starting edge, to enclose the selection fully between the
  // handles.
  return {rect.MaxXMinYCorner(), rect.MinXMinYCorner()};
}

std::pair<LayoutPoint, LayoutPoint> static GetLocalSelectionEndpoints(
    const LocalCaretRect& local_caret_rect) {
  const LayoutRect rect = local_caret_rect.rect;
  if (local_caret_rect.layout_object->Style()->IsHorizontalWritingMode())
    return {rect.MinXMinYCorner(), rect.MinXMaxYCorner()};

  return {rect.MinXMinYCorner(), rect.MaxXMinYCorner()};
}

static LayoutPoint GetSamplePointForVisibility(
    const LayoutPoint& edge_top_in_layer,
    const LayoutPoint& edge_bottom_in_layer) {
  FloatSize diff(edge_top_in_layer - edge_bottom_in_layer);
  // Adjust by ~1px to avoid integer snapping error. This logic is the same
  // as that in ComputeViewportSelectionBound in cc.
  diff.Scale(1 / diff.DiagonalLength());
  LayoutPoint sample_point = edge_bottom_in_layer;
  sample_point.Move(LayoutSize(diff));
  return sample_point;
}

// Returns whether this position is not visible on the screen (because
// clipped out).
static bool IsVisible(const LayoutObject& rect_layout_object,
                      const LayoutPoint& edge_top_in_layer,
                      const LayoutPoint& edge_bottom_in_layer) {
  Node* const node = rect_layout_object.GetNode();
  if (!node)
    return true;
  TextControlElement* text_control = EnclosingTextControl(node);
  if (!text_control)
    return true;
  if (!IsHTMLInputElement(text_control))
    return true;

  LayoutObject* layout_object = text_control->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return true;

  const LayoutPoint sample_point =
      GetSamplePointForVisibility(edge_top_in_layer, edge_bottom_in_layer);

  LayoutBox* const text_control_object = ToLayoutBox(layout_object);
  const LayoutPoint position_in_input(rect_layout_object.LocalToAncestorPoint(
      FloatPoint(sample_point), text_control_object,
      kTraverseDocumentBoundaries));
  return text_control_object->BorderBoxRect().Contains(position_in_input);
}

static CompositedSelectionBound ComputeSelectionBound(
    const PositionWithAffinity& position,
    const LayoutObject& layout_object,
    const LayoutPoint& edge_top_in_layer,
    const LayoutPoint& edge_bottom_in_layer) {
  CompositedSelectionBound bound;
  bound.is_text_direction_rtl =
      layout_object.HasFlippedBlocksWritingMode() ||
      PrimaryDirectionOf(*position.AnchorNode()) == TextDirection::kRtl;
  bound.edge_top_in_layer =
      LocalToInvalidationBackingPoint(edge_top_in_layer, layout_object);
  bound.edge_bottom_in_layer =
      LocalToInvalidationBackingPoint(edge_bottom_in_layer, layout_object);
  bound.layer = GetGraphicsLayerBacking(layout_object);
  bound.hidden =
      !IsVisible(layout_object, edge_top_in_layer, edge_bottom_in_layer);
  return bound;
}

static CompositedSelectionBound StartPositionInGraphicsLayerBacking(
    const PositionWithAffinity& position) {
  const LocalCaretRect& local_caret_rect = LocalCaretRectOfPosition(position);
  const LayoutObject* const layout_object = local_caret_rect.layout_object;
  if (!layout_object)
    return CompositedSelectionBound();

  LayoutPoint edge_top_in_layer, edge_bottom_in_layer;
  std::tie(edge_top_in_layer, edge_bottom_in_layer) =
      GetLocalSelectionStartpoints(local_caret_rect);
  return ComputeSelectionBound(position, *layout_object, edge_top_in_layer,
                               edge_bottom_in_layer);
}

static CompositedSelectionBound EndPositionInGraphicsLayerBacking(
    const PositionWithAffinity& position) {
  const LocalCaretRect& local_caret_rect = LocalCaretRectOfPosition(position);
  const LayoutObject* const layout_object = local_caret_rect.layout_object;
  if (!layout_object)
    return CompositedSelectionBound();

  LayoutPoint edge_top_in_layer, edge_bottom_in_layer;
  std::tie(edge_top_in_layer, edge_bottom_in_layer) =
      GetLocalSelectionEndpoints(local_caret_rect);
  return ComputeSelectionBound(position, *layout_object, edge_top_in_layer,
                               edge_bottom_in_layer);
}

CompositedSelection RenderedPosition::ComputeCompositedSelection(
    const FrameSelection& frame_selection) {
  if (!frame_selection.IsHandleVisible() || frame_selection.IsHidden())
    return {};

  // TODO(yoichio): Compute SelectionInDOMTree w/o VS canonicalization.
  // crbug.com/789870 for detail.
  const SelectionInDOMTree& visible_selection =
      frame_selection.ComputeVisibleSelectionInDOMTree().AsSelection();
  // Non-editable caret selections lack any kind of UI affordance, and
  // needn't be tracked by the client.
  if (visible_selection.IsCaret() &&
      !IsEditablePosition(visible_selection.ComputeStartPosition()))
    return {};

  CompositedSelection selection;
  selection.start = StartPositionInGraphicsLayerBacking(PositionWithAffinity(
      visible_selection.ComputeStartPosition(), visible_selection.Affinity()));
  if (!selection.start.layer)
    return {};

  selection.end = EndPositionInGraphicsLayerBacking(PositionWithAffinity(
      visible_selection.ComputeEndPosition(), visible_selection.Affinity()));
  if (!selection.end.layer)
    return {};

  DCHECK(!visible_selection.IsNone());
  selection.type =
      visible_selection.IsRange() ? kRangeSelection : kCaretSelection;
  return selection;
}

}  // namespace blink
