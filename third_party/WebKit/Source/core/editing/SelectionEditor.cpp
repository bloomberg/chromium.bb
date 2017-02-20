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

template <>
const VisibleSelection& SelectionEditor::visibleSelection<EditingStrategy>()
    const {
  return computeVisibleSelectionInDOMTree();
}

template <>
const VisibleSelectionInFlatTree&
SelectionEditor::visibleSelection<EditingInFlatTreeStrategy>() const {
  return computeVisibleSelectionInFlatTree();
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

void SelectionEditor::updateIfNeeded() {
  // TODO(yosin): We should unify |SelectionEditor::updateIfNeeded()| and
  // |updateCachedVisibleSelectionIfNeeded()|
  updateCachedVisibleSelectionIfNeeded();
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
