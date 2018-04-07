// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"

namespace blink {

WhitespaceAttacher::~WhitespaceAttacher() {
  if (last_text_node_ && last_text_node_needs_reattach_)
    ReattachWhitespaceSiblings(nullptr);
}

void WhitespaceAttacher::DidReattach(Node* node, LayoutObject* prev_in_flow) {
  DCHECK(node);
  DCHECK(node->IsTextNode() || node->IsElementNode());
  // See Invariants in WhitespaceAttacher.h
  DCHECK(!last_display_contents_ || !last_text_node_needs_reattach_);

  ForceLastTextNodeNeedsReattach();

  // No subsequent text nodes affected.
  if (!last_text_node_)
    return;

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    layout_object = prev_in_flow;

  // Only in-flow boxes affect subsequent whitespace.
  if (layout_object && !layout_object->IsFloatingOrOutOfFlowPositioned())
    ReattachWhitespaceSiblings(layout_object);
}

void WhitespaceAttacher::DidReattachText(Text* text) {
  DCHECK(text);
  DidReattach(text, text->GetLayoutObject());
  SetLastTextNode(text);
  if (!text->GetLayoutObject())
    last_text_node_needs_reattach_ = true;
}

void WhitespaceAttacher::DidReattachElement(Element* element,
                                            LayoutObject* prev_in_flow) {
  DCHECK(element);
  DidReattach(element, prev_in_flow);
}

void WhitespaceAttacher::DidVisitText(Text* text) {
  DCHECK(text);
  if (!last_text_node_ || !last_text_node_needs_reattach_) {
    SetLastTextNode(text);
    if (reattach_all_whitespace_nodes_ && text->ContainsOnlyWhitespace())
      last_text_node_needs_reattach_ = true;
    return;
  }
  // At this point we have a last_text_node_ which needs re-attachment.
  // If last_text_node_needs_reattach_ is true, we traverse into
  // display:contents elements to find the first preceding in-flow sibling, at
  // which point we do the re-attachment (covered by LastTextNodeNeedsReattach()
  // check in Element::NeedsRebuildLayoutTree()). DidVisitElement() below
  // returns early for display:contents when last_text_node_needs_reattach_ is
  // non-null.
  DCHECK(!last_display_contents_);
  if (LayoutObject* text_layout_object = text->GetLayoutObject()) {
    ReattachWhitespaceSiblings(text_layout_object);
  } else {
    if (last_text_node_->ContainsOnlyWhitespace())
      last_text_node_->ReattachLayoutTreeIfNeeded(Node::AttachContext());
  }
  SetLastTextNode(text);
  if (reattach_all_whitespace_nodes_ && text->ContainsOnlyWhitespace())
    last_text_node_needs_reattach_ = true;
}

void WhitespaceAttacher::DidVisitElement(Element* element) {
  DCHECK(element);
  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object) {
    // Don't set last_display_contents_ when we have a text node which needs to
    // be re-attached. See the comments in DidVisitText() above.
    if (last_text_node_needs_reattach_)
      return;
    if (element->HasDisplayContentsStyle())
      last_display_contents_ = element;
    return;
  }
  if (!last_text_node_ || !last_text_node_needs_reattach_) {
    SetLastTextNode(nullptr);
    return;
  }
  if (layout_object->IsFloatingOrOutOfFlowPositioned())
    return;
  ReattachWhitespaceSiblings(layout_object);
}

void WhitespaceAttacher::ReattachWhitespaceSiblings(
    LayoutObject* previous_in_flow) {
  DCHECK(!last_display_contents_);
  DCHECK(last_text_node_);
  DCHECK(last_text_node_needs_reattach_);
  ScriptForbiddenScope forbid_script;

  Node::AttachContext context;
  context.previous_in_flow = previous_in_flow;
  context.use_previous_in_flow = true;

  for (Node* sibling = last_text_node_; sibling;
       sibling = LayoutTreeBuilderTraversal::NextLayoutSibling(*sibling)) {
    LayoutObject* sibling_layout_object = sibling->GetLayoutObject();
    if (sibling->IsTextNode() && ToText(sibling)->ContainsOnlyWhitespace()) {
      bool had_layout_object = !!sibling_layout_object;
      ToText(sibling)->ReattachLayoutTreeIfNeeded(context);
      sibling_layout_object = sibling->GetLayoutObject();
      // If sibling's layout object status didn't change we don't need to
      // continue checking other siblings since their layout object status
      // won't change either.
      if (!!sibling_layout_object == had_layout_object)
        break;
      if (sibling_layout_object)
        context.previous_in_flow = sibling_layout_object;
    } else if (sibling_layout_object &&
               !sibling_layout_object->IsFloatingOrOutOfFlowPositioned()) {
      break;
    }
  }
  SetLastTextNode(nullptr);
}

void WhitespaceAttacher::ForceLastTextNodeNeedsReattach() {
  // If an element got re-attached, the need for a subsequent whitespace node
  // LayoutObject may have changed. Make sure we try a re-attach when we
  // encounter the next in-flow.
  if (last_text_node_needs_reattach_)
    return;
  if (last_display_contents_)
    UpdateLastTextNodeFromDisplayContents();
  if (last_text_node_)
    last_text_node_needs_reattach_ = true;
}

void WhitespaceAttacher::UpdateLastTextNodeFromDisplayContents() {
  DCHECK(last_display_contents_);
  DCHECK(last_display_contents_->HasDisplayContentsStyle());
  Element* contents_element = last_display_contents_.Release();
  Node* sibling =
      LayoutTreeBuilderTraversal::FirstLayoutChild(*contents_element);

  if (!sibling)
    sibling = LayoutTreeBuilderTraversal::NextLayoutSibling(*contents_element);

  if (!sibling) {
    DCHECK(!last_text_node_);
    return;
  }

  DCHECK(!sibling->IsElementNode() ||
         !ToElement(sibling)->HasDisplayContentsStyle());

  for (; sibling && sibling != last_text_node_;
       sibling = LayoutTreeBuilderTraversal::NextLayoutSibling(*sibling)) {
    LayoutObject* layout_object = sibling->GetLayoutObject();
    if (sibling->IsTextNode()) {
      Text* text = ToText(sibling);
      if (text->ContainsOnlyWhitespace()) {
        last_text_node_ = text;
        return;
      }
    }
    if (layout_object && !layout_object->IsFloatingOrOutOfFlowPositioned()) {
      last_text_node_ = nullptr;
      break;
    }
  }
}

}  // namespace blink
