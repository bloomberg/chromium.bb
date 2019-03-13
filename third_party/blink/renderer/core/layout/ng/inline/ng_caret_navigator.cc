// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_navigator.h"

#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

std::ostream& operator<<(std::ostream& out,
                         const NGCaretNavigator::Position& position) {
  return out << position.index << "/"
             << (position.IsBeforeCharacter() ? "BeforeCharacter"
                                              : "AfterCharacter");
}

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
  DCHECK_LE(offset, GetText().length());
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

NGCaretNavigator::Line NGCaretNavigator::ContainingLineOf(
    unsigned index) const {
  DCHECK_LT(index, GetText().length());
  // TODO(xiaochengh): Make it work for multi-col
  DCHECK(context_.CurrentFragment());
  unsigned last_line_end = 0;
  for (const auto child : context_.CurrentFragment()->Children()) {
    if (!child->IsLineBox())
      continue;
    const NGPhysicalLineBoxFragment* line =
        ToNGPhysicalLineBoxFragment(child.get());
    DCHECK(line->BreakToken());
    DCHECK(line->BreakToken()->IsInlineType());
    const NGInlineBreakToken* token = ToNGInlineBreakToken(line->BreakToken());
    const unsigned line_end =
        token->IsFinished() ? GetText().length() : token->TextOffset();
    if (line_end > index)
      return {last_line_end, line_end, line->BaseDirection()};
    last_line_end = line_end;
  }
  NOTREACHED();
  return {};
}

bool NGCaretNavigator::IsValidCaretPosition(const Position& position) const {
  unsigned index = position.index;
  if (position.IsAfterCharacter() && IsLineBreak(index))
    return false;
  if (IsCollapsedSpaceByLineWrap(index))
    return false;
  if (IsIgnoredInCaretMovement(index))
    return false;
  return true;
}

bool NGCaretNavigator::IsCollapsibleWhitespace(unsigned index) const {
  DCHECK_LT(index, GetText().length());
  if (GetText()[index] != kSpaceCharacter)
    return false;
  const NGInlineItem& item = GetData().FindItemForTextOffset(index);
  return item.Style()->CollapseWhiteSpace();
}

bool NGCaretNavigator::IsLineBreak(unsigned index) const {
  DCHECK_LT(index, GetText().length());
  return GetText()[index] == kNewlineCharacter;
}

bool NGCaretNavigator::IsCollapsedSpaceByLineWrap(unsigned index) const {
  DCHECK_LT(index, GetText().length());
  if (!IsCollapsibleWhitespace(index))
    return false;
  return index + 1 == ContainingLineOf(index).end_offset;
}

bool NGCaretNavigator::IsIgnoredInCaretMovement(unsigned index) const {
  DCHECK_LT(index, GetText().length());
  const NGInlineItem& item = GetData().FindItemForTextOffset(index);

  // Caret navigation works on text, atomic inlines and non-ZWS controls only.
  switch (item.Type()) {
    case NGInlineItem::kText:
    case NGInlineItem::kAtomicInline:
      break;
    case NGInlineItem::kControl:
      if (GetText()[index] == kZeroWidthSpaceCharacter)
        return true;
      break;
    default:
      return true;
  }

  // Ignore CSS generate contents.
  // TODO(xiaochengh): This might be general enough to be merged into
  // |NGInlineItem| as a member function.
  DCHECK(item.GetLayoutObject());
  const LayoutObject* object = item.GetLayoutObject();
  if (const auto* text_fragment = ToLayoutTextFragmentOrNull(object)) {
    // ::first-letter |LayoutTextFragment| returns null for |GetNode()|. Check
    // |AssociatedTextNode()| to see if it's created by a text node.
    if (!text_fragment->AssociatedTextNode())
      return true;
  } else {
    if (!object->NonPseudoNode())
      return true;
  }

  // Ignore collapsed whitespaces that not visually at line end due to bidi.
  // Caret movement should move over them as if they don't exist to match the
  // existing behavior.
  return IsCollapsedSpaceByLineWrap(index) &&
         index != VisualLastCharacterOf(ContainingLineOf(index));
}

bool NGCaretNavigator::IsEnterableChildContext(unsigned index) const {
  DCHECK_LT(index, GetText().length());
  if (GetText()[index] != kObjectReplacementCharacter)
    return false;

  const NGInlineItem& item = GetData().FindItemForTextOffset(index);
  if (item.Type() != NGInlineItem::kAtomicInline)
    return false;
  DCHECK(item.GetLayoutObject());
  const LayoutObject* object = item.GetLayoutObject();
  if (!object->IsLayoutBlockFlow())
    return false;
  if (!object->NonPseudoNode() || !object->GetNode()->IsElementNode())
    return false;
  const Element* node = ToElement(object->GetNode());
  return !node->GetShadowRoot() || !node->GetShadowRoot()->IsUserAgent();
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

Vector<int32_t, 32> NGCaretNavigator::CharacterIndicesInVisualOrder(
    const Line& line) const {
  DCHECK(IsBidiEnabled());

  Vector<UBiDiLevel, 32> levels;
  levels.ReserveCapacity(line.end_offset - line.start_offset);
  for (unsigned i = line.start_offset; i < line.end_offset; ++i)
    levels.push_back(BidiLevelAt(i));

  Vector<int32_t, 32> indices(levels.size());
  NGBidiParagraph::IndicesInVisualOrder(levels, &indices);

  for (auto& index : indices)
    index += line.start_offset;
  return indices;
}

unsigned NGCaretNavigator::VisualMostForwardCharacterOf(
    const Line& line,
    MoveDirection direction) const {
  if (!IsBidiEnabled()) {
    if (direction == MoveDirection::kTowardsLeft)
      return line.start_offset;
    return line.end_offset - 1;
  }

  const auto indices_in_visual_order = CharacterIndicesInVisualOrder(line);
  if (direction == MoveDirection::kTowardsLeft)
    return indices_in_visual_order.front();
  return indices_in_visual_order.back();
}

unsigned NGCaretNavigator::VisualFirstCharacterOf(const Line& line) const {
  return VisualMostForwardCharacterOf(line, IsLtr(line.base_direction)
                                                ? MoveDirection::kTowardsLeft
                                                : MoveDirection::kTowardsRight);
}

unsigned NGCaretNavigator::VisualLastCharacterOf(const Line& line) const {
  return VisualMostForwardCharacterOf(line, IsLtr(line.base_direction)
                                                ? MoveDirection::kTowardsRight
                                                : MoveDirection::kTowardsLeft);
}

NGCaretNavigator::VisualCharacterMovementResult
NGCaretNavigator::MoveCharacterInternal(unsigned index,
                                        MoveDirection move_direction) const {
  const Line line = ContainingLineOf(index);

  if (index == VisualMostForwardCharacterOf(line, move_direction)) {
    if (TowardsSameDirection(move_direction, line.base_direction)) {
      if (line.end_offset == GetText().length())
        return {VisualMovementResultType::kAfterContext, base::nullopt};
      const Line next_line = ContainingLineOf(line.end_offset);
      return {VisualMovementResultType::kWithinContext,
              VisualFirstCharacterOf(next_line)};
    }

    if (!line.start_offset)
      return {VisualMovementResultType::kBeforeContext, base::nullopt};
    const Line last_line = ContainingLineOf(line.start_offset - 1);
    return {VisualMovementResultType::kWithinContext,
            VisualLastCharacterOf(last_line)};
  }

  if (!IsBidiEnabled()) {
    if (move_direction == MoveDirection::kTowardsLeft)
      return {VisualMovementResultType::kWithinContext, index - 1};
    return {VisualMovementResultType::kWithinContext, index + 1};
  }

  Vector<int32_t, 32> indices_in_visual_order =
      CharacterIndicesInVisualOrder(line);
  const int32_t* visual_location = std::find(
      indices_in_visual_order.begin(), indices_in_visual_order.end(), index);
  DCHECK_NE(visual_location, indices_in_visual_order.end());
  if (move_direction == MoveDirection::kTowardsLeft) {
    DCHECK_NE(visual_location, indices_in_visual_order.begin());
    return {VisualMovementResultType::kWithinContext,
            *std::prev(visual_location)};
  }
  DCHECK_NE(std::next(visual_location), indices_in_visual_order.end());
  return {VisualMovementResultType::kWithinContext,
          *std::next(visual_location)};
}

NGCaretNavigator::VisualCaretMovementResult NGCaretNavigator::LeftPositionOf(
    const Position& caret_position) const {
  return MoveCaretInternal(caret_position, MoveDirection::kTowardsLeft);
}

NGCaretNavigator::VisualCaretMovementResult NGCaretNavigator::RightPositionOf(
    const Position& caret_position) const {
  return MoveCaretInternal(caret_position, MoveDirection::kTowardsRight);
}

NGCaretNavigator::UnvalidatedVisualCaretMovementResult
NGCaretNavigator::MoveCaretWithoutValidation(
    const Position& caret_position,
    MoveDirection move_direction) const {
  const unsigned index = caret_position.index;
  const MoveDirection opposite_direction = OppositeDirectionOf(move_direction);
  if (caret_position == EdgeOfInternal(index, opposite_direction)) {
    // TODO(xiaochengh): Consider grapheme cluster
    return {VisualMovementResultType::kWithinContext,
            EdgeOfInternal(index, move_direction),
            !IsIgnoredInCaretMovement(index)};
  }

  VisualCharacterMovementResult forward_character =
      MoveCharacterInternal(index, move_direction);
  if (!forward_character.IsWithinContext())
    return {forward_character.type};

  DCHECK(forward_character.index.has_value());
  const Position forward_caret =
      EdgeOfInternal(*forward_character.index, opposite_direction);
  return {VisualMovementResultType::kWithinContext, forward_caret};
}

NGCaretNavigator::VisualCaretMovementResult NGCaretNavigator::MoveCaretInternal(
    const Position& caret_position,
    MoveDirection move_direction) const {
  bool has_passed_character = false;
  base::Optional<Position> last_position;
  for (Position runner = caret_position;
       !has_passed_character || !IsValidCaretPosition(runner);) {
    const UnvalidatedVisualCaretMovementResult next =
        MoveCaretWithoutValidation(runner, move_direction);
    if (next.type != VisualMovementResultType::kWithinContext)
      return {next.type, base::nullopt};

    if (next.has_passed_character) {
      has_passed_character = true;

      const unsigned last_passed_character = next.position->index;
      if (IsEnterableChildContext(last_passed_character))
        return {VisualMovementResultType::kEnteredChildContext, runner};
    }

    runner = *next.position;
    last_position = runner;

    // TODO(xiaochengh): Handle the case where we reach a different line with a
    // different base direction, which occurs with 'unicode-bidi: plain-text'.
  }
  DCHECK(last_position.has_value());
  return {VisualMovementResultType::kWithinContext, *last_position};
}

NGCaretNavigator::Position NGCaretNavigator::LeftmostPositionInFirstLine()
    const {
  Line first_line = ContainingLineOf(0);
  unsigned leftmost_character =
      VisualMostForwardCharacterOf(first_line, MoveDirection::kTowardsLeft);
  // TODO(xiaochengh): Handle if the caret position is invalid.
  return LeftEdgeOf(leftmost_character);
}

NGCaretNavigator::Position NGCaretNavigator::RightmostPositionInFirstLine()
    const {
  Line first_line = ContainingLineOf(0);
  unsigned rightmost_character =
      VisualMostForwardCharacterOf(first_line, MoveDirection::kTowardsRight);
  // TODO(xiaochengh): Handle if the caret position is invalid.
  return RightEdgeOf(rightmost_character);
}

NGCaretNavigator::Position NGCaretNavigator::LeftmostPositionInLastLine()
    const {
  Line last_line = ContainingLineOf(GetText().length() - 1);
  unsigned leftmost_character =
      VisualMostForwardCharacterOf(last_line, MoveDirection::kTowardsLeft);
  // TODO(xiaochengh): Handle if the caret position is invalid.
  return LeftEdgeOf(leftmost_character);
}

NGCaretNavigator::Position NGCaretNavigator::RightmostPositionInLastLine()
    const {
  Line last_line = ContainingLineOf(GetText().length() - 1);
  unsigned rightmost_character =
      VisualMostForwardCharacterOf(last_line, MoveDirection::kTowardsRight);
  // TODO(xiaochengh): Handle if the caret position is invalid.
  return RightEdgeOf(rightmost_character);
}

}  // namespace blink
