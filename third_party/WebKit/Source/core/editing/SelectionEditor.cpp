/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "core/editing/SelectionEditor.h"

#include "core/dom/NodeWithIndex.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/SelectionAdjuster.h"
#include "core/frame/LocalFrame.h"

namespace blink {

SelectionEditor::SelectionEditor(LocalFrame& frame) : frame_(frame) {
  ClearVisibleSelection();
}

SelectionEditor::~SelectionEditor() {}

void SelectionEditor::AssertSelectionValid() const {
#if DCHECK_IS_ON()
  // Since We don't track dom tree version during attribute changes, we can't
  // use it for validity of |m_selection|.
  const_cast<SelectionEditor*>(this)->selection_.dom_tree_version_ =
      GetDocument().DomTreeVersion();
#endif
  selection_.AssertValidFor(GetDocument());
}

void SelectionEditor::ClearVisibleSelection() {
  selection_ = SelectionInDOMTree();
  cached_visible_selection_in_dom_tree_ = VisibleSelection();
  cached_visible_selection_in_flat_tree_ = VisibleSelectionInFlatTree();
  cached_visible_selection_in_dom_tree_is_dirty_ = false;
  cached_visible_selection_in_flat_tree_is_dirty_ = false;
  if (!ShouldAlwaysUseDirectionalSelection())
    return;
  selection_.is_directional_ = true;
}

void SelectionEditor::Dispose() {
  ClearDocumentCachedRange();
  ClearVisibleSelection();
}

Document& SelectionEditor::GetDocument() const {
  DCHECK(LifecycleContext());
  return *LifecycleContext();
}

VisibleSelection SelectionEditor::ComputeVisibleSelectionInDOMTree() const {
  DCHECK_EQ(GetFrame()->GetDocument(), GetDocument());
  DCHECK_EQ(GetFrame(), GetDocument().GetFrame());
  UpdateCachedVisibleSelectionIfNeeded();
  if (cached_visible_selection_in_dom_tree_.IsNone())
    return cached_visible_selection_in_dom_tree_;
  DCHECK_EQ(cached_visible_selection_in_dom_tree_.Base().GetDocument(),
            GetDocument());
  return cached_visible_selection_in_dom_tree_;
}

VisibleSelectionInFlatTree SelectionEditor::ComputeVisibleSelectionInFlatTree()
    const {
  DCHECK_EQ(GetFrame()->GetDocument(), GetDocument());
  DCHECK_EQ(GetFrame(), GetDocument().GetFrame());
  UpdateCachedVisibleSelectionInFlatTreeIfNeeded();
  if (cached_visible_selection_in_flat_tree_.IsNone())
    return cached_visible_selection_in_flat_tree_;
  DCHECK_EQ(cached_visible_selection_in_flat_tree_.Base().GetDocument(),
            GetDocument());
  return cached_visible_selection_in_flat_tree_;
}

SelectionInDOMTree SelectionEditor::GetSelectionInDOMTree() const {
  AssertSelectionValid();
  return selection_;
}

void SelectionEditor::MarkCacheDirty() {
  if (!cached_visible_selection_in_dom_tree_is_dirty_) {
    cached_visible_selection_in_dom_tree_ = VisibleSelection();
    cached_visible_selection_in_dom_tree_is_dirty_ = true;
  }
  if (!cached_visible_selection_in_flat_tree_is_dirty_) {
    cached_visible_selection_in_flat_tree_ = VisibleSelectionInFlatTree();
    cached_visible_selection_in_flat_tree_is_dirty_ = true;
  }
}

void SelectionEditor::SetSelection(const SelectionInDOMTree& new_selection) {
  new_selection.AssertValidFor(GetDocument());
  DCHECK_NE(selection_, new_selection);
  ClearDocumentCachedRange();
  MarkCacheDirty();
  selection_ = new_selection;
}

void SelectionEditor::DidChangeChildren(const ContainerNode&) {
  selection_.ResetDirectionCache();
  MarkCacheDirty();
  DidFinishDOMMutation();
}

void SelectionEditor::DidFinishTextChange(const Position& new_base,
                                          const Position& new_extent) {
  if (new_base == selection_.base_ && new_extent == selection_.extent_) {
    DidFinishDOMMutation();
    return;
  }
  selection_.base_ = new_base;
  selection_.extent_ = new_extent;
  selection_.ResetDirectionCache();
  MarkCacheDirty();
  DidFinishDOMMutation();
}

void SelectionEditor::DidFinishDOMMutation() {
  AssertSelectionValid();
}

void SelectionEditor::DocumentAttached(Document* document) {
  DCHECK(document);
  DCHECK(!LifecycleContext()) << LifecycleContext();
  style_version_for_dom_tree_ = static_cast<uint64_t>(-1);
  style_version_for_flat_tree_ = static_cast<uint64_t>(-1);
  ClearVisibleSelection();
  SetContext(document);
}

void SelectionEditor::ContextDestroyed(Document*) {
  Dispose();
  style_version_for_dom_tree_ = static_cast<uint64_t>(-1);
  style_version_for_flat_tree_ = static_cast<uint64_t>(-1);
  selection_ = SelectionInDOMTree();
  cached_visible_selection_in_dom_tree_ = VisibleSelection();
  cached_visible_selection_in_flat_tree_ = VisibleSelectionInFlatTree();
  cached_visible_selection_in_dom_tree_is_dirty_ = false;
  cached_visible_selection_in_flat_tree_is_dirty_ = false;
}

static Position ComputePositionForChildrenRemoval(const Position& position,
                                                  ContainerNode& container) {
  Node* node = position.ComputeContainerNode();
  if (container.ContainsIncludingHostElements(*node))
    return Position::FirstPositionInNode(container);
  return position;
}

void SelectionEditor::NodeChildrenWillBeRemoved(ContainerNode& container) {
  if (selection_.IsNone())
    return;
  const Position old_base = selection_.base_;
  const Position old_extent = selection_.extent_;
  const Position& new_base =
      ComputePositionForChildrenRemoval(old_base, container);
  const Position& new_extent =
      ComputePositionForChildrenRemoval(old_extent, container);
  if (new_base == old_base && new_extent == old_extent)
    return;
  selection_ = SelectionInDOMTree::Builder()
                   .SetBaseAndExtent(new_base, new_extent)
                   .Build();
  MarkCacheDirty();
}

void SelectionEditor::NodeWillBeRemoved(Node& node_to_be_removed) {
  if (selection_.IsNone())
    return;
  const Position old_base = selection_.base_;
  const Position old_extent = selection_.extent_;
  const Position& new_base =
      ComputePositionForNodeRemoval(old_base, node_to_be_removed);
  const Position& new_extent =
      ComputePositionForNodeRemoval(old_extent, node_to_be_removed);
  if (new_base == old_base && new_extent == old_extent)
    return;
  selection_ = SelectionInDOMTree::Builder()
                   .SetBaseAndExtent(new_base, new_extent)
                   .Build();
  MarkCacheDirty();
}

static Position UpdatePositionAfterAdoptingTextReplacement(
    const Position& position,
    CharacterData* node,
    unsigned offset,
    unsigned old_length,
    unsigned new_length) {
  if (position.AnchorNode() != node)
    return position;

  if (position.IsBeforeAnchor()) {
    return UpdatePositionAfterAdoptingTextReplacement(
        Position(node, 0), node, offset, old_length, new_length);
  }
  if (position.IsAfterAnchor()) {
    return UpdatePositionAfterAdoptingTextReplacement(
        Position(node, old_length), node, offset, old_length, new_length);
  }

  // See:
  // http://www.w3.org/TR/DOM-Level-2-Traversal-Range/ranges.html#Level-2-Range-Mutation
  DCHECK_GE(position.OffsetInContainerNode(), 0);
  unsigned position_offset =
      static_cast<unsigned>(position.OffsetInContainerNode());
  // Replacing text can be viewed as a deletion followed by insertion.
  if (position_offset >= offset && position_offset <= offset + old_length)
    position_offset = offset;

  // Adjust the offset if the position is after the end of the deleted contents
  // (positionOffset > offset + oldLength) to avoid having a stale offset.
  if (position_offset > offset + old_length)
    position_offset = position_offset - old_length + new_length;

  // Due to case folding
  // (http://unicode.org/Public/UCD/latest/ucd/CaseFolding.txt), LayoutText
  // length may be different from Text length.  A correct implementation would
  // translate the LayoutText offset to a Text offset; this is just a safety
  // precaution to avoid offset values that run off the end of the Text.
  if (position_offset > node->length())
    position_offset = node->length();

  return Position(node, position_offset);
}

void SelectionEditor::DidUpdateCharacterData(CharacterData* node,
                                             unsigned offset,
                                             unsigned old_length,
                                             unsigned new_length) {
  // The fragment check is a performance optimization. See
  // http://trac.webkit.org/changeset/30062.
  if (selection_.IsNone() || !node || !node->isConnected()) {
    DidFinishDOMMutation();
    return;
  }
  const Position& new_base = UpdatePositionAfterAdoptingTextReplacement(
      selection_.base_, node, offset, old_length, new_length);
  const Position& new_extent = UpdatePositionAfterAdoptingTextReplacement(
      selection_.extent_, node, offset, old_length, new_length);
  DidFinishTextChange(new_base, new_extent);
}

static Position UpdatePostionAfterAdoptingTextNodesMerged(
    const Position& position,
    const Text& merged_node,
    const NodeWithIndex& node_to_be_removed_with_index,
    unsigned old_length) {
  Node* const anchor_node = position.AnchorNode();
  const Node& node_to_be_removed = node_to_be_removed_with_index.GetNode();
  switch (position.AnchorType()) {
    case PositionAnchorType::kBeforeChildren:
    case PositionAnchorType::kAfterChildren:
      return position;
    case PositionAnchorType::kBeforeAnchor:
      if (anchor_node == node_to_be_removed)
        return Position(merged_node, merged_node.length());
      return position;
    case PositionAnchorType::kAfterAnchor:
      if (anchor_node == node_to_be_removed)
        return Position(merged_node, merged_node.length());
      if (anchor_node == merged_node)
        return Position(merged_node, old_length);
      return position;
    case PositionAnchorType::kOffsetInAnchor: {
      const int offset = position.OffsetInContainerNode();
      if (anchor_node == node_to_be_removed)
        return Position(merged_node, old_length + offset);
      if (anchor_node == node_to_be_removed.parentNode() &&
          offset == node_to_be_removed_with_index.Index()) {
        return Position(merged_node, old_length);
      }
      return position;
    }
  }
  NOTREACHED() << position;
  return position;
}

void SelectionEditor::DidMergeTextNodes(
    const Text& merged_node,
    const NodeWithIndex& node_to_be_removed_with_index,
    unsigned old_length) {
  if (selection_.IsNone()) {
    DidFinishDOMMutation();
    return;
  }
  const Position& new_base = UpdatePostionAfterAdoptingTextNodesMerged(
      selection_.base_, merged_node, node_to_be_removed_with_index, old_length);
  const Position& new_extent = UpdatePostionAfterAdoptingTextNodesMerged(
      selection_.extent_, merged_node, node_to_be_removed_with_index,
      old_length);
  DidFinishTextChange(new_base, new_extent);
}

static Position UpdatePostionAfterAdoptingTextNodeSplit(
    const Position& position,
    const Text& old_node) {
  if (!position.AnchorNode() || position.AnchorNode() != &old_node ||
      !position.IsOffsetInAnchor())
    return position;
  // See:
  // http://www.w3.org/TR/DOM-Level-2-Traversal-Range/ranges.html#Level-2-Range-Mutation
  DCHECK_GE(position.OffsetInContainerNode(), 0);
  unsigned position_offset =
      static_cast<unsigned>(position.OffsetInContainerNode());
  unsigned old_length = old_node.length();
  if (position_offset <= old_length)
    return position;
  return Position(ToText(old_node.nextSibling()), position_offset - old_length);
}

void SelectionEditor::DidSplitTextNode(const Text& old_node) {
  if (selection_.IsNone() || !old_node.isConnected()) {
    DidFinishDOMMutation();
    return;
  }
  const Position& new_base =
      UpdatePostionAfterAdoptingTextNodeSplit(selection_.base_, old_node);
  const Position& new_extent =
      UpdatePostionAfterAdoptingTextNodeSplit(selection_.extent_, old_node);
  DidFinishTextChange(new_base, new_extent);
}

bool SelectionEditor::ShouldAlwaysUseDirectionalSelection() const {
  return GetFrame()
      ->GetEditor()
      .Behavior()
      .ShouldConsiderSelectionAsDirectional();
}

bool SelectionEditor::NeedsUpdateVisibleSelection() const {
  return cached_visible_selection_in_dom_tree_is_dirty_ ||
         style_version_for_dom_tree_ != GetDocument().StyleVersion();
}

void SelectionEditor::UpdateCachedVisibleSelectionIfNeeded() const {
  // Note: Since we |FrameCaret::updateApperance()| is called from
  // |FrameView::performPostLayoutTasks()|, we check lifecycle against
  // |AfterPerformLayout| instead of |LayoutClean|.
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kAfterPerformLayout);
  AssertSelectionValid();
  if (!NeedsUpdateVisibleSelection())
    return;
  style_version_for_dom_tree_ = GetDocument().StyleVersion();
  cached_visible_selection_in_dom_tree_is_dirty_ = false;
  cached_visible_selection_in_dom_tree_ = CreateVisibleSelection(selection_);
  if (!cached_visible_selection_in_dom_tree_.IsNone())
    return;
  style_version_for_flat_tree_ = GetDocument().StyleVersion();
  cached_visible_selection_in_flat_tree_is_dirty_ = false;
  cached_visible_selection_in_flat_tree_ = VisibleSelectionInFlatTree();
}

bool SelectionEditor::NeedsUpdateVisibleSelectionInFlatTree() const {
  return cached_visible_selection_in_flat_tree_is_dirty_ ||
         style_version_for_flat_tree_ != GetDocument().StyleVersion();
}

void SelectionEditor::UpdateCachedVisibleSelectionInFlatTreeIfNeeded() const {
  // Note: Since we |FrameCaret::updateApperance()| is called from
  // |FrameView::performPostLayoutTasks()|, we check lifecycle against
  // |AfterPerformLayout| instead of |LayoutClean|.
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kAfterPerformLayout);
  AssertSelectionValid();
  if (!NeedsUpdateVisibleSelectionInFlatTree())
    return;
  style_version_for_flat_tree_ = GetDocument().StyleVersion();
  cached_visible_selection_in_flat_tree_is_dirty_ = false;
  SelectionInFlatTree::Builder builder;
  const PositionInFlatTree& base = ToPositionInFlatTree(selection_.Base());
  const PositionInFlatTree& extent = ToPositionInFlatTree(selection_.Extent());
  if (base.IsNotNull() && extent.IsNotNull())
    builder.SetBaseAndExtent(base, extent);
  else if (base.IsNotNull())
    builder.Collapse(base);
  else if (extent.IsNotNull())
    builder.Collapse(extent);
  builder.SetAffinity(selection_.Affinity())
      .SetIsDirectional(selection_.IsDirectional());
  cached_visible_selection_in_flat_tree_ =
      CreateVisibleSelection(builder.Build());
  if (!cached_visible_selection_in_flat_tree_.IsNone())
    return;
  style_version_for_dom_tree_ = GetDocument().StyleVersion();
  cached_visible_selection_in_dom_tree_is_dirty_ = false;
  cached_visible_selection_in_dom_tree_ = VisibleSelection();
}

void SelectionEditor::CacheRangeOfDocument(Range* range) {
  cached_range_ = range;
}

Range* SelectionEditor::DocumentCachedRange() const {
  return cached_range_;
}

void SelectionEditor::ClearDocumentCachedRange() {
  cached_range_ = nullptr;
}

DEFINE_TRACE(SelectionEditor) {
  visitor->Trace(frame_);
  visitor->Trace(selection_);
  visitor->Trace(cached_visible_selection_in_dom_tree_);
  visitor->Trace(cached_visible_selection_in_flat_tree_);
  visitor->Trace(cached_range_);
  SynchronousMutationObserver::Trace(visitor);
}

}  // namespace blink
