// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_navigator.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"

namespace blink {

NGCaretNavigator::~NGCaretNavigator() = default;

NGCaretNavigator::NGCaretNavigator(const LayoutBlockFlow& context)
    : context_(context),
      disallow_transition_(context.GetDocument().Lifecycle()) {
  DCHECK(RuntimeEnabledFeatures::BidiCaretAffinityEnabled());
  DCHECK(context.IsLayoutNGMixin());
  DCHECK(context.ChildrenInline());
  DCHECK(context.GetNGInlineNodeData());
  DCHECK(!context.GetDocument().NeedsLayoutTreeUpdate());
}

const NGInlineNodeData& NGCaretNavigator::GetData() const {
  return *context_.GetNGInlineNodeData();
}

const String& NGCaretNavigator::GetText() const {
  return GetData().text_content;
}

bool NGCaretNavigator::IsBidiEnabled() const {
  return GetData().IsBidiEnabled();
}

UBiDiLevel NGCaretNavigator::BidiLevelAt(unsigned index) const {
  DCHECK_LT(index, GetText().length());
  if (!IsBidiEnabled())
    return 0;
  return GetData().FindItemForTextOffset(index).BidiLevel();
}

TextDirection NGCaretNavigator::TextDirectionAt(unsigned index) const {
  UBiDiLevel level = BidiLevelAt(index);
  return DirectionFromLevel(level);
}

bool NGCaretNavigator::OffsetIsBidiBoundary(unsigned offset) const {
  DCHECK_LE(offset, GetText().length());
  if (!IsBidiEnabled())
    return false;
  if (!offset || offset == GetText().length())
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

  if (offset < GetText().length())
    return {offset, PositionAnchorType::kBefore};
  return {GetText().length() - 1, PositionAnchorType::kAfter};
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
  DCHECK_LT(index, GetText().length());
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

Vector<int32_t, 32> NGCaretNavigator::IndicesInVisualOrder() const {
  DCHECK(IsBidiEnabled());

  Vector<UBiDiLevel, 32> levels;
  levels.resize(GetText().length());
  for (unsigned i = 0; i < GetText().length(); ++i)
    levels[i] = BidiLevelAt(i);

  Vector<int32_t, 32> result;
  result.resize(levels.size());
  NGBidiParagraph::IndicesInVisualOrder(levels, &result);
  return result;
}

unsigned NGCaretNavigator::VisualIndexOf(unsigned index) const {
  if (!IsBidiEnabled())
    return index;

  const auto indices_in_visual_order = IndicesInVisualOrder();
  const auto* visual_iterator = std::find(indices_in_visual_order.begin(),
                                          indices_in_visual_order.end(), index);
  DCHECK_NE(visual_iterator, indices_in_visual_order.end());
  return std::distance(indices_in_visual_order.begin(), visual_iterator);
}

NGCaretNavigator::VisualCharacterMovementResult
NGCaretNavigator::MoveCharacterInternal(unsigned index,
                                        MoveDirection move_direction) const {
  DCHECK_LT(index, GetText().length());
  const unsigned visual_index = VisualIndexOf(index);
  const TextDirection base_direction = GetData().BaseDirection();

  const base::Optional<unsigned> maybe_result_visual_index =
      MoveVisualIndex(visual_index, GetText().length(), move_direction);
  if (!maybe_result_visual_index.has_value()) {
    if (TowardsSameDirection(move_direction, base_direction))
      return {VisualMovementResultType::kAfterContext, base::nullopt};
    return {VisualMovementResultType::kBeforeContext, base::nullopt};
  }

  const unsigned result_visual_index = maybe_result_visual_index.value();
  const unsigned result_index =
      IsBidiEnabled() ? IndicesInVisualOrder()[result_visual_index]
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
