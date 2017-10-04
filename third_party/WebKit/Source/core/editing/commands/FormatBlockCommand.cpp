/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/commands/FormatBlockCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/Position.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLElement.h"
#include "core/html_names.h"

namespace blink {

using namespace HTMLNames;

static Node* EnclosingBlockToSplitTreeTo(Node* start_node);
static bool IsElementForFormatBlock(const QualifiedName& tag_name);
static inline bool IsElementForFormatBlock(Node* node) {
  return node->IsElementNode() &&
         IsElementForFormatBlock(ToElement(node)->TagQName());
}

static Element* EnclosingBlockFlowElement(
    const VisiblePosition& visible_position) {
  if (visible_position.IsNull())
    return nullptr;
  return EnclosingBlockFlowElement(
      *visible_position.DeepEquivalent().AnchorNode());
}

FormatBlockCommand::FormatBlockCommand(Document& document,
                                       const QualifiedName& tag_name)
    : ApplyBlockElementCommand(document, tag_name), did_apply_(false) {}

void FormatBlockCommand::FormatSelection(
    const VisiblePosition& start_of_selection,
    const VisiblePosition& end_of_selection,
    EditingState* editing_state) {
  if (!IsElementForFormatBlock(TagName()))
    return;
  ApplyBlockElementCommand::FormatSelection(start_of_selection,
                                            end_of_selection, editing_state);
  did_apply_ = true;
}

void FormatBlockCommand::FormatRange(const Position& start,
                                     const Position& end,
                                     const Position& end_of_selection,
                                     HTMLElement*& block_element,
                                     EditingState* editing_state) {
  Element* ref_element = EnclosingBlockFlowElement(CreateVisiblePosition(end));
  Element* root = RootEditableElementOf(start);
  // Root is null for elements with contenteditable=false.
  if (!root || !ref_element)
    return;

  Node* node_to_split_to = EnclosingBlockToSplitTreeTo(start.AnchorNode());
  Node* outer_block =
      (start.AnchorNode() == node_to_split_to)
          ? start.AnchorNode()
          : SplitTreeToNode(start.AnchorNode(), node_to_split_to);
  Node* node_after_insertion_position = outer_block;
  Range* range = Range::Create(GetDocument(), start, end_of_selection);

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (IsElementForFormatBlock(ref_element->TagQName()) &&
      CreateVisiblePosition(start).DeepEquivalent() ==
          StartOfBlock(CreateVisiblePosition(start)).DeepEquivalent() &&
      (CreateVisiblePosition(end).DeepEquivalent() ==
           EndOfBlock(CreateVisiblePosition(end)).DeepEquivalent() ||
       IsNodeVisiblyContainedWithin(*ref_element, *range)) &&
      ref_element != root && !root->IsDescendantOf(ref_element)) {
    // Already in a block element that only contains the current paragraph
    if (ref_element->HasTagName(TagName()))
      return;
    node_after_insertion_position = ref_element;
  }

  if (!block_element) {
    // Create a new blockquote and insert it as a child of the root editable
    // element. We accomplish this by splitting all parents of the current
    // paragraph up to that point.
    block_element = CreateBlockElement();
    InsertNodeBefore(block_element, node_after_insertion_position,
                     editing_state);
    if (editing_state->IsAborted())
      return;
    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  }

  Position last_paragraph_in_block_node =
      block_element->lastChild()
          ? Position::AfterNode(*block_element->lastChild())
          : Position();
  bool was_end_of_paragraph =
      IsEndOfParagraph(CreateVisiblePosition(last_paragraph_in_block_node));

  MoveParagraphWithClones(CreateVisiblePosition(start),
                          CreateVisiblePosition(end), block_element,
                          outer_block, editing_state);
  if (editing_state->IsAborted())
    return;

  // Copy the inline style of the original block element to the newly created
  // block-style element.
  if (outer_block != node_after_insertion_position &&
      ToHTMLElement(node_after_insertion_position)->hasAttribute(styleAttr))
    block_element->setAttribute(
        styleAttr,
        ToHTMLElement(node_after_insertion_position)->getAttribute(styleAttr));

  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (was_end_of_paragraph &&
      !IsEndOfParagraph(CreateVisiblePosition(last_paragraph_in_block_node)) &&
      !IsStartOfParagraph(CreateVisiblePosition(last_paragraph_in_block_node)))
    InsertBlockPlaceholder(last_paragraph_in_block_node, editing_state);
}

Element* FormatBlockCommand::ElementForFormatBlockCommand(
    const EphemeralRange& range) {
  Node* common_ancestor = range.CommonAncestorContainer();
  while (common_ancestor && !IsElementForFormatBlock(common_ancestor))
    common_ancestor = common_ancestor->parentNode();

  if (!common_ancestor)
    return 0;

  Element* element =
      RootEditableElement(*range.StartPosition().ComputeContainerNode());
  if (!element || common_ancestor->contains(element))
    return 0;

  return common_ancestor->IsElementNode() ? ToElement(common_ancestor) : 0;
}

bool IsElementForFormatBlock(const QualifiedName& tag_name) {
  DEFINE_STATIC_LOCAL(
      HashSet<QualifiedName>, block_tags,
      ({
          addressTag, articleTag, asideTag,  blockquoteTag, ddTag,     divTag,
          dlTag,      dtTag,      footerTag, h1Tag,         h2Tag,     h3Tag,
          h4Tag,      h5Tag,      h6Tag,     headerTag,     hgroupTag, mainTag,
          navTag,     pTag,       preTag,    sectionTag,
      }));
  return block_tags.Contains(tag_name);
}

Node* EnclosingBlockToSplitTreeTo(Node* start_node) {
  DCHECK(start_node);
  Node* last_block = start_node;
  for (Node& runner : NodeTraversal::InclusiveAncestorsOf(*start_node)) {
    if (!HasEditableStyle(runner))
      return last_block;
    if (IsTableCell(&runner) || IsHTMLBodyElement(&runner) ||
        !runner.parentNode() || !HasEditableStyle(*runner.parentNode()) ||
        IsElementForFormatBlock(&runner))
      return &runner;
    if (IsEnclosingBlock(&runner))
      last_block = &runner;
    if (IsHTMLListElement(&runner))
      return HasEditableStyle(*runner.parentNode()) ? runner.parentNode()
                                                    : &runner;
  }
  return last_block;
}

}  // namespace blink
