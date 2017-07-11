/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
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

#include "core/editing/iterators/TextIterator.h"

#include <unicode/utf16.h>
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/Document.h"
#include "core/dom/ShadowRoot.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/Position.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/TextIteratorTextNodeHandler.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/TextControlElement.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableRow.h"
#include "platform/fonts/Font.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

namespace {

template <typename Strategy>
TextIteratorBehavior AdjustBehaviorFlags(const TextIteratorBehavior&);

template <>
TextIteratorBehavior AdjustBehaviorFlags<EditingStrategy>(
    const TextIteratorBehavior& behavior) {
  if (!behavior.ForSelectionToString())
    return behavior;
  return TextIteratorBehavior::Builder(behavior)
      .SetExcludeAutofilledValue(true)
      .Build();
}

template <>
TextIteratorBehavior AdjustBehaviorFlags<EditingInFlatTreeStrategy>(
    const TextIteratorBehavior& behavior) {
  return TextIteratorBehavior::Builder(behavior)
      .SetExcludeAutofilledValue(behavior.ForSelectionToString() ||
                                 behavior.ExcludeAutofilledValue())
      .SetEntersOpenShadowRoots(false)
      .SetEntersTextControls(false)
      .Build();
}

static inline bool HasDisplayContents(const Node& node) {
  return node.IsElementNode() && ToElement(node).HasDisplayContentsStyle();
}

// Checks if |advance()| skips the descendants of |node|, which is the case if
// |node| is neither a shadow root nor the owner of a layout object.
static bool NotSkipping(const Node& node) {
  return node.GetLayoutObject() || HasDisplayContents(node) ||
         (node.IsShadowRoot() && node.OwnerShadowHost()->GetLayoutObject());
}

template <typename Strategy>
Node* StartNode(Node* start_container, int start_offset) {
  if (start_container->IsCharacterDataNode())
    return start_container;
  if (Node* child = Strategy::ChildAt(*start_container, start_offset))
    return child;
  if (!start_offset)
    return start_container;
  return Strategy::NextSkippingChildren(*start_container);
}

template <typename Strategy>
Node* EndNode(const Node& end_container, int end_offset) {
  if (!end_container.IsCharacterDataNode() && end_offset > 0)
    return Strategy::ChildAt(end_container, end_offset - 1);
  return nullptr;
}

// This function is like Range::pastLastNode, except for the fact that it can
// climb up out of shadow trees and ignores all nodes that will be skipped in
// |advance()|.
template <typename Strategy>
Node* PastLastNode(const Node& range_end_container, int range_end_offset) {
  if (range_end_offset >= 0 && !range_end_container.IsCharacterDataNode() &&
      NotSkipping(range_end_container)) {
    for (Node* next = Strategy::ChildAt(range_end_container, range_end_offset);
         next; next = Strategy::NextSibling(*next)) {
      if (NotSkipping(*next))
        return next;
    }
  }
  for (const Node* node = &range_end_container; node;) {
    const Node* parent = ParentCrossingShadowBoundaries<Strategy>(*node);
    if (parent && NotSkipping(*parent)) {
      if (Node* next = Strategy::NextSibling(*node))
        return next;
    }
    node = parent;
  }
  return nullptr;
}

// Figure out the initial value of m_shadowDepth: the depth of startContainer's
// tree scope from the common ancestor tree scope.
template <typename Strategy>
int ShadowDepthOf(const Node& start_container, const Node& end_container);

template <>
int ShadowDepthOf<EditingStrategy>(const Node& start_container,
                                   const Node& end_container) {
  const TreeScope* common_ancestor_tree_scope =
      start_container.GetTreeScope().CommonAncestorTreeScope(
          end_container.GetTreeScope());
  DCHECK(common_ancestor_tree_scope);
  int shadow_depth = 0;
  for (const TreeScope* tree_scope = &start_container.GetTreeScope();
       tree_scope != common_ancestor_tree_scope;
       tree_scope = tree_scope->ParentTreeScope())
    ++shadow_depth;
  return shadow_depth;
}

template <>
int ShadowDepthOf<EditingInFlatTreeStrategy>(const Node& start_container,
                                             const Node& end_container) {
  return 0;
}

bool IsRenderedAsTable(const Node* node) {
  if (!node || !node->IsElementNode())
    return false;
  LayoutObject* layout_object = node->GetLayoutObject();
  return layout_object && layout_object->IsTable();
}

}  // namespace

template <typename Strategy>
TextIteratorAlgorithm<Strategy>::TextIteratorAlgorithm(
    const EphemeralRangeTemplate<Strategy>& range,
    const TextIteratorBehavior& behavior)
    : TextIteratorAlgorithm(range.StartPosition(),
                            range.EndPosition(),
                            behavior) {}

template <typename Strategy>
TextIteratorAlgorithm<Strategy>::TextIteratorAlgorithm(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end,
    const TextIteratorBehavior& behavior)
    : start_container_(start.ComputeContainerNode()),
      start_offset_(start.ComputeOffsetInContainerNode()),
      end_container_(end.ComputeContainerNode()),
      end_offset_(end.ComputeOffsetInContainerNode()),
      end_node_(EndNode<Strategy>(*end_container_, end_offset_)),
      past_end_node_(PastLastNode<Strategy>(*end_container_, end_offset_)),
      node_(StartNode<Strategy>(start_container_, start_offset_)),
      iteration_progress_(kHandledNone),
      shadow_depth_(
          ShadowDepthOf<Strategy>(*start_container_, *end_container_)),
      behavior_(AdjustBehaviorFlags<Strategy>(behavior)),
      text_state_(new TextIteratorTextState()),
      text_node_handler_(
          new TextIteratorTextNodeHandler(behavior_, text_state_)) {
  DCHECK(start_container_);
  DCHECK(end_container_);

  // TODO(dglazkov): TextIterator should not be created for documents that don't
  // have a frame, but it currently still happens in some cases. See
  // http://crbug.com/591877 for details.
  DCHECK(!start.GetDocument()->View() ||
         !start.GetDocument()->View()->NeedsLayout());
  DCHECK(!start.GetDocument()->NeedsLayoutTreeUpdate());
  // To avoid renderer hang, we use |CHECK_LE()| to catch the bad callers
  // in release build.
  CHECK_LE(start, end);

  if (!node_)
    return;

  fully_clipped_stack_.SetUpFullyClippedStack(node_);

  // Identify the first run.
  Advance();
}

template <typename Strategy>
TextIteratorAlgorithm<Strategy>::~TextIteratorAlgorithm() {
  if (!handle_shadow_root_)
    return;
  Document* document = OwnerDocument();
  if (!document)
    return;
  if (behavior_.ForInnerText())
    UseCounter::Count(document, WebFeature::kInnerTextWithShadowTree);
  if (behavior_.ForSelectionToString())
    UseCounter::Count(document, WebFeature::kSelectionToStringWithShadowTree);
  if (behavior_.ForWindowFind())
    UseCounter::Count(document, WebFeature::kWindowFindWithShadowTree);
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::IsInsideAtomicInlineElement() const {
  if (AtEnd() || length() != 1 || !node_)
    return false;

  LayoutObject* layout_object = node_->GetLayoutObject();
  return layout_object && layout_object->IsAtomicInlineLevel();
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::HandleRememberedProgress() {
  // Handle remembered node that needed a newline after the text node's newline
  if (needs_another_newline_) {
    // Emit the extra newline, and position it *inside* m_node, after m_node's
    // contents, in case it's a block, in the same way that we position the
    // first newline. The range for the emitted newline should start where the
    // line break begins.
    // FIXME: It would be cleaner if we emitted two newlines during the last
    // iteration, instead of using m_needsAnotherNewline.
    Node* last_child = Strategy::LastChild(*node_);
    Node* base_node = last_child ? last_child : node_.Get();
    SpliceBuffer('\n', Strategy::Parent(*base_node), base_node, 1, 1);
    needs_another_newline_ = false;
    return true;
  }

  if (needs_handle_replaced_element_) {
    HandleReplacedElement();
    if (text_state_->PositionNode())
      return true;
  }

  // Try to emit more text runs if we are handling a text node.
  return text_node_handler_->HandleRemainingTextRuns();
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::Advance() {
  if (should_stop_)
    return;

  if (node_)
    DCHECK(!node_->GetDocument().NeedsLayoutTreeUpdate()) << node_;

  text_state_->ResetRunInformation();

  if (HandleRememberedProgress())
    return;

  while (node_ && (node_ != past_end_node_ || shadow_depth_ > 0)) {
    if (!should_stop_ && StopsOnFormControls() &&
        HTMLFormControlElement::EnclosingFormControlElement(node_))
      should_stop_ = true;

    // if the range ends at offset 0 of an element, represent the
    // position, but not the content, of that element e.g. if the
    // node is a blockflow element, emit a newline that
    // precedes the element
    if (node_ == end_container_ && !end_offset_) {
      RepresentNodeOffsetZero();
      node_ = nullptr;
      return;
    }

    LayoutObject* layout_object = node_->GetLayoutObject();
    if (!layout_object) {
      if (node_->IsShadowRoot() || HasDisplayContents(*node_)) {
        // Shadow roots or display: contents elements don't have LayoutObjects,
        // but we want to visit children anyway.
        iteration_progress_ = iteration_progress_ < kHandledNode
                                  ? kHandledNode
                                  : iteration_progress_;
        handle_shadow_root_ = node_->IsShadowRoot();
      } else {
        iteration_progress_ = kHandledChildren;
      }
    } else {
      // Enter author shadow roots, from youngest, if any and if necessary.
      if (iteration_progress_ < kHandledOpenShadowRoots) {
        if (EntersOpenShadowRoots() && node_->IsElementNode() &&
            ToElement(node_)->openShadowRoot()) {
          ShadowRoot* youngest_shadow_root = ToElement(node_)->openShadowRoot();
          DCHECK(youngest_shadow_root->GetType() == ShadowRootType::V0 ||
                 youngest_shadow_root->GetType() == ShadowRootType::kOpen);
          node_ = youngest_shadow_root;
          iteration_progress_ = kHandledNone;
          ++shadow_depth_;
          fully_clipped_stack_.PushFullyClippedState(node_);
          continue;
        }

        iteration_progress_ = kHandledOpenShadowRoots;
      }

      // Enter user-agent shadow root, if necessary.
      if (iteration_progress_ < kHandledUserAgentShadowRoot) {
        if (EntersTextControls() && layout_object->IsTextControl()) {
          ShadowRoot* user_agent_shadow_root =
              ToElement(node_)->UserAgentShadowRoot();
          DCHECK(user_agent_shadow_root->GetType() ==
                 ShadowRootType::kUserAgent);
          node_ = user_agent_shadow_root;
          iteration_progress_ = kHandledNone;
          ++shadow_depth_;
          fully_clipped_stack_.PushFullyClippedState(node_);
          continue;
        }
        iteration_progress_ = kHandledUserAgentShadowRoot;
      }

      // Handle the current node according to its type.
      if (iteration_progress_ < kHandledNode) {
        if (layout_object->IsText() &&
            node_->getNodeType() ==
                Node::kTextNode) {  // FIXME: What about kCdataSectionNode?
          if (!fully_clipped_stack_.Top() || IgnoresStyleVisibility())
            HandleTextNode();
        } else if (layout_object &&
                   (layout_object->IsImage() ||
                    layout_object->IsLayoutEmbeddedContent() ||
                    (node_ && node_->IsHTMLElement() &&
                     (IsHTMLFormControlElement(ToHTMLElement(*node_)) ||
                      isHTMLLegendElement(ToHTMLElement(*node_)) ||
                      isHTMLImageElement(ToHTMLElement(*node_)) ||
                      isHTMLMeterElement(ToHTMLElement(*node_)) ||
                      isHTMLProgressElement(ToHTMLElement(*node_)))))) {
          HandleReplacedElement();
        } else {
          HandleNonTextNode();
        }
        iteration_progress_ = kHandledNode;
        if (text_state_->PositionNode())
          return;
      }
    }

    // Find a new current node to handle in depth-first manner,
    // calling exitNode() as we come back thru a parent node.
    //
    // 1. Iterate over child nodes, if we haven't done yet.
    // To support |TextIteratorEmitsImageAltText|, we don't traversal child
    // nodes, in flat tree.
    Node* next =
        iteration_progress_ < kHandledChildren && !isHTMLImageElement(*node_)
            ? Strategy::FirstChild(*node_)
            : nullptr;
    if (!next) {
      // 2. If we've already iterated children or they are not available, go to
      // the next sibling node.
      next = Strategy::NextSibling(*node_);
      if (!next) {
        // 3. If we are at the last child, go up the node tree until we find a
        // next sibling.
        ContainerNode* parent_node = Strategy::Parent(*node_);
        while (!next && parent_node) {
          if (node_ == end_node_ ||
              Strategy::IsDescendantOf(*end_container_, *parent_node))
            return;
          bool have_layout_object = node_->GetLayoutObject();
          node_ = parent_node;
          fully_clipped_stack_.Pop();
          parent_node = Strategy::Parent(*node_);
          if (have_layout_object)
            ExitNode();
          if (text_state_->PositionNode()) {
            iteration_progress_ = kHandledChildren;
            return;
          }
          next = Strategy::NextSibling(*node_);
        }

        if (!next && !parent_node && shadow_depth_ > 0) {
          // 4. Reached the top of a shadow root. If it's created by author,
          // then try to visit the next
          // sibling shadow root, if any.
          if (!node_->IsShadowRoot()) {
            NOTREACHED();
            should_stop_ = true;
            return;
          }
          ShadowRoot* shadow_root = ToShadowRoot(node_);
          if (shadow_root->GetType() == ShadowRootType::V0 ||
              shadow_root->GetType() == ShadowRootType::kOpen) {
            ShadowRoot* next_shadow_root = shadow_root->OlderShadowRoot();
            if (next_shadow_root &&
                next_shadow_root->GetType() == ShadowRootType::V0) {
              fully_clipped_stack_.Pop();
              node_ = next_shadow_root;
              iteration_progress_ = kHandledNone;
              // m_shadowDepth is unchanged since we exit from a shadow root and
              // enter another.
              fully_clipped_stack_.PushFullyClippedState(node_);
            } else {
              // We are the last shadow root; exit from here and go back to
              // where we were.
              node_ = &shadow_root->host();
              iteration_progress_ = kHandledOpenShadowRoots;
              --shadow_depth_;
              fully_clipped_stack_.Pop();
            }
          } else {
            // If we are in a closed or user-agent shadow root, then go back to
            // the host.
            // TODO(kochi): Make sure we treat closed shadow as user agent
            // shadow here.
            DCHECK(shadow_root->GetType() == ShadowRootType::kClosed ||
                   shadow_root->GetType() == ShadowRootType::kUserAgent);
            node_ = &shadow_root->host();
            iteration_progress_ = kHandledUserAgentShadowRoot;
            --shadow_depth_;
            fully_clipped_stack_.Pop();
          }
          continue;
        }
      }
      fully_clipped_stack_.Pop();
    }

    // set the new current node
    node_ = next;
    if (node_)
      fully_clipped_stack_.PushFullyClippedState(node_);
    iteration_progress_ = kHandledNone;

    // how would this ever be?
    if (text_state_->PositionNode())
      return;
  }
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::HandleTextNode() {
  if (ExcludesAutofilledValue()) {
    TextControlElement* control = EnclosingTextControl(node_);
    // For security reason, we don't expose suggested value if it is
    // auto-filled.
    if (control && control->IsAutofilled())
      return;
  }

  DCHECK_NE(last_text_node_, node_)
      << "We should never call HandleTextNode on the same node twice";
  Text* text = ToText(node_);
  last_text_node_ = text;

  // TODO(editing-dev): Introduce a |DOMOffsetRange| class so that we can pass
  // an offset range with unbounded endpoint(s) in an easy but still clear way.
  if (node_ != start_container_) {
    if (node_ != end_container_)
      text_node_handler_->HandleTextNodeWhole(text);
    else
      text_node_handler_->HandleTextNodeEndAt(text, end_offset_);
    return;
  }
  if (node_ != end_container_) {
    text_node_handler_->HandleTextNodeStartFrom(text, start_offset_);
    return;
  }
  text_node_handler_->HandleTextNodeInRange(text, start_offset_, end_offset_);
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::SupportsAltText(Node* node) {
  if (!node->IsHTMLElement())
    return false;
  HTMLElement& element = ToHTMLElement(*node);

  // FIXME: Add isSVGImageElement.
  if (isHTMLImageElement(element))
    return true;
  if (isHTMLInputElement(ToHTMLElement(*node)) &&
      toHTMLInputElement(*node).type() == InputTypeNames::image)
    return true;
  return false;
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::HandleReplacedElement() {
  needs_handle_replaced_element_ = false;

  if (fully_clipped_stack_.Top())
    return;

  LayoutObject* layout_object = node_->GetLayoutObject();
  if (layout_object->Style()->Visibility() != EVisibility::kVisible &&
      !IgnoresStyleVisibility()) {
    return;
  }

  if (EmitsObjectReplacementCharacter()) {
    SpliceBuffer(kObjectReplacementCharacter, Strategy::Parent(*node_), node_,
                 0, 1);
    return;
  }

  DCHECK_EQ(last_text_node_, text_node_handler_->GetNode());
  if (last_text_node_) {
    if (text_node_handler_->FixLeadingWhiteSpaceForReplacedElement(
            Strategy::Parent(*last_text_node_))) {
      needs_handle_replaced_element_ = true;
      return;
    }
  }

  if (EntersTextControls() && layout_object->IsTextControl()) {
    // The shadow tree should be already visited.
    return;
  }

  if (EmitsCharactersBetweenAllVisiblePositions()) {
    // We want replaced elements to behave like punctuation for boundary
    // finding, and to simply take up space for the selection preservation
    // code in moveParagraphs, so we use a comma.
    SpliceBuffer(',', Strategy::Parent(*node_), node_, 0, 1);
    return;
  }

  text_state_->UpdateForReplacedElement(node_);

  if (EmitsImageAltText() && TextIterator::SupportsAltText(node_)) {
    text_state_->EmitAltText(node_);
    if (text_state_->length())
      return;
  }
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::ShouldEmitTabBeforeNode(Node* node) {
  LayoutObject* r = node->GetLayoutObject();

  // Table cells are delimited by tabs.
  if (!r || !IsTableCell(node))
    return false;

  // Want a tab before every cell other than the first one
  LayoutTableCell* rc = ToLayoutTableCell(r);
  LayoutTable* t = rc->Table();
  return t && (t->CellPreceding(*rc) || t->CellAbove(*rc));
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::ShouldEmitNewlineForNode(
    Node* node,
    bool emits_original_text) {
  LayoutObject* layout_object = node->GetLayoutObject();

  if (layout_object ? !layout_object->IsBR() : !isHTMLBRElement(node))
    return false;
  return emits_original_text || !(node->IsInShadowTree() &&
                                  isHTMLInputElement(*node->OwnerShadowHost()));
}

static bool ShouldEmitNewlinesBeforeAndAfterNode(Node& node) {
  // Block flow (versus inline flow) is represented by having
  // a newline both before and after the element.
  LayoutObject* r = node.GetLayoutObject();
  if (!r) {
    return (node.HasTagName(blockquoteTag) || node.HasTagName(ddTag) ||
            node.HasTagName(divTag) || node.HasTagName(dlTag) ||
            node.HasTagName(dtTag) || node.HasTagName(h1Tag) ||
            node.HasTagName(h2Tag) || node.HasTagName(h3Tag) ||
            node.HasTagName(h4Tag) || node.HasTagName(h5Tag) ||
            node.HasTagName(h6Tag) || node.HasTagName(hrTag) ||
            node.HasTagName(liTag) || node.HasTagName(listingTag) ||
            node.HasTagName(olTag) || node.HasTagName(pTag) ||
            node.HasTagName(preTag) || node.HasTagName(trTag) ||
            node.HasTagName(ulTag));
  }

  // Need to make an exception for option and optgroup, because we want to
  // keep the legacy behavior before we added layoutObjects to them.
  if (isHTMLOptionElement(node) || isHTMLOptGroupElement(node))
    return false;

  // Need to make an exception for table cells, because they are blocks, but we
  // want them tab-delimited rather than having newlines before and after.
  if (IsTableCell(&node))
    return false;

  // Need to make an exception for table row elements, because they are neither
  // "inline" or "LayoutBlock", but we want newlines for them.
  if (r->IsTableRow()) {
    LayoutTable* t = ToLayoutTableRow(r)->Table();
    if (t && !t->IsInline())
      return true;
  }

  return !r->IsInline() && r->IsLayoutBlock() &&
         !r->IsFloatingOrOutOfFlowPositioned() && !r->IsBody() &&
         !r->IsRubyText();
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::ShouldEmitNewlineAfterNode(Node& node) {
  // FIXME: It should be better but slower to create a VisiblePosition here.
  if (!ShouldEmitNewlinesBeforeAndAfterNode(node))
    return false;
  // Check if this is the very last layoutObject in the document.
  // If so, then we should not emit a newline.
  Node* next = &node;
  do {
    next = Strategy::NextSkippingChildren(*next);
    if (next && next->GetLayoutObject())
      return true;
  } while (next);
  return false;
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::ShouldEmitNewlineBeforeNode(Node& node) {
  return ShouldEmitNewlinesBeforeAndAfterNode(node);
}

static bool ShouldEmitExtraNewlineForNode(Node* node) {
  // When there is a significant collapsed bottom margin, emit an extra
  // newline for a more realistic result. We end up getting the right
  // result even without margin collapsing. For example: <div><p>text</p></div>
  // will work right even if both the <div> and the <p> have bottom margins.
  LayoutObject* r = node->GetLayoutObject();
  if (!r || !r->IsBox())
    return false;

  // NOTE: We only do this for a select set of nodes, and fwiw WinIE appears
  // not to do this at all
  if (node->HasTagName(h1Tag) || node->HasTagName(h2Tag) ||
      node->HasTagName(h3Tag) || node->HasTagName(h4Tag) ||
      node->HasTagName(h5Tag) || node->HasTagName(h6Tag) ||
      node->HasTagName(pTag)) {
    const ComputedStyle* style = r->Style();
    if (style) {
      int bottom_margin = ToLayoutBox(r)->CollapsedMarginAfter().ToInt();
      int font_size = style->GetFontDescription().ComputedPixelSize();
      if (bottom_margin * 2 >= font_size)
        return true;
    }
  }

  return false;
}

// Whether or not we should emit a character as we enter m_node (if it's a
// container) or as we hit it (if it's atomic).
template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::ShouldRepresentNodeOffsetZero() {
  if (EmitsCharactersBetweenAllVisiblePositions() && IsRenderedAsTable(node_))
    return true;

  // Leave element positioned flush with start of a paragraph
  // (e.g. do not insert tab before a table cell at the start of a paragraph)
  if (text_state_->LastCharacter() == '\n')
    return false;

  // Otherwise, show the position if we have emitted any characters
  if (text_state_->HasEmitted())
    return true;

  // We've not emitted anything yet. Generally, there is no need for any
  // positioning then. The only exception is when the element is visually not in
  // the same line as the start of the range (e.g. the range starts at the end
  // of the previous paragraph).
  // NOTE: Creating VisiblePositions and comparing them is relatively expensive,
  // so we make quicker checks to possibly avoid that. Another check that we
  // could make is is whether the inline vs block flow changed since the
  // previous visible element. I think we're already in a special enough case
  // that that won't be needed, tho.

  // No character needed if this is the first node in the range.
  if (node_ == start_container_)
    return false;

  // If we are outside the start container's subtree, assume we need to emit.
  // FIXME: m_startContainer could be an inline block
  if (!Strategy::IsDescendantOf(*node_, *start_container_))
    return true;

  // If we started as m_startContainer offset 0 and the current node is a
  // descendant of the start container, we already had enough context to
  // correctly decide whether to emit after a preceding block. We chose not to
  // emit (m_hasEmitted is false), so don't second guess that now.
  // NOTE: Is this really correct when m_node is not a leftmost descendant?
  // Probably immaterial since we likely would have already emitted something by
  // now.
  if (!start_offset_)
    return false;

  // If this node is unrendered or invisible the VisiblePosition checks below
  // won't have much meaning.
  // Additionally, if the range we are iterating over contains huge sections of
  // unrendered content, we would create VisiblePositions on every call to this
  // function without this check.
  if (!node_->GetLayoutObject() ||
      node_->GetLayoutObject()->Style()->Visibility() !=
          EVisibility::kVisible ||
      (node_->GetLayoutObject()->IsLayoutBlockFlow() &&
       !ToLayoutBlock(node_->GetLayoutObject())->Size().Height() &&
       !isHTMLBodyElement(*node_)))
    return false;

  // The startPos.isNotNull() check is needed because the start could be before
  // the body, and in that case we'll get null. We don't want to put in newlines
  // at the start in that case.
  // The currPos.isNotNull() check is needed because positions in non-HTML
  // content (like SVG) do not have visible positions, and we don't want to emit
  // for them either.
  VisiblePosition start_pos =
      CreateVisiblePosition(Position(start_container_, start_offset_));
  VisiblePosition curr_pos = VisiblePosition::BeforeNode(*node_);
  return start_pos.IsNotNull() && curr_pos.IsNotNull() &&
         !InSameLine(start_pos, curr_pos);
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::ShouldEmitSpaceBeforeAndAfterNode(
    Node* node) {
  return IsRenderedAsTable(node) &&
         (node->GetLayoutObject()->IsInline() ||
          EmitsCharactersBetweenAllVisiblePositions());
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::RepresentNodeOffsetZero() {
  // Emit a character to show the positioning of m_node.

  // When we haven't been emitting any characters,
  // shouldRepresentNodeOffsetZero() can create VisiblePositions, which is
  // expensive. So, we perform the inexpensive checks on m_node to see if it
  // necessitates emitting a character first and will early return before
  // encountering shouldRepresentNodeOffsetZero()s worse case behavior.
  if (ShouldEmitTabBeforeNode(node_)) {
    if (ShouldRepresentNodeOffsetZero())
      SpliceBuffer('\t', Strategy::Parent(*node_), node_, 0, 0);
  } else if (ShouldEmitNewlineBeforeNode(*node_)) {
    if (ShouldRepresentNodeOffsetZero())
      SpliceBuffer('\n', Strategy::Parent(*node_), node_, 0, 0);
  } else if (ShouldEmitSpaceBeforeAndAfterNode(node_)) {
    if (ShouldRepresentNodeOffsetZero())
      SpliceBuffer(kSpaceCharacter, Strategy::Parent(*node_), node_, 0, 0);
  }
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::HandleNonTextNode() {
  if (ShouldEmitNewlineForNode(node_, EmitsOriginalText()))
    SpliceBuffer('\n', Strategy::Parent(*node_), node_, 0, 1);
  else if (EmitsCharactersBetweenAllVisiblePositions() &&
           node_->GetLayoutObject() && node_->GetLayoutObject()->IsHR())
    SpliceBuffer(kSpaceCharacter, Strategy::Parent(*node_), node_, 0, 1);
  else
    RepresentNodeOffsetZero();
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::ExitNode() {
  // prevent emitting a newline when exiting a collapsed block at beginning of
  // the range
  // FIXME: !m_hasEmitted does not necessarily mean there was a collapsed
  // block... it could have been an hr (e.g.). Also, a collapsed block could
  // have height (e.g. a table) and therefore look like a blank line.
  if (!text_state_->HasEmitted())
    return;

  // Emit with a position *inside* m_node, after m_node's contents, in
  // case it is a block, because the run should start where the
  // emitted character is positioned visually.
  Node* last_child = Strategy::LastChild(*node_);
  Node* base_node = last_child ? last_child : node_.Get();
  // FIXME: This shouldn't require the m_lastTextNode to be true, but we can't
  // change that without making the logic in _web_attributedStringFromRange
  // match. We'll get that for free when we switch to use TextIterator in
  // _web_attributedStringFromRange. See <rdar://problem/5428427> for an example
  // of how this mismatch will cause problems.
  if (last_text_node_ && ShouldEmitNewlineAfterNode(*node_)) {
    // use extra newline to represent margin bottom, as needed
    bool add_newline = ShouldEmitExtraNewlineForNode(node_);

    // FIXME: We need to emit a '\n' as we leave an empty block(s) that
    // contain a VisiblePosition when doing selection preservation.
    if (text_state_->LastCharacter() != '\n') {
      // insert a newline with a position following this block's contents.
      SpliceBuffer(kNewlineCharacter, Strategy::Parent(*base_node), base_node,
                   1, 1);
      // remember whether to later add a newline for the current node
      DCHECK(!needs_another_newline_);
      needs_another_newline_ = add_newline;
    } else if (add_newline) {
      // insert a newline with a position following this block's contents.
      SpliceBuffer(kNewlineCharacter, Strategy::Parent(*base_node), base_node,
                   1, 1);
    }
  }

  // If nothing was emitted, see if we need to emit a space.
  if (!text_state_->PositionNode() && ShouldEmitSpaceBeforeAndAfterNode(node_))
    SpliceBuffer(kSpaceCharacter, Strategy::Parent(*base_node), base_node, 1,
                 1);
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::SpliceBuffer(UChar c,
                                                   Node* text_node,
                                                   Node* offset_base_node,
                                                   int text_start_offset,
                                                   int text_end_offset) {
  text_state_->SpliceBuffer(c, text_node, offset_base_node, text_start_offset,
                            text_end_offset);
  text_node_handler_->ResetCollapsedWhiteSpaceFixup();
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy> TextIteratorAlgorithm<Strategy>::Range()
    const {
  // use the current run information, if we have it
  if (text_state_->PositionNode()) {
    return EphemeralRangeTemplate<Strategy>(StartPositionInCurrentContainer(),
                                            EndPositionInCurrentContainer());
  }

  // otherwise, return the end of the overall range we were given
  if (end_container_)
    return EphemeralRangeTemplate<Strategy>(
        PositionTemplate<Strategy>(end_container_, end_offset_));

  return EphemeralRangeTemplate<Strategy>();
}

template <typename Strategy>
Document* TextIteratorAlgorithm<Strategy>::OwnerDocument() const {
  if (text_state_->PositionNode())
    return &text_state_->PositionNode()->GetDocument();
  if (end_container_)
    return &end_container_->GetDocument();
  return 0;
}

template <typename Strategy>
Node* TextIteratorAlgorithm<Strategy>::GetNode() const {
  if (text_state_->PositionNode() || end_container_) {
    Node* node = CurrentContainer();
    if (node->IsCharacterDataNode())
      return node;
    return Strategy::ChildAt(*node, StartOffsetInCurrentContainer());
  }
  return 0;
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::StartOffsetInCurrentContainer() const {
  if (text_state_->PositionNode()) {
    text_state_->FlushPositionOffsets();
    return text_state_->PositionStartOffset();
  }
  DCHECK(end_container_);
  return end_offset_;
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::EndOffsetInCurrentContainer() const {
  if (text_state_->PositionNode()) {
    text_state_->FlushPositionOffsets();
    return text_state_->PositionEndOffset();
  }
  DCHECK(end_container_);
  return end_offset_;
}

template <typename Strategy>
Node* TextIteratorAlgorithm<Strategy>::CurrentContainer() const {
  if (text_state_->PositionNode()) {
    return text_state_->PositionNode();
  }
  DCHECK(end_container_);
  return end_container_;
}

template <typename Strategy>
PositionTemplate<Strategy>
TextIteratorAlgorithm<Strategy>::StartPositionInCurrentContainer() const {
  return PositionTemplate<Strategy>::EditingPositionOf(
      CurrentContainer(), StartOffsetInCurrentContainer());
}

template <typename Strategy>
PositionTemplate<Strategy>
TextIteratorAlgorithm<Strategy>::EndPositionInCurrentContainer() const {
  return PositionTemplate<Strategy>::EditingPositionOf(
      CurrentContainer(), EndOffsetInCurrentContainer());
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::RangeLength(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end,
    const TextIteratorBehavior& behavior) {
  DCHECK(start.GetDocument());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      start.GetDocument()->Lifecycle());

  int length = 0;
  for (TextIteratorAlgorithm<Strategy> it(start, end, behavior); !it.AtEnd();
       it.Advance())
    length += it.length();

  return length;
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::RangeLength(
    const EphemeralRangeTemplate<Strategy>& range,
    const TextIteratorBehavior& behavior) {
  return RangeLength(range.StartPosition(), range.EndPosition(), behavior);
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::IsInTextSecurityMode() const {
  return IsTextSecurityNode(GetNode());
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::IsBetweenSurrogatePair(
    int position) const {
  DCHECK_GE(position, 0);
  return position > 0 && position < length() &&
         U16_IS_LEAD(CharacterAt(position - 1)) &&
         U16_IS_TRAIL(CharacterAt(position));
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::CopyTextTo(ForwardsTextBuffer* output,
                                                int position,
                                                int min_length) const {
  int end = std::min(length(), position + min_length);
  if (IsBetweenSurrogatePair(end))
    ++end;
  int copied_length = end - position;
  CopyCodeUnitsTo(output, position, copied_length);
  return copied_length;
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::CopyTextTo(ForwardsTextBuffer* output,
                                                int position) const {
  return CopyTextTo(output, position, length() - position);
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::CopyCodeUnitsTo(
    ForwardsTextBuffer* output,
    int position,
    int copy_length) const {
  text_state_->AppendTextTo(output, position, copy_length);
}

// --------

template <typename Strategy>
static String CreatePlainText(const EphemeralRangeTemplate<Strategy>& range,
                              const TextIteratorBehavior& behavior) {
  if (range.IsNull())
    return g_empty_string;

  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      range.StartPosition().GetDocument()->Lifecycle());

  TextIteratorAlgorithm<Strategy> it(range.StartPosition(), range.EndPosition(),
                                     behavior);

  if (it.AtEnd())
    return g_empty_string;

  // The initial buffer size can be critical for performance:
  // https://bugs.webkit.org/show_bug.cgi?id=81192
  static const unsigned kInitialCapacity = 1 << 15;

  StringBuilder builder;
  builder.ReserveCapacity(kInitialCapacity);

  for (; !it.AtEnd(); it.Advance())
    it.GetText().AppendTextToStringBuilder(builder);

  if (builder.IsEmpty())
    return g_empty_string;

  return builder.ToString();
}

String PlainText(const EphemeralRange& range,
                 const TextIteratorBehavior& behavior) {
  return CreatePlainText<EditingStrategy>(range, behavior);
}

String PlainText(const EphemeralRangeInFlatTree& range,
                 const TextIteratorBehavior& behavior) {
  return CreatePlainText<EditingInFlatTreeStrategy>(range, behavior);
}

template class CORE_TEMPLATE_EXPORT TextIteratorAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    TextIteratorAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink
