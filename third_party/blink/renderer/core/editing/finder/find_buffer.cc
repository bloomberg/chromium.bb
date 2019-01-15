// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_searcher_icu.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/unicode_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

FindBuffer::FindBuffer(const PositionInFlatTree& start_position) {
  DCHECK(start_position.ComputeContainerNode());
  CollectTextUntilBlockBoundary(*start_position.ComputeContainerNode());
}

FindBuffer::Results::Results() {
  empty_result_ = true;
}

FindBuffer::Results::Results(const Vector<UChar>& buffer,
                             String search_text,
                             const mojom::blink::FindOptions& options) {
  // We need to own the |search_text| because |text_searcher_| only has a
  // StringView (doesn't own the search text).
  search_text_ = search_text;
  text_searcher_.SetPattern(search_text_, options.match_case);
  text_searcher_.SetText(buffer.data(), buffer.size());
  text_searcher_.SetOffset(0);
}

FindBuffer::Results::Iterator::Iterator(TextSearcherICU* text_searcher)
    : text_searcher_(text_searcher), has_match_(true) {
  operator++();
}

const FindBuffer::BufferMatchResult FindBuffer::Results::Iterator::operator*()
    const {
  DCHECK(has_match_);
  return FindBuffer::BufferMatchResult({match_.start, match_.length});
}

void FindBuffer::Results::Iterator::operator++() {
  DCHECK(has_match_);
  has_match_ = text_searcher_->NextMatchResult(match_);
}

FindBuffer::Results::Iterator FindBuffer::Results::begin() {
  if (empty_result_)
    return end();
  text_searcher_.SetOffset(0);
  return Iterator(&text_searcher_);
}

FindBuffer::Results::Iterator FindBuffer::Results::end() const {
  return Iterator();
}

unsigned FindBuffer::Results::CountForTesting() {
  unsigned result = 0;
  for (Iterator it = begin(); it != end(); ++it) {
    ++result;
  }
  return result;
}

bool ShouldIgnoreContents(const Node& node) {
  if (!node.IsHTMLElement())
    return false;
  const HTMLElement& element = ToHTMLElement(node);
  return (!element.ShouldSerializeEndTag() && !IsHTMLInputElement(element)) ||
         IsHTMLIFrameElement(element) || IsHTMLImageElement(element) ||
         IsHTMLLegendElement(element) || IsHTMLMeterElement(element) ||
         IsHTMLObjectElement(element) || IsHTMLProgressElement(element) ||
         IsHTMLSelectElement(element) || IsHTMLStyleElement(element) ||
         IsHTMLScriptElement(element) || IsHTMLVideoElement(element) ||
         IsHTMLAudioElement(element) ||
         (element.GetDisplayLockContext() &&
          !element.GetDisplayLockContext()->IsSearchable());
}

Node* GetDisplayNoneAncestor(const Node& node) {
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(node)) {
    const ComputedStyle* style = ancestor.EnsureComputedStyle();
    if (style && style->Display() == EDisplay::kNone)
      return &ancestor;
    if (ancestor.IsDocumentNode())
      return nullptr;
  }
  return nullptr;
}

bool IsBlock(EDisplay display) {
  return display == EDisplay::kBlock || display == EDisplay::kTable ||
         display == EDisplay::kFlowRoot || display == EDisplay::kGrid ||
         display == EDisplay::kFlex || display == EDisplay::kListItem;
}

Node* GetVisibleTextNode(Node& start_node) {
  Node* node = &start_node;
  // Move to outside display none subtree if we're inside one.
  while (Node* ancestor = GetDisplayNoneAncestor(*node)) {
    if (ancestor->IsDocumentNode())
      return nullptr;
    node = FlatTreeTraversal::NextSkippingChildren(*ancestor);
    if (!node)
      return nullptr;
  }
  // Move to first text node that's visible.
  while (node) {
    const ComputedStyle* style = node->EnsureComputedStyle();
    if (ShouldIgnoreContents(*node) ||
        (style && style->Display() == EDisplay::kNone)) {
      // This element and its descendants are not visible, skip it.
      node = FlatTreeTraversal::NextSkippingChildren(*node);
      continue;
    }
    if (style && style->Visibility() == EVisibility::kVisible &&
        node->IsTextNode()) {
      return node;
    }
    // This element is hidden, but node might be visible,
    // or this is not a text node, so we move on.
    node = FlatTreeTraversal::Next(*node);
  }
  return nullptr;
}

const Node& GetLowestDisplayBlockInclusiveAncestor(const Node& start_node) {
  // Gets lowest inclusive ancestor that has block display value.
  // <div id=outer>a<div id=inner>b</div>c</div>
  // If we run this on "a" or "c" text node in we will get the outer div.
  // If we run it on the "b" text node we will get the inner div.
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(start_node)) {
    const ComputedStyle* style = ancestor.EnsureComputedStyle();
    if (style && !ancestor.IsTextNode() && IsBlock(style->Display()))
      return ancestor;
  }
  return *start_node.GetDocument().documentElement();
}

std::unique_ptr<FindBuffer::Results> FindBuffer::FindMatches(
    const WebString& search_text,
    const mojom::blink::FindOptions& options) const {
  if (buffer_.IsEmpty() || search_text.length() > buffer_.size())
    return std::make_unique<Results>();
  String search_text_16_bit = search_text;
  search_text_16_bit.Ensure16Bit();
  FoldQuoteMarksAndSoftHyphens(search_text_16_bit);
  return std::make_unique<Results>(buffer_, search_text_16_bit, options);
}

EphemeralRangeInFlatTree FindBuffer::RangeFromBufferIndex(
    unsigned start_index,
    unsigned end_index) const {
  PositionInFlatTree start_position =
      PositionAtStartOfCharacterAtIndex(start_index);
  PositionInFlatTree end_position =
      PositionAtEndOfCharacterAtIndex(end_index - 1);
  return EphemeralRangeInFlatTree(start_position, end_position);
}

// Collects text until block boundary located at or after |start_node|
// to |buffer_|. Saves the next starting node after the block to
// |node_after_block_|.
void FindBuffer::CollectTextUntilBlockBoundary(Node& start_node) {
  // Get first visible text node from |start_node|.
  Node* node = GetVisibleTextNode(start_node);
  if (!node || !node->isConnected()) {
    node_after_block_ = nullptr;
    return;
  }

  const Node& block_ancestor = GetLowestDisplayBlockInclusiveAncestor(*node);
  const Node* just_after_block = FlatTreeTraversal::Next(
      FlatTreeTraversal::LastWithinOrSelf(block_ancestor));
  const LayoutBlockFlow* last_block_flow = nullptr;

  // Collect all text under |block_ancestor| to |buffer_|,
  // unless we meet another block on the way. If so, we should split.
  // Example: <div id="outer">a<span>b</span>c<div>d</div></div>
  // Will try to collect all text in outer div but will actually
  // stop when it encounters the inner div. So buffer will be "abc".
  const Node* first_traversed_node = node;
  while (node && node != just_after_block) {
    if (ShouldIgnoreContents(*node)) {
      // Move the node so we wouldn't encounter this node or its descendants
      // later.
      buffer_.push_back(kObjectReplacementCharacter);
      node = FlatTreeTraversal::NextSkippingChildren(*node);
      continue;
    }
    const ComputedStyle* style = node->EnsureComputedStyle();
    if (style->Display() == EDisplay::kNone) {
      // This element and its descendants are not visible, skip it.
      // We can safely just check the computed style of this node since
      // we guarantee |block_ancestor| is visible.
      node = FlatTreeTraversal::NextSkippingChildren(*node);
      if (node && !FlatTreeTraversal::IsDescendantOf(*node, block_ancestor))
        break;
      continue;
    }
    // This node is in its own sub-block separate from our starting position.
    if (first_traversed_node != node && !node->IsTextNode() &&
        IsBlock(style->Display())) {
      break;
    }

    if (style->Visibility() == EVisibility::kVisible && node->IsTextNode() &&
        node->GetLayoutObject()) {
      const Text& text_node = ToText(*node);
      LayoutBlockFlow& block_flow =
          *NGOffsetMapping::GetInlineFormattingContextOf(
              *text_node.GetLayoutObject());
      if (last_block_flow && last_block_flow != block_flow) {
        // We enter another block flow.
        break;
      }
      if (!last_block_flow) {
        DCHECK(!offset_mapping_storage_);
        last_block_flow = &block_flow;
      }
      AddTextToBuffer(text_node, block_flow);
    }
    node = FlatTreeTraversal::Next(*node);
  }
  node_after_block_ = node;
  FoldQuoteMarksAndSoftHyphens(buffer_.data(), buffer_.size());
}

FindBuffer::BufferNodeMapping FindBuffer::MappingForIndex(
    unsigned index) const {
  // Get the first entry that starts at a position higher than offset, and
  // move back one entry.
  auto* it = std::upper_bound(
      buffer_node_mappings_.begin(), buffer_node_mappings_.end(), index,
      [](const unsigned offset, const BufferNodeMapping& entry) {
        return offset < entry.offset_in_buffer;
      });
  DCHECK_NE(it, buffer_node_mappings_.begin());
  auto* const entry = std::prev(it);
  return *entry;
}

PositionInFlatTree FindBuffer::PositionAtStartOfCharacterAtIndex(
    unsigned index) const {
  DCHECK_LT(index, buffer_.size());
  BufferNodeMapping entry = MappingForIndex(index);
  return ToPositionInFlatTree(offset_mapping_->GetLastPosition(
      index - entry.offset_in_buffer + entry.offset_in_mapping));
}

PositionInFlatTree FindBuffer::PositionAtEndOfCharacterAtIndex(
    unsigned index) const {
  DCHECK_LT(index, buffer_.size());
  BufferNodeMapping entry = MappingForIndex(index);
  return ToPositionInFlatTree(offset_mapping_->GetFirstPosition(
      index - entry.offset_in_buffer + entry.offset_in_mapping + 1));
}

void FindBuffer::AddTextToBuffer(const Text& text_node,
                                 LayoutBlockFlow& block_flow) {
  if (!offset_mapping_storage_) {
    offset_mapping_ =
        NGInlineNode::GetOffsetMapping(&block_flow, &offset_mapping_storage_);
  }
  const String mapped_text = offset_mapping_->GetText();
  const NGMappingUnitRange range =
      offset_mapping_->GetMappingUnitsForNode(text_node);
  unsigned last_unit_end = 0;
  bool first_unit = true;
  for (const NGOffsetMappingUnit& unit : range) {
    String text_for_unit =
        mapped_text.Substring(unit.TextContentStart(),
                              unit.TextContentEnd() - unit.TextContentStart());
    text_for_unit.Ensure16Bit();
    text_for_unit.Replace('\n', kObjectReplacementCharacter);
    if (first_unit || last_unit_end != unit.TextContentStart()) {
      // This is the first unit, or the units are not consecutive, so we need to
      // insert a new BufferNodeMapping.
      buffer_node_mappings_.push_back(
          BufferNodeMapping({buffer_.size(), unit.TextContentStart()}));
      first_unit = false;
    }
    buffer_.Append(text_for_unit.Characters16(), text_for_unit.length());
    last_unit_end = unit.TextContentEnd();
  }
}

}  // namespace blink
