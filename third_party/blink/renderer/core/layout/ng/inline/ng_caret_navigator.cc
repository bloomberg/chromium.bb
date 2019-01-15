// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_navigator.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"

namespace blink {

NGCaretNavigator::~NGCaretNavigator() = default;

NGCaretNavigator::NGCaretNavigator(NGCaretNavigator&& other)
    : text_(other.text_),
      bidi_levels_(std::move(other.bidi_levels_)),
      indices_in_visual_order_(std::move(other.indices_in_visual_order_)),
      visual_indices_(std::move(other.visual_indices_)),
      is_bidi_enabled_(other.is_bidi_enabled_),
      base_direction_(other.base_direction_) {}

NGCaretNavigator::NGCaretNavigator(const NGInlineNodeData& data)
    : text_(data.text_content),
      is_bidi_enabled_(data.IsBidiEnabled()),
      base_direction_(data.BaseDirection()) {
  if (!is_bidi_enabled_)
    return;

  bidi_levels_.resize(text_.length());
  for (const NGInlineItem& item : data.items) {
    if (!item.Length())
      continue;
    for (unsigned i = item.StartOffset(); i < item.EndOffset(); ++i) {
      DCHECK_LT(i, bidi_levels_.size());
      bidi_levels_[i] = item.BidiLevel();
    }
  }

  indices_in_visual_order_.resize(text_.length());
  NGBidiParagraph::IndicesInVisualOrder(bidi_levels_,
                                        &indices_in_visual_order_);

  visual_indices_.resize(text_.length());
  for (unsigned i = 0; i < indices_in_visual_order_.size(); ++i)
    visual_indices_[indices_in_visual_order_[i]] = i;
}

UBiDiLevel NGCaretNavigator::BidiLevelAt(unsigned index) const {
  DCHECK_LT(index, text_.length());
  if (!is_bidi_enabled_)
    return 0;
  DCHECK_LT(index, bidi_levels_.size());
  return bidi_levels_[index];
}

TextDirection NGCaretNavigator::TextDirectionAt(unsigned index) const {
  UBiDiLevel level = BidiLevelAt(index);
  return DirectionFromLevel(level);
}

bool NGCaretNavigator::OffsetIsBidiBoundary(unsigned offset) const {
  DCHECK_LE(offset, text_.length());
  if (!is_bidi_enabled_)
    return false;
  if (!offset || offset == text_.length())
    return false;
  return BidiLevelAt(offset - 1) != BidiLevelAt(offset);
}

NGCaretNavigator::Position
NGCaretNavigator::CaretPositionFromTextContentOffsetAndAffinity(
    unsigned offset,
    TextAffinity affinity) const {
  // Callers sometimes pass in (0, upstream) or (length, downstream), which
  // originate from legacy callers. Make sure they are fixed up.
  // TODO(xiaochengh): Catch and eliminate such callers.
  if (affinity == TextAffinity::kUpstream) {
    if (offset)
      return {offset - 1, PositionAnchorType::kAfter};
    return {0, PositionAnchorType::kBefore};
  }

  if (offset < text_.length())
    return {offset, PositionAnchorType::kBefore};
  return {text_.length() - 1, PositionAnchorType::kAfter};
}

// static
NGCaretNavigator::MoveDirection NGCaretNavigator::OppositeDirectionOf(
    MoveDirection direction) {
  if (direction == MoveDirection::kTowardsLeft)
    return MoveDirection::kTowardsRight;
  return MoveDirection::kTowardsLeft;
}

// static
bool NGCaretNavigator::TowardsSameDirection(MoveDirection move_direction,
                                            TextDirection text_direction) {
  if (IsLtr(text_direction))
    return move_direction == MoveDirection::kTowardsRight;
  return move_direction == MoveDirection::kTowardsLeft;
}

NGCaretNavigator::Position NGCaretNavigator::LeftEdgeOf(unsigned index) const {
  return EdgeOfInternal(index, MoveDirection::kTowardsLeft);
}

NGCaretNavigator::Position NGCaretNavigator::RightEdgeOf(unsigned index) const {
  return EdgeOfInternal(index, MoveDirection::kTowardsRight);
}

NGCaretNavigator::Position NGCaretNavigator::EdgeOfInternal(
    unsigned index,
    MoveDirection edge_direction) const {
  DCHECK_LT(index, text_.length());
  const TextDirection character_direction = TextDirectionAt(index);
  return {index, TowardsSameDirection(edge_direction, character_direction)
                     ? PositionAnchorType::kAfter
                     : PositionAnchorType::kBefore};
}

NGCaretNavigator::VisualCharacterMovementResult
NGCaretNavigator::LeftCharacterOf(unsigned index) const {
  return MoveCharacterInternal(index, MoveDirection::kTowardsLeft);
}

NGCaretNavigator::VisualCharacterMovementResult
NGCaretNavigator::RightCharacterOf(unsigned index) const {
  return MoveCharacterInternal(index, MoveDirection::kTowardsRight);
}

// static
base::Optional<unsigned> NGCaretNavigator::MoveVisualIndex(
    unsigned visual_index,
    unsigned length,
    MoveDirection move_direction) {
  if (move_direction == MoveDirection::kTowardsLeft) {
    if (!visual_index)
      return base::nullopt;
    return visual_index - 1;
  }

  if (visual_index + 1 == length)
    return base::nullopt;
  return visual_index + 1;
}

NGCaretNavigator::VisualCharacterMovementResult
NGCaretNavigator::MoveCharacterInternal(unsigned index,
                                        MoveDirection move_direction) const {
  DCHECK_LT(index, text_.length());
  const unsigned visual_index =
      is_bidi_enabled_ ? visual_indices_[index] : index;

  const base::Optional<unsigned> maybe_result_visual_index =
      MoveVisualIndex(visual_index, text_.length(), move_direction);
  if (!maybe_result_visual_index.has_value()) {
    if (TowardsSameDirection(move_direction, base_direction_))
      return {VisualMovementResultType::kAfterContext, base::nullopt};
    return {VisualMovementResultType::kBeforeContext, base::nullopt};
  }

  const unsigned result_visual_index = maybe_result_visual_index.value();
  const unsigned result_index =
      is_bidi_enabled_ ? indices_in_visual_order_[result_visual_index]
                       : result_visual_index;
  return {VisualMovementResultType::kWithinContext, result_index};
}

NGCaretNavigator::VisualCaretMovementResult NGCaretNavigator::LeftPositionOf(
    const Position& caret_position) const {
  return MoveCaretInternal(caret_position, MoveDirection::kTowardsLeft);
}

NGCaretNavigator::VisualCaretMovementResult NGCaretNavigator::RightPositionOf(
    const Position& caret_position) const {
  return MoveCaretInternal(caret_position, MoveDirection::kTowardsRight);
}

NGCaretNavigator::VisualCaretMovementResult NGCaretNavigator::MoveCaretInternal(
    const Position& caret_position,
    MoveDirection move_direction) const {
  const unsigned index = caret_position.index;
  const MoveDirection opposite_direction = OppositeDirectionOf(move_direction);
  if (caret_position == EdgeOfInternal(index, opposite_direction)) {
    // TODO(xiaochengh): Consider grapheme cluster
    return {VisualMovementResultType::kWithinContext,
            EdgeOfInternal(index, move_direction)};
  }

  VisualCharacterMovementResult forward_character =
      MoveCharacterInternal(index, move_direction);
  if (forward_character.IsWithinContext()) {
    DCHECK(forward_character.index.has_value());
    const Position forward_caret =
        EdgeOfInternal(forward_character.index.value(), opposite_direction);
    return MoveCaretInternal(forward_caret, move_direction);
  }

  return {forward_character.type, base::nullopt};
}

}  // namespace blink
