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

#include "core/editing/RenderedPosition.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InlineBoxPosition.h"
#include "core/editing/InlineBoxTraversal.h"
#include "core/editing/LocalCaretRect.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/forms/TextControlElement.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/compositing/CompositedSelection.h"

namespace blink {

template <typename Strategy>
static inline LayoutObject* LayoutObjectFromPosition(
    const PositionTemplate<Strategy>& position) {
  DCHECK(position.IsNotNull());
  Node* layout_object_node = nullptr;
  switch (position.AnchorType()) {
    case PositionAnchorType::kOffsetInAnchor:
      layout_object_node = position.ComputeNodeAfterPosition();
      if (!layout_object_node || !layout_object_node->GetLayoutObject())
        layout_object_node = position.AnchorNode()->lastChild();
      break;

    case PositionAnchorType::kBeforeAnchor:
    case PositionAnchorType::kAfterAnchor:
      break;

    case PositionAnchorType::kBeforeChildren:
      layout_object_node = Strategy::FirstChild(*position.AnchorNode());
      break;
    case PositionAnchorType::kAfterChildren:
      layout_object_node = Strategy::LastChild(*position.AnchorNode());
      break;
  }
  if (!layout_object_node || !layout_object_node->GetLayoutObject())
    layout_object_node = position.AnchorNode();
  return layout_object_node->GetLayoutObject();
}

RenderedPosition::RenderedPosition(const VisiblePosition& position)
    : RenderedPosition(position.DeepEquivalent(), position.Affinity()) {}

RenderedPosition::RenderedPosition(const VisiblePositionInFlatTree& position)
    : RenderedPosition(position.DeepEquivalent(), position.Affinity()) {}

// TODO(editing-dev): Stop duplicating code in the two constructors

RenderedPosition::RenderedPosition(const Position& position,
                                   TextAffinity affinity)
    : layout_object_(nullptr), inline_box_(nullptr), offset_(0) {
  if (position.IsNull())
    return;
  InlineBoxPosition box_position =
      ComputeInlineBoxPosition(PositionWithAffinity(position, affinity));
  inline_box_ = box_position.inline_box;
  offset_ = box_position.offset_in_box;
  if (inline_box_)
    layout_object_ =
        LineLayoutAPIShim::LayoutObjectFrom(inline_box_->GetLineLayoutItem());
  else
    layout_object_ = LayoutObjectFromPosition(position);
}

RenderedPosition::RenderedPosition(const PositionInFlatTree& position,
                                   TextAffinity affinity)
    : layout_object_(nullptr), inline_box_(nullptr), offset_(0) {
  if (position.IsNull())
    return;
  InlineBoxPosition box_position = ComputeInlineBoxPosition(
      PositionInFlatTreeWithAffinity(position, affinity));
  inline_box_ = box_position.inline_box;
  offset_ = box_position.offset_in_box;
  if (inline_box_)
    layout_object_ =
        LineLayoutAPIShim::LayoutObjectFrom(inline_box_->GetLineLayoutItem());
  else
    layout_object_ = LayoutObjectFromPosition(position);
}

const InlineBox* RenderedPosition::PrevLeafChild() const {
  if (!prev_leaf_child_.has_value())
    prev_leaf_child_ = inline_box_->PrevLeafChildIgnoringLineBreak();
  return prev_leaf_child_.value();
}

const InlineBox* RenderedPosition::NextLeafChild() const {
  if (!next_leaf_child_.has_value())
    next_leaf_child_ = inline_box_->NextLeafChildIgnoringLineBreak();
  return next_leaf_child_.value();
}

bool RenderedPosition::IsEquivalent(const RenderedPosition& other) const {
  return (layout_object_ == other.layout_object_ &&
          inline_box_ == other.inline_box_ && offset_ == other.offset_) ||
         (AtLeftmostOffsetInBox() && other.AtRightmostOffsetInBox() &&
          PrevLeafChild() == other.inline_box_) ||
         (AtRightmostOffsetInBox() && other.AtLeftmostOffsetInBox() &&
          NextLeafChild() == other.inline_box_);
}

unsigned char RenderedPosition::BidiLevelOnLeft() const {
  const InlineBox* box =
      AtLeftmostOffsetInBox() ? PrevLeafChild() : inline_box_;
  return box ? box->BidiLevel() : 0;
}

unsigned char RenderedPosition::BidiLevelOnRight() const {
  const InlineBox* box =
      AtRightmostOffsetInBox() ? NextLeafChild() : inline_box_;
  return box ? box->BidiLevel() : 0;
}

RenderedPosition RenderedPosition::LeftBoundaryOfBidiRun(
    unsigned char bidi_level_of_run) {
  if (!inline_box_ || bidi_level_of_run > inline_box_->BidiLevel())
    return RenderedPosition();

  const InlineBox* const box =
      InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRunIgnoringLineBreak(
          *inline_box_, bidi_level_of_run);
  return RenderedPosition(
      LineLayoutAPIShim::LayoutObjectFrom(box->GetLineLayoutItem()), box,
      box->CaretLeftmostOffset());
}

RenderedPosition RenderedPosition::RightBoundaryOfBidiRun(
    unsigned char bidi_level_of_run) {
  if (!inline_box_ || bidi_level_of_run > inline_box_->BidiLevel())
    return RenderedPosition();

  const InlineBox* const box =
      InlineBoxTraversal::FindRightBoundaryOfEntireBidiRunIgnoringLineBreak(
          *inline_box_, bidi_level_of_run);
  return RenderedPosition(
      LineLayoutAPIShim::LayoutObjectFrom(box->GetLineLayoutItem()), box,
      box->CaretRightmostOffset());
}

bool RenderedPosition::AtLeftBoundaryOfBidiRun(
    ShouldMatchBidiLevel should_match_bidi_level,
    unsigned char bidi_level_of_run) const {
  if (!inline_box_)
    return false;

  if (AtLeftmostOffsetInBox()) {
    if (should_match_bidi_level == kIgnoreBidiLevel)
      return !PrevLeafChild() ||
             PrevLeafChild()->BidiLevel() < inline_box_->BidiLevel();
    return inline_box_->BidiLevel() >= bidi_level_of_run &&
           (!PrevLeafChild() ||
            PrevLeafChild()->BidiLevel() < bidi_level_of_run);
  }

  if (AtRightmostOffsetInBox()) {
    if (should_match_bidi_level == kIgnoreBidiLevel)
      return NextLeafChild() &&
             inline_box_->BidiLevel() < NextLeafChild()->BidiLevel();
    return NextLeafChild() && inline_box_->BidiLevel() < bidi_level_of_run &&
           NextLeafChild()->BidiLevel() >= bidi_level_of_run;
  }

  return false;
}

bool RenderedPosition::AtRightBoundaryOfBidiRun(
    ShouldMatchBidiLevel should_match_bidi_level,
    unsigned char bidi_level_of_run) const {
  if (!inline_box_)
    return false;

  if (AtRightmostOffsetInBox()) {
    if (should_match_bidi_level == kIgnoreBidiLevel)
      return !NextLeafChild() ||
             NextLeafChild()->BidiLevel() < inline_box_->BidiLevel();
    return inline_box_->BidiLevel() >= bidi_level_of_run &&
           (!NextLeafChild() ||
            NextLeafChild()->BidiLevel() < bidi_level_of_run);
  }

  if (AtLeftmostOffsetInBox()) {
    if (should_match_bidi_level == kIgnoreBidiLevel)
      return PrevLeafChild() &&
             inline_box_->BidiLevel() < PrevLeafChild()->BidiLevel();
    return PrevLeafChild() && inline_box_->BidiLevel() < bidi_level_of_run &&
           PrevLeafChild()->BidiLevel() >= bidi_level_of_run;
  }

  return false;
}

Position RenderedPosition::PositionAtLeftBoundaryOfBiDiRun() const {
  DCHECK(AtLeftBoundaryOfBidiRun());

  if (AtLeftmostOffsetInBox())
    return Position::EditingPositionOf(layout_object_->GetNode(), offset_);

  return Position::EditingPositionOf(
      NextLeafChild()->GetLineLayoutItem().GetNode(),
      NextLeafChild()->CaretLeftmostOffset());
}

Position RenderedPosition::PositionAtRightBoundaryOfBiDiRun() const {
  DCHECK(AtRightBoundaryOfBidiRun());

  if (AtRightmostOffsetInBox())
    return Position::EditingPositionOf(layout_object_->GetNode(), offset_);

  return Position::EditingPositionOf(
      PrevLeafChild()->GetLineLayoutItem().GetNode(),
      PrevLeafChild()->CaretRightmostOffset());
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

bool LayoutObjectContainsPosition(LayoutObject* target,
                                  const Position& position) {
  for (LayoutObject* layout_object = LayoutObjectFromPosition(position);
       layout_object && layout_object->GetNode();
       layout_object = layout_object->Parent()) {
    if (layout_object == target)
      return true;
  }
  return false;
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
  selection.start = StartPositionInGraphicsLayerBacking(
      PositionWithAffinity(visible_selection.ComputeStartPosition()));
  if (!selection.start.layer)
    return {};

  selection.end = EndPositionInGraphicsLayerBacking(
      PositionWithAffinity(visible_selection.ComputeEndPosition()));
  if (!selection.end.layer)
    return {};

  DCHECK(!visible_selection.IsNone());
  selection.type =
      visible_selection.IsRange() ? kRangeSelection : kCaretSelection;
  return selection;
}

}  // namespace blink
