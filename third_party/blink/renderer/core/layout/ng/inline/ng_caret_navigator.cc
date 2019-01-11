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

NGCaretNavigator::Position NGCaretNavigator::LeftEdgeOf(unsigned index) const {
  DCHECK_LT(index, text_.length());
  const TextDirection direction = TextDirectionAt(index);
  return {index, IsLtr(direction) ? PositionAnchorType::kBefore
                                  : PositionAnchorType::kAfter};
}

NGCaretNavigator::Position NGCaretNavigator::RightEdgeOf(unsigned index) const {
  DCHECK_LT(index, text_.length());
  const TextDirection direction = TextDirectionAt(index);
  return {index, IsRtl(direction) ? PositionAnchorType::kBefore
                                  : PositionAnchorType::kAfter};
}

NGCaretNavigator::VisualCharacterMovementResult
NGCaretNavigator::LeftCharacterOf(unsigned index) const {
  DCHECK_LT(index, text_.length());
  if (!is_bidi_enabled_) {
    if (!index)
      return {VisualMovementResultType::kBeforeContext, base::nullopt};
    return {VisualMovementResultType::kWithinContext, index - 1};
  }

  const unsigned visual_index = visual_indices_[index];
  if (visual_index) {
    return {VisualMovementResultType::kWithinContext,
            indices_in_visual_order_[visual_index - 1]};
  }

  if (IsLtr(base_direction_))
    return {VisualMovementResultType::kBeforeContext, base::nullopt};
  return {VisualMovementResultType::kAfterContext, base::nullopt};
}

NGCaretNavigator::VisualCharacterMovementResult
NGCaretNavigator::RightCharacterOf(unsigned index) const {
  DCHECK_LT(index, text_.length());
  if (bidi_levels_.IsEmpty()) {
    if (index + 1 == text_.length())
      return {VisualMovementResultType::kAfterContext, base::nullopt};
    return {VisualMovementResultType::kWithinContext, index + 1};
  }

  const unsigned visual_index = visual_indices_[index];
  if (visual_index + 1 < text_.length()) {
    return {VisualMovementResultType::kWithinContext,
            indices_in_visual_order_[visual_index + 1]};
  }

  if (IsRtl(base_direction_))
    return {VisualMovementResultType::kBeforeContext, base::nullopt};
  return {VisualMovementResultType::kAfterContext, base::nullopt};
}

NGCaretNavigator::VisualCaretMovementResult NGCaretNavigator::LeftPositionOf(
    const Position& caret_position) const {
  const unsigned index = caret_position.index;
  if (caret_position == RightEdgeOf(index)) {
    // TODO(xiaochengh): Consider grapheme cluster
    return {VisualMovementResultType::kWithinContext, LeftEdgeOf(index)};
  }

  VisualCharacterMovementResult left_character = LeftCharacterOf(index);
  if (left_character.IsWithinContext()) {
    DCHECK(left_character.index.has_value());
    return LeftPositionOf(RightEdgeOf(left_character.index.value()));
  }

  return {left_character.type, base::nullopt};
}

NGCaretNavigator::VisualCaretMovementResult NGCaretNavigator::RightPositionOf(
    const Position& caret_position) const {
  const unsigned index = caret_position.index;
  if (caret_position == LeftEdgeOf(index)) {
    // TODO(xiaochengh): Consider grapheme cluster
    return {VisualMovementResultType::kWithinContext, RightEdgeOf(index)};
  }

  VisualCharacterMovementResult right_character = RightCharacterOf(index);
  if (right_character.IsWithinContext()) {
    DCHECK(right_character.index.has_value());
    return RightPositionOf(LeftEdgeOf(right_character.index.value()));
  }

  return {right_character.type, base::nullopt};
}

}  // namespace blink
