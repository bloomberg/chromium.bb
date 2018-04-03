// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/TextOffsetMapping.h"

#include "core/dom/Node.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Position.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/layout/LayoutBlock.h"

namespace blink {

namespace {

// Note: Since "inline-block" and "float" are not considered as text segment
// boundary, we should not consider them as block for scanning.
// Example in selection text:
//  <div>|ab<b style="display:inline-box">CD</b>ef</div>
//  selection.modify('extent', 'forward', 'word')
//  <div>^ab<b style="display:inline-box">CD</b>ef|</div>
// See also test cases for "inline-block" and "float" in "TextIterator.cpp".
bool IsBlockForTextOffsetMapping(const LayoutObject& object) {
  return object.IsLayoutBlock() && !object.IsAtomicInlineLevel() &&
         !object.IsFloatingOrOutOfFlowPositioned();
}

PositionInFlatTree ComputeEndPosition(const LayoutBlock& block) {
  for (LayoutObject* runner = block.LastLeafChild(); runner;
       runner = runner->PreviousInPreOrder(&block)) {
    Node* node = runner->NonPseudoNode();
    if (!node)
      continue;
    if (node->IsTextNode())
      return PositionInFlatTree(node, ToText(node)->length());
    return PositionInFlatTree::AfterNode(*node);
  }
  if (Node* block_node = block.NonPseudoNode()) {
    // Empty DIV reaches here.
    return PositionInFlatTree::LastPositionInNode(*block_node);
  }
  // TODO(editing-dev): Once we have the test case reaches here, we should
  // change caller to handle this.
  NOTREACHED() << block.DebugName();
  return PositionInFlatTree();
}

PositionInFlatTree ComputeStartPosition(const LayoutBlock& block) {
  for (LayoutObject* runner = block.FirstChild(); runner;
       runner = runner->NextInPreOrder(&block)) {
    Node* node = runner->NonPseudoNode();
    if (!node)
      continue;
    if (node->IsTextNode())
      return PositionInFlatTree(node, 0);
    return PositionInFlatTree::BeforeNode(*node);
  }
  if (Node* block_node = block.NonPseudoNode()) {
    // Empty DIV reaches here.
    return PositionInFlatTree::FirstPositionInNode(*block_node);
  }
  // TODO(editing-dev): Once we have the test case reaches here, we should
  // change caller to handle this.
  NOTREACHED() << block.DebugName();
  return PositionInFlatTree();
}

// Returns range of block containing |position|.
// Note: Container node of |position| should be associated to |LayoutObject|.
EphemeralRangeInFlatTree ComputeBlockRange(const LayoutBlock& block) {
  DCHECK(!block.IsAtomicInlineLevel());
  const PositionInFlatTree& start = ComputeStartPosition(block);
  DCHECK(start.IsNotNull()) << block.DebugName();
  const PositionInFlatTree& end = ComputeEndPosition(block);
  DCHECK(end.IsNotNull()) << block.DebugName();
  return EphemeralRangeInFlatTree(start, end);
}

String Ensure16Bit(const String& text) {
  String text16(text);
  text16.Ensure16Bit();
  return text16;
}

}  // namespace

TextOffsetMapping::TextOffsetMapping(const LayoutBlock& block,
                                     const TextIteratorBehavior behavior)
    : behavior_(behavior),
      range_(ComputeBlockRange(block)),
      text16_(Ensure16Bit(PlainText(range_, behavior_))) {}

TextOffsetMapping::TextOffsetMapping(const LayoutBlock& block)
    : TextOffsetMapping(block,
                        TextIteratorBehavior::Builder()
                            .SetEmitsCharactersBetweenAllVisiblePositions(true)
                            .SetEmitsSmallXForTextSecurity(true)
                            .Build()) {}

int TextOffsetMapping::ComputeTextOffset(
    const PositionInFlatTree& position) const {
  return TextIteratorInFlatTree::RangeLength(range_.StartPosition(), position,
                                             behavior_);
}

PositionInFlatTree TextOffsetMapping::GetPositionBefore(unsigned offset) const {
  DCHECK_LE(offset, text16_.length());
  CharacterIteratorInFlatTree iterator(range_, behavior_);
  if (offset >= 1 && offset == text16_.length()) {
    iterator.Advance(offset - 1);
    return iterator.GetPositionAfter();
  }
  iterator.Advance(offset);
  return iterator.GetPositionBefore();
}

PositionInFlatTree TextOffsetMapping::GetPositionAfter(unsigned offset) const {
  DCHECK_LE(offset, text16_.length());
  CharacterIteratorInFlatTree iterator(range_, behavior_);
  if (offset > 0)
    iterator.Advance(offset - 1);
  return iterator.GetPositionAfter();
}

EphemeralRangeInFlatTree TextOffsetMapping::ComputeRange(unsigned start,
                                                         unsigned end) const {
  DCHECK_LE(end, text16_.length());
  DCHECK_LE(start, end);
  if (start == end)
    return EphemeralRangeInFlatTree();
  return EphemeralRangeInFlatTree(GetPositionBefore(start),
                                  GetPositionAfter(end));
}

unsigned TextOffsetMapping::FindNonWhitespaceCharacterFrom(
    unsigned offset) const {
  for (unsigned runner = offset; runner < text16_.length(); ++runner) {
    if (!IsWhitespace(text16_[runner]))
      return runner;
  }
  return text16_.length();
}

// static
const LayoutBlock& TextOffsetMapping::ComputeContainigBlock(
    const PositionInFlatTree& position) {
  const Node& container = *position.ComputeContainerNode();
  for (LayoutObject* runner = container.GetLayoutObject(); runner;
       runner = runner->ContainingBlock()) {
    if (IsBlockForTextOffsetMapping(*runner))
      return ToLayoutBlock(*runner);
  }
  NOTREACHED() << position;
  return *ToLayoutBlock(container.GetLayoutObject());
}

// static
LayoutBlock* TextOffsetMapping::NextBlockFor(const LayoutBlock& block) {
  for (LayoutObject* runner = block.NextInPreOrderAfterChildren(); runner;
       runner = runner->NextInPreOrder()) {
    if (IsBlockForTextOffsetMapping(*runner))
      return ToLayoutBlock(runner);
  }
  return nullptr;
}

// static
LayoutBlock* TextOffsetMapping::PreviousBlockFor(const LayoutBlock& block) {
  for (LayoutObject* runner = block.PreviousInPreOrder(); runner;
       runner = runner->PreviousInPreOrder()) {
    if (IsBlockForTextOffsetMapping(*runner) && !block.IsDescendantOf(runner))
      return ToLayoutBlock(runner);
  }
  return nullptr;
}

}  // namespace blink
