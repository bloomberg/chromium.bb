/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/DOMSelection.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Node.h"
#include "core/dom/Range.h"
#include "core/dom/TreeScope.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "wtf/text/WTFString.h"

namespace blink {

static Node* selectionShadowAncestor(LocalFrame* frame) {
  Node* node = frame->selection()
                   .computeVisibleSelectionInDOMTreeDeprecated()
                   .base()
                   .anchorNode();
  if (!node)
    return 0;

  if (!node->isInShadowTree())
    return 0;

  return frame->document()->ancestorInThisScope(node);
}

DOMSelection::DOMSelection(const TreeScope* treeScope)
    : ContextClient(treeScope->rootNode().document().frame()),
      m_treeScope(treeScope) {}

void DOMSelection::clearTreeScope() {
  m_treeScope = nullptr;
}

// TODO(editing-dev): The behavior after loosing browsing context is not
// specified. https://github.com/w3c/selection-api/issues/82
bool DOMSelection::isAvailable() const {
  return frame() && frame()->selection().isAvailable();
}

void DOMSelection::updateFrameSelection(const SelectionInDOMTree& selection,
                                        Range* newCachedRange) const {
  DCHECK(frame());
  FrameSelection& frameSelection = frame()->selection();
  // TODO(tkent): Specify FrameSelection::DoNotSetFocus. crbug.com/690272
  bool didSet = frameSelection.setSelectionDeprecated(selection);
  cacheRangeIfSelectionOfDocument(newCachedRange);
  if (!didSet)
    return;
  Element* focusedElement = frame()->document()->focusedElement();
  frameSelection.didSetSelectionDeprecated();
  if (frame() && frame()->document() &&
      focusedElement != frame()->document()->focusedElement())
    UseCounter::count(frame(), UseCounter::SelectionFuncionsChangeFocus);
}

const VisibleSelection& DOMSelection::visibleSelection() const {
  DCHECK(frame());
  return frame()->selection().computeVisibleSelectionInDOMTreeDeprecated();
}

bool DOMSelection::isBaseFirstInSelection() const {
  DCHECK(frame());
  const SelectionInDOMTree& selection =
      frame()->selection().selectionInDOMTree();
  return selection.base() <= selection.extent();
}

// TODO(tkent): Following four functions based on VisibleSelection should be
// removed.
static Position anchorPosition(const VisibleSelection& selection) {
  Position anchor =
      selection.isBaseFirst() ? selection.start() : selection.end();
  return anchor.parentAnchoredEquivalent();
}

static Position focusPosition(const VisibleSelection& selection) {
  Position focus =
      selection.isBaseFirst() ? selection.end() : selection.start();
  return focus.parentAnchoredEquivalent();
}

static Position basePosition(const VisibleSelection& selection) {
  return selection.base().parentAnchoredEquivalent();
}

static Position extentPosition(const VisibleSelection& selection) {
  return selection.extent().parentAnchoredEquivalent();
}

Node* DOMSelection::anchorNode() const {
  if (Range* range = primaryRangeOrNull()) {
    if (!frame() || isBaseFirstInSelection())
      return range->startContainer();
    return range->endContainer();
  }
  return nullptr;
}

unsigned DOMSelection::anchorOffset() const {
  if (Range* range = primaryRangeOrNull()) {
    if (!frame() || isBaseFirstInSelection())
      return range->startOffset();
    return range->endOffset();
  }
  return 0;
}

Node* DOMSelection::focusNode() const {
  if (Range* range = primaryRangeOrNull()) {
    if (!frame() || isBaseFirstInSelection())
      return range->endContainer();
    return range->startContainer();
  }
  return nullptr;
}

unsigned DOMSelection::focusOffset() const {
  if (Range* range = primaryRangeOrNull()) {
    if (!frame() || isBaseFirstInSelection())
      return range->endOffset();
    return range->startOffset();
  }
  return 0;
}

Node* DOMSelection::baseNode() const {
  if (!isAvailable())
    return 0;

  return shadowAdjustedNode(basePosition(visibleSelection()));
}

unsigned DOMSelection::baseOffset() const {
  if (!isAvailable())
    return 0;

  return shadowAdjustedOffset(basePosition(visibleSelection()));
}

Node* DOMSelection::extentNode() const {
  if (!isAvailable())
    return 0;

  return shadowAdjustedNode(extentPosition(visibleSelection()));
}

unsigned DOMSelection::extentOffset() const {
  if (!isAvailable())
    return 0;

  return shadowAdjustedOffset(extentPosition(visibleSelection()));
}

bool DOMSelection::isCollapsed() const {
  if (!isAvailable() || selectionShadowAncestor(frame()))
    return true;
  if (Range* range = primaryRangeOrNull())
    return range->collapsed();
  return true;
}

String DOMSelection::type() const {
  if (!isAvailable())
    return String();
  // This is a WebKit DOM extension, incompatible with an IE extension
  // IE has this same attribute, but returns "none", "text" and "control"
  // http://msdn.microsoft.com/en-us/library/ms534692(VS.85).aspx
  if (rangeCount() == 0)
    return "None";
  if (isCollapsed())
    return "Caret";
  return "Range";
}

unsigned DOMSelection::rangeCount() const {
  if (!isAvailable())
    return 0;
  if (documentCachedRange())
    return 1;
  if (frame()
          ->selection()
          .computeVisibleSelectionInDOMTreeDeprecated()
          .isNone())
    return 0;
  // Any selection can be adjusted to Range for Document.
  if (isSelectionOfDocument())
    return 1;
  // In ShadowRoot, we need to try adjustment.
  return createRangeFromSelectionEditor() ? 1 : 0;
}

// https://www.w3.org/TR/selection-api/#dom-selection-collapse
void DOMSelection::collapse(Node* node,
                            unsigned offset,
                            ExceptionState& exceptionState) {
  if (!isAvailable())
    return;

  // 1. If node is null, this method must behave identically as
  // removeAllRanges() and abort these steps.
  if (!node) {
    UseCounter::count(frame(), UseCounter::SelectionCollapseNull);
    frame()->selection().clear();
    return;
  }

  // 2. The method must throw an IndexSizeError exception if offset is longer
  // than node's length ([DOM4]) and abort these steps.
  Range::checkNodeWOffset(node, offset, exceptionState);
  if (exceptionState.hadException())
    return;

  // 3. If node's root is not the document associated with the context object,
  // abort these steps.
  if (!isValidForPosition(node))
    return;

  // 4. Otherwise, let newRange be a new range.
  Range* newRange = Range::create(*frame()->document());

  // 5. Set ([DOM4]) the start and the end of newRange to (node, offset).
  newRange->setStart(node, offset, exceptionState);
  if (exceptionState.hadException())
    return;
  newRange->setEnd(node, offset, exceptionState);
  if (exceptionState.hadException())
    return;

  // 6. Set the context object's range to newRange.
  updateFrameSelection(
      SelectionInDOMTree::Builder()
          .collapse(Position(node, offset))
          .setIsDirectional(frame()->selection().isDirectional())
          .build(),
      newRange);
}

// https://www.w3.org/TR/selection-api/#dom-selection-collapsetoend
void DOMSelection::collapseToEnd(ExceptionState& exceptionState) {
  if (!isAvailable())
    return;

  // The method must throw InvalidStateError exception if the context object is
  // empty.
  if (rangeCount() == 0) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "there is no selection.");
    return;
  }

  // Otherwise, it must create a new range, set both its start and end to the
  // end of the context object's range,
  Range* newRange = getRangeAt(0, ASSERT_NO_EXCEPTION)->cloneRange();
  newRange->collapse(false);

  // and then set the context object's range to the newly-created range.
  SelectionInDOMTree::Builder builder;
  builder.collapse(newRange->endPosition());
  updateFrameSelection(builder.build(), newRange);
}

// https://www.w3.org/TR/selection-api/#dom-selection-collapsetostart
void DOMSelection::collapseToStart(ExceptionState& exceptionState) {
  if (!isAvailable())
    return;

  // The method must throw InvalidStateError ([DOM4]) exception if the context
  // object is empty.
  if (rangeCount() == 0) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "there is no selection.");
    return;
  }

  // Otherwise, it must create a new range, set both its start and end to the
  // start of the context object's range,
  Range* newRange = getRangeAt(0, ASSERT_NO_EXCEPTION)->cloneRange();
  newRange->collapse(true);

  // and then set the context object's range to the newly-created range.
  SelectionInDOMTree::Builder builder;
  builder.collapse(newRange->startPosition());
  updateFrameSelection(builder.build(), newRange);
}

void DOMSelection::empty() {
  if (!isAvailable())
    return;
  frame()->selection().clear();
}

void DOMSelection::setBaseAndExtent(Node* baseNode,
                                    unsigned baseOffset,
                                    Node* extentNode,
                                    unsigned extentOffset,
                                    ExceptionState& exceptionState) {
  if (!isAvailable())
    return;

  // TODO(editing-dev): Behavior on where base or extent is null is still
  // under discussion: https://github.com/w3c/selection-api/issues/72
  if (!baseNode) {
    UseCounter::count(frame(), UseCounter::SelectionSetBaseAndExtentNull);
    frame()->selection().clear();
    return;
  }
  if (!extentNode) {
    UseCounter::count(frame(), UseCounter::SelectionSetBaseAndExtentNull);
    extentOffset = 0;
  }

  Range::checkNodeWOffset(baseNode, baseOffset, exceptionState);
  if (exceptionState.hadException())
    return;
  if (extentNode) {
    Range::checkNodeWOffset(extentNode, extentOffset, exceptionState);
    if (exceptionState.hadException())
      return;
  }

  if (!isValidForPosition(baseNode) || !isValidForPosition(extentNode))
    return;

  clearCachedRangeIfSelectionOfDocument();

  // TODO(editing-dev): Once SVG USE element doesn't modify DOM tree, we
  // should get rid of this update layout call.
  // See http://crbug.com/566281
  // See "svg/text/textpath-reference-crash.html"
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  Position basePosition(baseNode, baseOffset);
  Position extentPosition(extentNode, extentOffset);
  Range* newRange = Range::create(baseNode->document());
  if (extentPosition.isNull()) {
    newRange->setStart(baseNode, baseOffset);
    newRange->setEnd(baseNode, baseOffset);
  } else if (basePosition < extentPosition) {
    newRange->setStart(baseNode, baseOffset);
    newRange->setEnd(extentNode, extentOffset);
  } else {
    newRange->setStart(extentNode, extentOffset);
    newRange->setEnd(baseNode, baseOffset);
  }
  updateFrameSelection(
      SelectionInDOMTree::Builder()
          .setBaseAndExtentDeprecated(basePosition, extentPosition)
          .setIsDirectional(true)
          .build(),
      newRange);
}

void DOMSelection::modify(const String& alterString,
                          const String& directionString,
                          const String& granularityString) {
  if (!isAvailable())
    return;

  FrameSelection::EAlteration alter;
  if (equalIgnoringCase(alterString, "extend"))
    alter = FrameSelection::AlterationExtend;
  else if (equalIgnoringCase(alterString, "move"))
    alter = FrameSelection::AlterationMove;
  else
    return;

  SelectionDirection direction;
  if (equalIgnoringCase(directionString, "forward"))
    direction = DirectionForward;
  else if (equalIgnoringCase(directionString, "backward"))
    direction = DirectionBackward;
  else if (equalIgnoringCase(directionString, "left"))
    direction = DirectionLeft;
  else if (equalIgnoringCase(directionString, "right"))
    direction = DirectionRight;
  else
    return;

  TextGranularity granularity;
  if (equalIgnoringCase(granularityString, "character"))
    granularity = CharacterGranularity;
  else if (equalIgnoringCase(granularityString, "word"))
    granularity = WordGranularity;
  else if (equalIgnoringCase(granularityString, "sentence"))
    granularity = SentenceGranularity;
  else if (equalIgnoringCase(granularityString, "line"))
    granularity = LineGranularity;
  else if (equalIgnoringCase(granularityString, "paragraph"))
    granularity = ParagraphGranularity;
  else if (equalIgnoringCase(granularityString, "lineboundary"))
    granularity = LineBoundary;
  else if (equalIgnoringCase(granularityString, "sentenceboundary"))
    granularity = SentenceBoundary;
  else if (equalIgnoringCase(granularityString, "paragraphboundary"))
    granularity = ParagraphBoundary;
  else if (equalIgnoringCase(granularityString, "documentboundary"))
    granularity = DocumentBoundary;
  else
    return;

  Element* focusedElement = frame()->document()->focusedElement();
  frame()->selection().modify(alter, direction, granularity);
  if (frame() && frame()->document() &&
      focusedElement != frame()->document()->focusedElement())
    UseCounter::count(frame(), UseCounter::SelectionFuncionsChangeFocus);
}

// https://www.w3.org/TR/selection-api/#dom-selection-extend
void DOMSelection::extend(Node* node,
                          unsigned offset,
                          ExceptionState& exceptionState) {
  DCHECK(node);
  if (!isAvailable())
    return;

  // 1. If node's root is not the document associated with the context object,
  // abort these steps.
  if (!isValidForPosition(node))
    return;

  // 2. If the context object is empty, throw an InvalidStateError exception and
  // abort these steps.
  if (rangeCount() == 0) {
    exceptionState.throwDOMException(
        InvalidStateError, "This Selection object doesn't have any Ranges.");
    return;
  }

  Range::checkNodeWOffset(node, offset, exceptionState);
  if (exceptionState.hadException())
    return;

  // 3. Let oldAnchor and oldFocus be the context object's anchor and focus, and
  // let newFocus be the boundary point (node, offset).
  const Position oldAnchor(anchorNode(), anchorOffset());
  DCHECK(!oldAnchor.isNull());
  const Position newFocus(node, offset);

  clearCachedRangeIfSelectionOfDocument();

  // 4. Let newRange be a new range.
  Range* newRange = Range::create(*frame()->document());

  // 5. If node's root is not the same as the context object's range's root, set
  // newRange's start and end to newFocus.
  // E.g. oldAnchor might point in shadow Text node in TextControlElement.
  if (oldAnchor.anchorNode()->treeRoot() != node->treeRoot()) {
    newRange->setStart(node, offset);
    newRange->setEnd(node, offset);

  } else if (oldAnchor <= newFocus) {
    // 6. Otherwise, if oldAnchor is before or equal to newFocus, set newRange's
    // start to oldAnchor, then set its end to newFocus.
    newRange->setStart(oldAnchor.anchorNode(),
                       oldAnchor.offsetInContainerNode());
    newRange->setEnd(node, offset);

  } else {
    // 7. Otherwise, set newRange's start to newFocus, then set its end to
    // oldAnchor.
    newRange->setStart(node, offset);
    newRange->setEnd(oldAnchor.anchorNode(), oldAnchor.offsetInContainerNode());
  }

  // 8. Set the context object's range to newRange.
  SelectionInDOMTree::Builder builder;
  if (newRange->collapsed())
    builder.collapse(newFocus);
  else
    builder.collapse(oldAnchor).extend(newFocus);
  updateFrameSelection(builder.setIsDirectional(true).build(), newRange);
}

Range* DOMSelection::getRangeAt(unsigned index,
                                ExceptionState& exceptionState) const {
  if (!isAvailable())
    return nullptr;

  if (index >= rangeCount()) {
    exceptionState.throwDOMException(
        IndexSizeError, String::number(index) + " is not a valid index.");
    return nullptr;
  }

  // If you're hitting this, you've added broken multi-range selection support
  DCHECK_EQ(rangeCount(), 1u);

  if (Range* cachedRange = documentCachedRange())
    return cachedRange;

  Range* range = createRangeFromSelectionEditor();
  cacheRangeIfSelectionOfDocument(range);
  return range;
}

Range* DOMSelection::primaryRangeOrNull() const {
  return rangeCount() > 0 ? getRangeAt(0, ASSERT_NO_EXCEPTION) : nullptr;
}

Range* DOMSelection::createRangeFromSelectionEditor() const {
  Position anchor = blink::anchorPosition(visibleSelection());
  if (isSelectionOfDocument() && !anchor.anchorNode()->isInShadowTree())
    return frame()->selection().firstRange();

  Node* node = shadowAdjustedNode(anchor);
  if (!node)  // crbug.com/595100
    return nullptr;
  Position focus = focusPosition(visibleSelection());
  if (!visibleSelection().isBaseFirst()) {
    return Range::create(*anchor.document(), shadowAdjustedNode(focus),
                         shadowAdjustedOffset(focus), node,
                         shadowAdjustedOffset(anchor));
  }
  return Range::create(*anchor.document(), node, shadowAdjustedOffset(anchor),
                       shadowAdjustedNode(focus), shadowAdjustedOffset(focus));
}

bool DOMSelection::isSelectionOfDocument() const {
  return m_treeScope == m_treeScope->document();
}

void DOMSelection::cacheRangeIfSelectionOfDocument(Range* range) const {
  if (!isSelectionOfDocument())
    return;
  if (!frame())
    return;
  frame()->selection().cacheRangeOfDocument(range);
}

Range* DOMSelection::documentCachedRange() const {
  if (!isSelectionOfDocument())
    return nullptr;
  return frame()->selection().documentCachedRange();
}

void DOMSelection::clearCachedRangeIfSelectionOfDocument() {
  if (!isSelectionOfDocument())
    return;
  frame()->selection().clearDocumentCachedRange();
}

void DOMSelection::removeRange(Range* range) {
  DCHECK(range);
  if (!isAvailable())
    return;
  if (range == primaryRangeOrNull())
    frame()->selection().clear();
}

void DOMSelection::removeAllRanges() {
  if (!isAvailable())
    return;
  frame()->selection().clear();
}

void DOMSelection::addRange(Range* newRange) {
  DCHECK(newRange);

  if (!isAvailable())
    return;

  if (newRange->ownerDocument() != frame()->document())
    return;

  if (!newRange->isConnected()) {
    addConsoleError("The given range isn't in document.");
    return;
  }

  FrameSelection& selection = frame()->selection();

  if (newRange->ownerDocument() != selection.document()) {
    // "editing/selection/selection-in-iframe-removed-crash.html" goes here.
    return;
  }

  if (rangeCount() == 0) {
    updateFrameSelection(SelectionInDOMTree::Builder()
                             .collapse(newRange->startPosition())
                             .extend(newRange->endPosition())
                             .build(),
                         newRange);
    return;
  }

  Range* originalRange = primaryRangeOrNull();
  DCHECK(originalRange);

  if (originalRange->startContainer()->treeScope() !=
      newRange->startContainer()->treeScope()) {
    return;
  }

  if (originalRange->compareBoundaryPoints(Range::kStartToEnd, newRange,
                                           ASSERT_NO_EXCEPTION) < 0 ||
      newRange->compareBoundaryPoints(Range::kStartToEnd, originalRange,
                                      ASSERT_NO_EXCEPTION) < 0) {
    return;
  }

  // TODO(tkent): "Merge the ranges if they intersect" was removed. We show a
  // warning message for a while, and continue to collect the usage data.
  // <https://code.google.com/p/chromium/issues/detail?id=353069>.
  Deprecation::countDeprecation(frame(),
                                UseCounter::SelectionAddRangeIntersect);
}

// https://www.w3.org/TR/selection-api/#dom-selection-deletefromdocument
void DOMSelection::deleteFromDocument() {
  if (!isAvailable())
    return;

  // The method must invoke deleteContents() ([DOM4]) on the context object's
  // range if the context object is not empty. Otherwise the method must do
  // nothing.
  if (Range* range = documentCachedRange()) {
    range->deleteContents(ASSERT_NO_EXCEPTION);
    return;
  }

  // The following code is necessary for
  // editing/selection/deleteFromDocument-crash.html, which assumes
  // deleteFromDocument() for text selection in a TEXTAREA deletes the TEXTAREA
  // value.

  FrameSelection& selection = frame()->selection();

  if (selection.computeVisibleSelectionInDOMTreeDeprecated().isNone())
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // |VisibleSelection::toNormalizedEphemeralRange| requires clean layout.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  Range* selectedRange =
      createRange(selection.computeVisibleSelectionInDOMTreeDeprecated()
                      .toNormalizedEphemeralRange());
  if (!selectedRange)
    return;

  // |selectedRange| may point nodes in a different root.
  selectedRange->deleteContents(ASSERT_NO_EXCEPTION);
}

bool DOMSelection::containsNode(const Node* n, bool allowPartial) const {
  DCHECK(n);

  if (!isAvailable())
    return false;

  if (frame()->document() != n->document())
    return false;

  unsigned nodeIndex = n->nodeIndex();

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // |VisibleSelection::toNormalizedEphemeralRange| requires clean layout.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  FrameSelection& selection = frame()->selection();
  const EphemeralRange selectedRange =
      selection.computeVisibleSelectionInDOMTreeDeprecated()
          .toNormalizedEphemeralRange();
  if (selectedRange.isNull())
    return false;

  ContainerNode* parentNode = n->parentNode();
  if (!parentNode)
    return false;

  const Position startPosition =
      selectedRange.startPosition().toOffsetInAnchor();
  const Position endPosition = selectedRange.endPosition().toOffsetInAnchor();
  DummyExceptionStateForTesting exceptionState;
  bool nodeFullySelected =
      Range::compareBoundaryPoints(
          parentNode, nodeIndex, startPosition.computeContainerNode(),
          startPosition.offsetInContainerNode(), exceptionState) >= 0 &&
      !exceptionState.hadException() &&
      Range::compareBoundaryPoints(
          parentNode, nodeIndex + 1, endPosition.computeContainerNode(),
          endPosition.offsetInContainerNode(), exceptionState) <= 0 &&
      !exceptionState.hadException();
  if (exceptionState.hadException())
    return false;
  if (nodeFullySelected)
    return true;

  bool nodeFullyUnselected =
      (Range::compareBoundaryPoints(
           parentNode, nodeIndex, endPosition.computeContainerNode(),
           endPosition.offsetInContainerNode(), exceptionState) > 0 &&
       !exceptionState.hadException()) ||
      (Range::compareBoundaryPoints(
           parentNode, nodeIndex + 1, startPosition.computeContainerNode(),
           startPosition.offsetInContainerNode(), exceptionState) < 0 &&
       !exceptionState.hadException());
  DCHECK(!exceptionState.hadException());
  if (nodeFullyUnselected)
    return false;

  return allowPartial || n->isTextNode();
}

void DOMSelection::selectAllChildren(Node* n, ExceptionState& exceptionState) {
  DCHECK(n);

  // This doesn't (and shouldn't) select text node characters.
  setBaseAndExtent(n, 0, n, n->countChildren(), exceptionState);
}

String DOMSelection::toString() {
  if (!isAvailable())
    return String();

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      frame()->document()->lifecycle());

  const EphemeralRange range = frame()
                                   ->selection()
                                   .computeVisibleSelectionInDOMTreeDeprecated()
                                   .toNormalizedEphemeralRange();
  return plainText(
      range,
      TextIteratorBehavior::Builder().setForSelectionToString(true).build());
}

Node* DOMSelection::shadowAdjustedNode(const Position& position) const {
  if (position.isNull())
    return 0;

  Node* containerNode = position.computeContainerNode();
  Node* adjustedNode = m_treeScope->ancestorInThisScope(containerNode);

  if (!adjustedNode)
    return 0;

  if (containerNode == adjustedNode)
    return containerNode;

  DCHECK(!adjustedNode->isShadowRoot()) << adjustedNode;
  return adjustedNode->parentOrShadowHostNode();
}

unsigned DOMSelection::shadowAdjustedOffset(const Position& position) const {
  if (position.isNull())
    return 0;

  Node* containerNode = position.computeContainerNode();
  Node* adjustedNode = m_treeScope->ancestorInThisScope(containerNode);

  if (!adjustedNode)
    return 0;

  if (containerNode == adjustedNode)
    return position.computeOffsetInContainerNode();

  return adjustedNode->nodeIndex();
}

bool DOMSelection::isValidForPosition(Node* node) const {
  DCHECK(frame());
  if (!node)
    return true;
  return node->document() == frame()->document() && node->isConnected();
}

void DOMSelection::addConsoleError(const String& message) {
  if (m_treeScope)
    m_treeScope->document().addConsoleMessage(
        ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
}

DEFINE_TRACE(DOMSelection) {
  visitor->trace(m_treeScope);
  ContextClient::trace(visitor);
}

}  // namespace blink
