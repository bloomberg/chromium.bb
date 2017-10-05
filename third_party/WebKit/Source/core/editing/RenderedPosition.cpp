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

#include "core/editing/InlineBoxPosition.h"
#include "core/editing/InlineBoxTraversal.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/TextControlElement.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/compositing/CompositedSelectionBound.h"

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

RenderedPosition::RenderedPosition(const Position& position,
                                   TextAffinity affinity)
    : layout_object_(nullptr), inline_box_(nullptr), offset_(0) {
  if (position.IsNull())
    return;
  InlineBoxPosition box_position = ComputeInlineBoxPosition(position, affinity);
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
  InlineBoxPosition box_position = ComputeInlineBoxPosition(position, affinity);
  inline_box_ = box_position.inline_box;
  offset_ = box_position.offset_in_box;
  if (inline_box_)
    layout_object_ =
        LineLayoutAPIShim::LayoutObjectFrom(inline_box_->GetLineLayoutItem());
  else
    layout_object_ = LayoutObjectFromPosition(position);
}

InlineBox* RenderedPosition::PrevLeafChild() const {
  if (!prev_leaf_child_.has_value())
    prev_leaf_child_ = inline_box_->PrevLeafChildIgnoringLineBreak();
  return prev_leaf_child_.value();
}

InlineBox* RenderedPosition::NextLeafChild() const {
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
  InlineBox* box = AtLeftmostOffsetInBox() ? PrevLeafChild() : inline_box_;
  return box ? box->BidiLevel() : 0;
}

unsigned char RenderedPosition::BidiLevelOnRight() const {
  InlineBox* box = AtRightmostOffsetInBox() ? NextLeafChild() : inline_box_;
  return box ? box->BidiLevel() : 0;
}

RenderedPosition RenderedPosition::LeftBoundaryOfBidiRun(
    unsigned char bidi_level_of_run) {
  if (!inline_box_ || bidi_level_of_run > inline_box_->BidiLevel())
    return RenderedPosition();

  InlineBox* const box =
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

  InlineBox* const box =
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

IntRect RenderedPosition::AbsoluteRect(
    LayoutUnit* extra_width_to_end_of_line) const {
  if (IsNull())
    return IntRect();

  IntRect local_rect = PixelSnappedIntRect(layout_object_->LocalCaretRect(
      inline_box_, offset_, extra_width_to_end_of_line));
  return local_rect == IntRect()
             ? IntRect()
             : layout_object_->LocalToAbsoluteQuad(FloatRect(local_rect))
                   .EnclosingBoundingBox();
}

// Convert a local point into the coordinate system of backing coordinates.
// Also returns the backing layer if needed.
FloatPoint RenderedPosition::LocalToInvalidationBackingPoint(
    const LayoutPoint& local_point,
    GraphicsLayer** graphics_layer_backing) const {
  const LayoutBoxModelObject& paint_invalidation_container =
      layout_object_->ContainerForPaintInvalidation();
  DCHECK(paint_invalidation_container.Layer());

  FloatPoint container_point = layout_object_->LocalToAncestorPoint(
      FloatPoint(local_point), &paint_invalidation_container,
      kTraverseDocumentBoundaries);

  // A layoutObject can have no invalidation backing if it is from a detached
  // frame, or when forced compositing is disabled.
  if (paint_invalidation_container.Layer()->GetCompositingState() ==
      kNotComposited)
    return container_point;

  PaintLayer::MapPointInPaintInvalidationContainerToBacking(
      paint_invalidation_container, container_point);

  // Must not use the scrolling contents layer, so pass
  // |paintInvalidationContainer|.
  if (GraphicsLayer* graphics_layer =
          paint_invalidation_container.Layer()->GraphicsLayerBacking(
              &paint_invalidation_container)) {
    if (graphics_layer_backing)
      *graphics_layer_backing = graphics_layer;

    container_point.Move(-graphics_layer->OffsetFromLayoutObject());
  }

  return container_point;
}

void RenderedPosition::GetLocalSelectionEndpoints(
    bool selection_start,
    LayoutPoint& edge_top_in_layer,
    LayoutPoint& edge_bottom_in_layer,
    bool& is_text_direction_rtl) const {
  const LayoutRect rect = layout_object_->LocalCaretRect(inline_box_, offset_);
  if (layout_object_->Style()->IsHorizontalWritingMode()) {
    edge_top_in_layer = rect.MinXMinYCorner();
    edge_bottom_in_layer = rect.MinXMaxYCorner();
    return;
  }
  edge_top_in_layer = rect.MinXMinYCorner();
  edge_bottom_in_layer = rect.MaxXMinYCorner();

  // When text is vertical, it looks better for the start handle baseline to
  // be at the starting edge, to enclose the selection fully between the
  // handles.
  if (selection_start) {
    LayoutUnit x_swap = edge_bottom_in_layer.X();
    edge_bottom_in_layer.SetX(edge_top_in_layer.X());
    edge_top_in_layer.SetX(x_swap);
  }

  // Flipped blocks writing mode is not only vertical but also right to left.
  is_text_direction_rtl = layout_object_->HasFlippedBlocksWritingMode();
}

void RenderedPosition::PositionInGraphicsLayerBacking(
    CompositedSelectionBound& bound,
    bool selection_start) const {
  bound.layer = nullptr;
  bound.edge_top_in_layer = bound.edge_bottom_in_layer = FloatPoint();

  if (IsNull())
    return;

  LayoutPoint edge_top_in_layer;
  LayoutPoint edge_bottom_in_layer;
  GetLocalSelectionEndpoints(selection_start, edge_top_in_layer,
                             edge_bottom_in_layer, bound.is_text_direction_rtl);

  bound.edge_top_in_layer =
      LocalToInvalidationBackingPoint(edge_top_in_layer, &bound.layer);
  bound.edge_bottom_in_layer =
      LocalToInvalidationBackingPoint(edge_bottom_in_layer, nullptr);
}

LayoutPoint RenderedPosition::GetSamplePointForVisibility(
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

bool RenderedPosition::IsVisible(bool selection_start) {
  if (IsNull())
    return false;

  Node* node = layout_object_->GetNode();
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

  LayoutPoint edge_top_in_layer;
  LayoutPoint edge_bottom_in_layer;
  bool ignored2;
  GetLocalSelectionEndpoints(selection_start, edge_top_in_layer,
                             edge_bottom_in_layer, ignored2);
  LayoutPoint sample_point =
      GetSamplePointForVisibility(edge_top_in_layer, edge_bottom_in_layer);

  LayoutBox* text_control_object = ToLayoutBox(layout_object);
  LayoutPoint position_in_input(layout_object_->LocalToAncestorPoint(
      FloatPoint(sample_point), text_control_object,
      kTraverseDocumentBoundaries));
  if (!text_control_object->BorderBoxRect().Contains(position_in_input))
    return false;

  return true;
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

}  // namespace blink
