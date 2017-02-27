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

SelectionEditor::SelectionEditor(LocalFrame& frame) : m_frame(frame) {
  clearVisibleSelection();
}

SelectionEditor::~SelectionEditor() {}

void SelectionEditor::assertSelectionValid() const {
#if DCHECK_IS_ON()
  // Since We don't track dom tree version during attribute changes, we can't
  // use it for validity of |m_selection|.
  const_cast<SelectionEditor*>(this)->m_selection.m_domTreeVersion =
      document().domTreeVersion();
#endif
  m_selection.assertValidFor(document());
}

void SelectionEditor::clearVisibleSelection() {
  m_selection = SelectionInDOMTree();
  m_cachedVisibleSelectionInDOMTree = VisibleSelection();
  m_cachedVisibleSelectionInFlatTree = VisibleSelectionInFlatTree();
  m_cacheIsDirty = false;
  if (!shouldAlwaysUseDirectionalSelection())
    return;
  m_selection.m_isDirectional = true;
}

void SelectionEditor::dispose() {
  resetLogicalRange();
  clearDocumentCachedRange();
  clearVisibleSelection();
}

Document& SelectionEditor::document() const {
  DCHECK(lifecycleContext());
  return *lifecycleContext();
}

const VisibleSelection& SelectionEditor::computeVisibleSelectionInDOMTree()
    const {
  DCHECK_EQ(frame()->document(), document());
  DCHECK_EQ(frame(), document().frame());
  updateCachedVisibleSelectionIfNeeded();
  if (m_cachedVisibleSelectionInDOMTree.isNone())
    return m_cachedVisibleSelectionInDOMTree;
  DCHECK_EQ(m_cachedVisibleSelectionInDOMTree.base().document(), document());
  return m_cachedVisibleSelectionInDOMTree;
}

const VisibleSelectionInFlatTree&
SelectionEditor::computeVisibleSelectionInFlatTree() const {
  DCHECK_EQ(frame()->document(), document());
  DCHECK_EQ(frame(), document().frame());
  updateCachedVisibleSelectionIfNeeded();
  if (m_cachedVisibleSelectionInFlatTree.isNone())
    return m_cachedVisibleSelectionInFlatTree;
  DCHECK_EQ(m_cachedVisibleSelectionInFlatTree.base().document(), document());
  return m_cachedVisibleSelectionInFlatTree;
}

const SelectionInDOMTree& SelectionEditor::selectionInDOMTree() const {
  assertSelectionValid();
  return m_selection;
}

bool SelectionEditor::hasEditableStyle() const {
  return computeVisibleSelectionInDOMTree().hasEditableStyle();
}

bool SelectionEditor::isContentEditable() const {
  return computeVisibleSelectionInDOMTree().isContentEditable();
}

bool SelectionEditor::isContentRichlyEditable() const {
  return computeVisibleSelectionInDOMTree().isContentRichlyEditable();
}

void SelectionEditor::markCacheDirty() {
  if (m_cacheIsDirty)
    return;
  m_cachedVisibleSelectionInFlatTree = VisibleSelectionInFlatTree();
  m_cachedVisibleSelectionInDOMTree = VisibleSelection();
  m_cacheIsDirty = true;
}

void SelectionEditor::setSelection(const SelectionInDOMTree& newSelection) {
  newSelection.assertValidFor(document());
  if (m_selection == newSelection)
    return;
  resetLogicalRange();
  clearDocumentCachedRange();
  markCacheDirty();
  m_selection = newSelection;
}

void SelectionEditor::didChangeChildren(const ContainerNode&) {
  markCacheDirty();
  didFinishDOMMutation();
}

void SelectionEditor::didFinishTextChange(const Position& newBase,
                                          const Position& newExtent) {
  if (newBase == m_selection.m_base && newExtent == m_selection.m_extent) {
    didFinishDOMMutation();
    return;
  }
  m_selection.m_base = newBase;
  m_selection.m_extent = newExtent;
  markCacheDirty();
  didFinishDOMMutation();
}

void SelectionEditor::didFinishDOMMutation() {
  assertSelectionValid();
}

void SelectionEditor::documentAttached(Document* document) {
  DCHECK(document);
  DCHECK(!lifecycleContext()) << lifecycleContext();
  m_styleVersion = static_cast<uint64_t>(-1);
  clearVisibleSelection();
  setContext(document);
}

void SelectionEditor::contextDestroyed(Document*) {
  dispose();
  m_styleVersion = static_cast<uint64_t>(-1);
  m_selection = SelectionInDOMTree();
  m_cachedVisibleSelectionInDOMTree = VisibleSelection();
  m_cachedVisibleSelectionInFlatTree = VisibleSelectionInFlatTree();
  m_cacheIsDirty = false;
}

static Position computePositionForChildrenRemoval(const Position& position,
                                                  ContainerNode& container) {
  Node* node = position.computeContainerNode();
  if (container.containsIncludingHostElements(*node))
    return Position::firstPositionInNode(&container);
  return position;
}

void SelectionEditor::nodeChildrenWillBeRemoved(ContainerNode& container) {
  if (m_selection.isNone())
    return;
  const Position oldBase = m_selection.m_base;
  const Position oldExtent = m_selection.m_extent;
  const Position& newBase =
      computePositionForChildrenRemoval(oldBase, container);
  const Position& newExtent =
      computePositionForChildrenRemoval(oldExtent, container);
  if (newBase == oldBase && newExtent == oldExtent)
    return;
  m_selection = SelectionInDOMTree::Builder()
                    .setBaseAndExtent(newBase, newExtent)
                    .build();
  markCacheDirty();
}

void SelectionEditor::nodeWillBeRemoved(Node& nodeToBeRemoved) {
  if (m_selection.isNone())
    return;
  const Position oldBase = m_selection.m_base;
  const Position oldExtent = m_selection.m_extent;
  const Position& newBase =
      computePositionForNodeRemoval(oldBase, nodeToBeRemoved);
  const Position& newExtent =
      computePositionForNodeRemoval(oldExtent, nodeToBeRemoved);
  if (newBase == oldBase && newExtent == oldExtent)
    return;
  m_selection = SelectionInDOMTree::Builder()
                    .setBaseAndExtent(newBase, newExtent)
                    .build();
  markCacheDirty();
}

static Position updatePositionAfterAdoptingTextReplacement(
    const Position& position,
    CharacterData* node,
    unsigned offset,
    unsigned oldLength,
    unsigned newLength) {
  if (position.anchorNode() != node)
    return position;

  if (position.isBeforeAnchor()) {
    return updatePositionAfterAdoptingTextReplacement(
        Position(node, 0), node, offset, oldLength, newLength);
  }
  if (position.isAfterAnchor()) {
    return updatePositionAfterAdoptingTextReplacement(
        Position(node, oldLength), node, offset, oldLength, newLength);
  }

  // See:
  // http://www.w3.org/TR/DOM-Level-2-Traversal-Range/ranges.html#Level-2-Range-Mutation
  DCHECK_GE(position.offsetInContainerNode(), 0);
  unsigned positionOffset =
      static_cast<unsigned>(position.offsetInContainerNode());
  // Replacing text can be viewed as a deletion followed by insertion.
  if (positionOffset >= offset && positionOffset <= offset + oldLength)
    positionOffset = offset;

  // Adjust the offset if the position is after the end of the deleted contents
  // (positionOffset > offset + oldLength) to avoid having a stale offset.
  if (positionOffset > offset + oldLength)
    positionOffset = positionOffset - oldLength + newLength;

  // Due to case folding
  // (http://unicode.org/Public/UCD/latest/ucd/CaseFolding.txt), LayoutText
  // length may be different from Text length.  A correct implementation would
  // translate the LayoutText offset to a Text offset; this is just a safety
  // precaution to avoid offset values that run off the end of the Text.
  if (positionOffset > node->length())
    positionOffset = node->length();

  return Position(node, positionOffset);
}

void SelectionEditor::didUpdateCharacterData(CharacterData* node,
                                             unsigned offset,
                                             unsigned oldLength,
                                             unsigned newLength) {
  // The fragment check is a performance optimization. See
  // http://trac.webkit.org/changeset/30062.
  if (m_selection.isNone() || !node || !node->isConnected()) {
    didFinishDOMMutation();
    return;
  }
  const Position& newBase = updatePositionAfterAdoptingTextReplacement(
      m_selection.m_base, node, offset, oldLength, newLength);
  const Position& newExtent = updatePositionAfterAdoptingTextReplacement(
      m_selection.m_extent, node, offset, oldLength, newLength);
  didFinishTextChange(newBase, newExtent);
}

// TODO(yosin): We should introduce |Position(const Text&, int)| to avoid
// |const_cast<Text*>|.
static Position updatePostionAfterAdoptingTextNodesMerged(
    const Position& position,
    const Text& mergedNode,
    const NodeWithIndex& nodeToBeRemovedWithIndex,
    unsigned oldLength) {
  Node* const anchorNode = position.anchorNode();
  const Node& nodeToBeRemoved = nodeToBeRemovedWithIndex.node();
  switch (position.anchorType()) {
    case PositionAnchorType::BeforeChildren:
    case PositionAnchorType::AfterChildren:
      return position;
    case PositionAnchorType::BeforeAnchor:
      if (anchorNode == nodeToBeRemoved)
        return Position(const_cast<Text*>(&mergedNode), mergedNode.length());
      return position;
    case PositionAnchorType::AfterAnchor:
      if (anchorNode == nodeToBeRemoved)
        return Position(const_cast<Text*>(&mergedNode), mergedNode.length());
      if (anchorNode == mergedNode)
        return Position(const_cast<Text*>(&mergedNode), oldLength);
      return position;
    case PositionAnchorType::OffsetInAnchor: {
      const int offset = position.offsetInContainerNode();
      if (anchorNode == nodeToBeRemoved)
        return Position(const_cast<Text*>(&mergedNode), oldLength + offset);
      if (anchorNode == nodeToBeRemoved.parentNode() &&
          offset == nodeToBeRemovedWithIndex.index()) {
        return Position(const_cast<Text*>(&mergedNode), oldLength);
      }
      return position;
    }
  }
  NOTREACHED() << position;
  return position;
}

void SelectionEditor::didMergeTextNodes(
    const Text& mergedNode,
    const NodeWithIndex& nodeToBeRemovedWithIndex,
    unsigned oldLength) {
  if (m_selection.isNone()) {
    didFinishDOMMutation();
    return;
  }
  const Position& newBase = updatePostionAfterAdoptingTextNodesMerged(
      m_selection.m_base, mergedNode, nodeToBeRemovedWithIndex, oldLength);
  const Position& newExtent = updatePostionAfterAdoptingTextNodesMerged(
      m_selection.m_extent, mergedNode, nodeToBeRemovedWithIndex, oldLength);
  didFinishTextChange(newBase, newExtent);
}

static Position updatePostionAfterAdoptingTextNodeSplit(
    const Position& position,
    const Text& oldNode) {
  if (!position.anchorNode() || position.anchorNode() != &oldNode ||
      !position.isOffsetInAnchor())
    return position;
  // See:
  // http://www.w3.org/TR/DOM-Level-2-Traversal-Range/ranges.html#Level-2-Range-Mutation
  DCHECK_GE(position.offsetInContainerNode(), 0);
  unsigned positionOffset =
      static_cast<unsigned>(position.offsetInContainerNode());
  unsigned oldLength = oldNode.length();
  if (positionOffset <= oldLength)
    return position;
  return Position(toText(oldNode.nextSibling()), positionOffset - oldLength);
}

void SelectionEditor::didSplitTextNode(const Text& oldNode) {
  if (m_selection.isNone() || !oldNode.isConnected()) {
    didFinishDOMMutation();
    return;
  }
  const Position& newBase =
      updatePostionAfterAdoptingTextNodeSplit(m_selection.m_base, oldNode);
  const Position& newExtent =
      updatePostionAfterAdoptingTextNodeSplit(m_selection.m_extent, oldNode);
  didFinishTextChange(newBase, newExtent);
}

void SelectionEditor::resetLogicalRange() {
  // Non-collapsed ranges are not allowed to start at the end of a line that
  // is wrapped, they start at the beginning of the next line instead
  if (!m_logicalRange)
    return;
  m_logicalRange->dispose();
  m_logicalRange = nullptr;
}

void SelectionEditor::setLogicalRange(Range* range) {
  DCHECK_EQ(range->ownerDocument(), document());
  DCHECK(!m_logicalRange) << "A logical range should be one.";
  m_logicalRange = range;
}

Range* SelectionEditor::firstRange() const {
  if (m_logicalRange)
    return m_logicalRange->cloneRange();
  return firstRangeOf(computeVisibleSelectionInDOMTree());
}

bool SelectionEditor::shouldAlwaysUseDirectionalSelection() const {
  return frame()->editor().behavior().shouldConsiderSelectionAsDirectional();
}

bool SelectionEditor::needsUpdateVisibleSelection() const {
  return m_cacheIsDirty || m_styleVersion != document().styleVersion();
}

void SelectionEditor::updateCachedVisibleSelectionIfNeeded() const {
  // Note: Since we |FrameCaret::updateApperance()| is called from
  // |FrameView::performPostLayoutTasks()|, we check lifecycle against
  // |AfterPerformLayout| instead of |LayoutClean|.
  DCHECK_GE(document().lifecycle().state(),
            DocumentLifecycle::AfterPerformLayout);
  assertSelectionValid();
  if (!needsUpdateVisibleSelection())
    return;

  m_cachedVisibleSelectionInDOMTree = createVisibleSelection(m_selection);
  m_cachedVisibleSelectionInFlatTree = createVisibleSelection(
      SelectionInFlatTree::Builder()
          .setBaseAndExtent(toPositionInFlatTree(m_selection.base()),
                            toPositionInFlatTree(m_selection.extent()))
          .setAffinity(m_selection.affinity())
          .setHasTrailingWhitespace(m_selection.hasTrailingWhitespace())
          .setGranularity(m_selection.granularity())
          .setIsDirectional(m_selection.isDirectional())
          .build());
  m_styleVersion = document().styleVersion();
  m_cacheIsDirty = false;
}

void SelectionEditor::cacheRangeOfDocument(Range* range) {
  m_cachedRange = range;
}

Range* SelectionEditor::documentCachedRange() const {
  return m_cachedRange;
}

void SelectionEditor::clearDocumentCachedRange() {
  m_cachedRange = nullptr;
}

DEFINE_TRACE(SelectionEditor) {
  visitor->trace(m_frame);
  visitor->trace(m_selection);
  visitor->trace(m_cachedVisibleSelectionInDOMTree);
  visitor->trace(m_cachedVisibleSelectionInFlatTree);
  visitor->trace(m_logicalRange);
  visitor->trace(m_cachedRange);
  SynchronousMutationObserver::trace(visitor);
}

}  // namespace blink
