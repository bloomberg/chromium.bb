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

#include "core/editing/FrameSelection.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/CharacterData.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/NodeWithIndex.h"
#include "core/dom/Text.h"
#include "core/editing/CaretDisplayItemClient.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameCaret.h"
#include "core/editing/GranularityStrategy.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/PendingSelection.h"
#include "core/editing/SelectionController.h"
#include "core/editing/SelectionEditor.h"
#include "core/editing/SelectionModifier.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/commands/TypingCommand.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/serializers/Serialization.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/EditorClient.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/SpatialNavigation.h"
#include "core/paint/PaintLayer.h"
#include "platform/SecureTextInput.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/text/UnicodeUtilities.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/CString.h"
#include <stdio.h>

#define EDIT_DEBUG 0

namespace blink {

using namespace HTMLNames;

static inline bool shouldAlwaysUseDirectionalSelection(LocalFrame* frame) {
  return frame->editor().behavior().shouldConsiderSelectionAsDirectional();
}

FrameSelection::FrameSelection(LocalFrame& frame)
    : m_frame(frame),
      m_pendingSelection(PendingSelection::create(*this)),
      m_selectionEditor(SelectionEditor::create(frame)),
      m_granularity(CharacterGranularity),
      m_xPosForVerticalArrowNavigation(NoXPosForVerticalArrowNavigation()),
      m_focused(frame.page() &&
                frame.page()->focusController().focusedFrame() == frame),
      m_frameCaret(new FrameCaret(frame, *m_selectionEditor)) {}

FrameSelection::~FrameSelection() {}

const DisplayItemClient& FrameSelection::caretDisplayItemClientForTesting()
    const {
  return m_frameCaret->displayItemClient();
}

Document& FrameSelection::document() const {
  DCHECK(lifecycleContext());
  return *lifecycleContext();
}

bool FrameSelection::isHandleVisible() const {
  return selectionInDOMTree().isHandleVisible();
}

const VisibleSelection& FrameSelection::computeVisibleSelectionInDOMTree()
    const {
  return m_selectionEditor->computeVisibleSelectionInDOMTree();
}

const VisibleSelectionInFlatTree&
FrameSelection::computeVisibleSelectionInFlatTree() const {
  return m_selectionEditor->computeVisibleSelectionInFlatTree();
}

const SelectionInDOMTree& FrameSelection::selectionInDOMTree() const {
  return m_selectionEditor->selectionInDOMTree();
}

Element* FrameSelection::rootEditableElementOrDocumentElement() const {
  Element* selectionRoot =
      computeVisibleSelectionInDOMTreeDeprecated().rootEditableElement();
  return selectionRoot ? selectionRoot : document().documentElement();
}

// TODO(yosin): We should move |rootEditableElementOrTreeScopeRootNodeOf()| to
// "EditingUtilities.cpp"
ContainerNode* rootEditableElementOrTreeScopeRootNodeOf(
    const Position& position) {
  Element* selectionRoot = rootEditableElementOf(position);
  if (selectionRoot)
    return selectionRoot;

  Node* const node = position.computeContainerNode();
  return node ? &node->treeScope().rootNode() : 0;
}

const VisibleSelection&
FrameSelection::computeVisibleSelectionInDOMTreeDeprecated() const {
  // TODO(yosin): We should hoist updateStyleAndLayoutIgnorePendingStylesheets
  // to caller. See http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  return computeVisibleSelectionInDOMTree();
}

const VisibleSelectionInFlatTree& FrameSelection::selectionInFlatTree() const {
  return computeVisibleSelectionInFlatTree();
}

void FrameSelection::moveCaretSelection(const IntPoint& point) {
  DCHECK(!document().needsLayoutTreeUpdate());

  Element* const editable =
      computeVisibleSelectionInDOMTree().rootEditableElement();
  if (!editable)
    return;

  const VisiblePosition position =
      visiblePositionForContentsPoint(point, frame());
  SelectionInDOMTree::Builder builder;
  builder.setIsDirectional(selectionInDOMTree().isDirectional());
  builder.setIsHandleVisible(true);
  if (position.isNotNull())
    builder.collapse(position.toPositionWithAffinity());
  setSelection(builder.build(), CloseTyping | ClearTypingStyle | UserTriggered);
}

void FrameSelection::setSelection(const SelectionInDOMTree& passedSelection,
                                  SetSelectionOptions options,
                                  CursorAlignOnScroll align,
                                  TextGranularity granularity) {
  if (setSelectionDeprecated(passedSelection, options, granularity))
    didSetSelectionDeprecated(options, align);
}

bool FrameSelection::setSelectionDeprecated(
    const SelectionInDOMTree& passedSelection,
    SetSelectionOptions options,
    TextGranularity granularity) {
  DCHECK(isAvailable());
  passedSelection.assertValidFor(document());

  SelectionInDOMTree::Builder builder(passedSelection);
  if (shouldAlwaysUseDirectionalSelection(m_frame))
    builder.setIsDirectional(true);
  SelectionInDOMTree newSelection = builder.build();
  if (m_granularityStrategy &&
      (options & FrameSelection::DoNotClearStrategy) == 0)
    m_granularityStrategy->Clear();
  bool closeTyping = options & CloseTyping;
  bool shouldClearTypingStyle = options & ClearTypingStyle;
  m_granularity = granularity;

  // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
  // |Editor| class.
  if (closeTyping)
    TypingCommand::closeTyping(m_frame);

  if (shouldClearTypingStyle)
    m_frame->editor().clearTypingStyle();

  const SelectionInDOMTree oldSelectionInDOMTree =
      m_selectionEditor->selectionInDOMTree();
  if (oldSelectionInDOMTree == newSelection)
    return false;
  m_selectionEditor->setSelection(newSelection);
  scheduleVisualUpdateForPaintInvalidationIfNeeded();

  const Document& currentDocument = document();
  // TODO(yosin): We should get rid of unsued |options| for
  // |Editor::respondToChangedSelection()|.
  // Note: Since, setting focus can modify DOM tree, we should use
  // |oldSelection| before setting focus
  m_frame->editor().respondToChangedSelection(
      oldSelectionInDOMTree.computeStartPosition(), options);
  DCHECK_EQ(currentDocument, document());
  return true;
}

void FrameSelection::didSetSelectionDeprecated(SetSelectionOptions options,
                                               CursorAlignOnScroll align) {
  const Document& currentDocument = document();
  if (!selectionInDOMTree().isNone() && !(options & DoNotSetFocus)) {
    setFocusedNodeIfNeeded();
    // |setFocusedNodeIfNeeded()| dispatches sync events "FocusOut" and
    // "FocusIn", |m_frame| may associate to another document.
    if (!isAvailable() || document() != currentDocument) {
      // Once we get test case to reach here, we should change this
      // if-statement to |DCHECK()|.
      NOTREACHED();
      return;
    }
  }

  m_frameCaret->stopCaretBlinkTimer();
  updateAppearance();

  // Always clear the x position used for vertical arrow navigation.
  // It will be restored by the vertical arrow navigation code if necessary.
  m_xPosForVerticalArrowNavigation = NoXPosForVerticalArrowNavigation();

  // TODO(yosin): Can we move this to at end of this function?
  // This may dispatch a synchronous focus-related events.
  if (!(options & DoNotSetFocus)) {
    selectFrameElementInParentIfFullySelected();
    if (!isAvailable() || document() != currentDocument) {
      // editing/selection/selectallchildren-crash.html and
      // editing/selection/longpress-selection-in-iframe-removed-crash.html
      // reach here.
      return;
    }
  }

  EUserTriggered userTriggered = selectionOptionsToUserTriggered(options);
  notifyLayoutObjectOfSelectionChange(userTriggered);
  if (userTriggered == UserTriggered) {
    ScrollAlignment alignment;

    if (m_frame->editor().behavior().shouldCenterAlignWhenSelectionIsRevealed())
      alignment = (align == CursorAlignOnScroll::Always)
                      ? ScrollAlignment::alignCenterAlways
                      : ScrollAlignment::alignCenterIfNeeded;
    else
      alignment = (align == CursorAlignOnScroll::Always)
                      ? ScrollAlignment::alignTopAlways
                      : ScrollAlignment::alignToEdgeIfNeeded;

    revealSelection(alignment, RevealExtent);
  }

  notifyAccessibilityForSelectionChange();
  notifyCompositorForSelectionChange();
  notifyEventHandlerForSelectionChange();
  m_frame->domWindow()->enqueueDocumentEvent(
      Event::create(EventTypeNames::selectionchange));
}

void FrameSelection::setSelection(const SelectionInFlatTree& newSelection,
                                  SetSelectionOptions options,
                                  CursorAlignOnScroll align,
                                  TextGranularity granularity) {
  newSelection.assertValidFor(document());
  SelectionInDOMTree::Builder builder;
  builder.setAffinity(newSelection.affinity())
      .setBaseAndExtent(toPositionInDOMTree(newSelection.base()),
                        toPositionInDOMTree(newSelection.extent()))
      .setGranularity(newSelection.granularity())
      .setIsDirectional(newSelection.isDirectional())
      .setIsHandleVisible(newSelection.isHandleVisible())
      .setHasTrailingWhitespace(newSelection.hasTrailingWhitespace());
  return setSelection(builder.build(), options, align, granularity);
}

void FrameSelection::nodeChildrenWillBeRemoved(ContainerNode& container) {
  if (!container.inActiveDocument())
    return;
  // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
  // |Editor| class.
  if (!document().isRunningExecCommand())
    TypingCommand::closeTyping(m_frame);
}

void FrameSelection::nodeWillBeRemoved(Node& node) {
  // There can't be a selection inside a fragment, so if a fragment's node is
  // being removed, the selection in the document that created the fragment
  // needs no adjustment.
  if (!node.inActiveDocument())
    return;
  // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
  // |Editor| class.
  if (!document().isRunningExecCommand())
    TypingCommand::closeTyping(m_frame);
}

void FrameSelection::didChangeFocus() {
  // Hits in
  // virtual/gpu/compositedscrolling/scrollbars/scrollbar-miss-mousemove-disabled.html
  DisableCompositingQueryAsserts disabler;
  updateAppearance();
}

static DispatchEventResult dispatchSelectStart(
    const VisibleSelection& selection) {
  Node* selectStartTarget = selection.extent().computeContainerNode();
  if (!selectStartTarget)
    return DispatchEventResult::NotCanceled;

  return selectStartTarget->dispatchEvent(
      Event::createCancelableBubble(EventTypeNames::selectstart));
}

// The return value of |FrameSelection::modify()| is different based on
// value of |userTriggered| parameter.
// When |userTriggered| is |userTriggered|, |modify()| returns false if
// "selectstart" event is dispatched and canceled, otherwise returns true.
// When |userTriggered| is |NotUserTrigged|, return value specifies whether
// selection is modified or not.
bool FrameSelection::modify(EAlteration alter,
                            SelectionDirection direction,
                            TextGranularity granularity,
                            EUserTriggered userTriggered) {
  SelectionModifier selectionModifier(
      *frame(), computeVisibleSelectionInDOMTreeDeprecated(),
      m_xPosForVerticalArrowNavigation);
  const bool modified = selectionModifier.modify(alter, direction, granularity);
  if (userTriggered == UserTriggered &&
      selectionModifier.selection().isRange() &&
      computeVisibleSelectionInDOMTreeDeprecated().isCaret() &&
      dispatchSelectStart(computeVisibleSelectionInDOMTreeDeprecated()) !=
          DispatchEventResult::NotCanceled) {
    return false;
  }
  if (!modified) {
    if (userTriggered == NotUserTriggered)
      return false;
    // If spatial navigation enabled, focus navigator will move focus to
    // another element. See snav-input.html and snav-textarea.html
    if (isSpatialNavigationEnabled(m_frame))
      return false;
    // Even if selection isn't changed, we prevent to default action, e.g.
    // scroll window when caret is at end of content editable.
    return true;
  }

  const SetSelectionOptions options =
      CloseTyping | ClearTypingStyle | userTriggered;
  setSelection(selectionModifier.selection().asSelection(), options);

  if (granularity == LineGranularity || granularity == ParagraphGranularity)
    m_xPosForVerticalArrowNavigation =
        selectionModifier.xPosForVerticalArrowNavigation();

  if (userTriggered == UserTriggered)
    m_granularity = CharacterGranularity;

  scheduleVisualUpdateForPaintInvalidationIfNeeded();

  return true;
}

bool FrameSelection::modify(EAlteration alter,
                            unsigned verticalDistance,
                            VerticalDirection direction) {
  SelectionModifier selectionModifier(
      *frame(), computeVisibleSelectionInDOMTreeDeprecated());
  if (!selectionModifier.modifyWithPageGranularity(alter, verticalDistance,
                                                   direction)) {
    return false;
  }

  setSelection(selectionModifier.selection().asSelection(),
               CloseTyping | ClearTypingStyle | UserTriggered,
               alter == AlterationMove ? CursorAlignOnScroll::Always
                                       : CursorAlignOnScroll::IfNeeded);

  m_granularity = CharacterGranularity;

  return true;
}

void FrameSelection::clear() {
  m_granularity = CharacterGranularity;
  if (m_granularityStrategy)
    m_granularityStrategy->Clear();
  setSelection(SelectionInDOMTree());
}

void FrameSelection::documentAttached(Document* document) {
  DCHECK(document);
  m_useSecureKeyboardEntryWhenActive = false;
  m_selectionEditor->documentAttached(document);
  setContext(document);
}

void FrameSelection::contextDestroyed(Document* document) {
  m_granularity = CharacterGranularity;

  LayoutViewItem view = m_frame->contentLayoutItem();
  if (!view.isNull())
    view.clearSelection();

  m_frame->editor().clearTypingStyle();
}

void FrameSelection::clearPreviousCaretVisualRect(const LayoutBlock& block) {
  m_frameCaret->clearPreviousVisualRect(block);
}

void FrameSelection::layoutBlockWillBeDestroyed(const LayoutBlock& block) {
  m_frameCaret->layoutBlockWillBeDestroyed(block);
}

void FrameSelection::updateStyleAndLayoutIfNeeded() {
  m_frameCaret->updateStyleAndLayoutIfNeeded();
}

void FrameSelection::invalidatePaintIfNeeded(
    const LayoutBlock& block,
    const PaintInvalidatorContext& context) {
  m_frameCaret->invalidatePaintIfNeeded(block, context);
}

bool FrameSelection::shouldPaintCaret(const LayoutBlock& block) const {
  DCHECK_GE(document().lifecycle().state(), DocumentLifecycle::LayoutClean);
  bool result = m_frameCaret->shouldPaintCaret(block);
  DCHECK(!result ||
         (computeVisibleSelectionInDOMTreeDeprecated().isCaret() &&
          computeVisibleSelectionInDOMTree().hasEditableStyle()));
  return result;
}

IntRect FrameSelection::absoluteCaretBounds() {
  DCHECK(computeVisibleSelectionInDOMTree().isValidFor(*m_frame->document()));
  return m_frameCaret->absoluteCaretBounds();
}

void FrameSelection::paintCaret(GraphicsContext& context,
                                const LayoutPoint& paintOffset) {
  m_frameCaret->paintCaret(context, paintOffset);
}

bool FrameSelection::contains(const LayoutPoint& point) {
  if (document().layoutViewItem().isNull())
    return false;

  // Treat a collapsed selection like no selection.
  const VisibleSelectionInFlatTree& visibleSelection =
      computeVisibleSelectionInFlatTree();
  if (!visibleSelection.isRange())
    return false;

  HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
  HitTestResult result(request, point);
  document().layoutViewItem().hitTest(result);
  Node* innerNode = result.innerNode();
  if (!innerNode || !innerNode->layoutObject())
    return false;

  const VisiblePositionInFlatTree& visiblePos =
      createVisiblePosition(fromPositionInDOMTree<EditingInFlatTreeStrategy>(
          innerNode->layoutObject()->positionForPoint(result.localPoint())));
  if (visiblePos.isNull())
    return false;

  const VisiblePositionInFlatTree& visibleStart =
      visibleSelection.visibleStart();
  const VisiblePositionInFlatTree& visibleEnd = visibleSelection.visibleEnd();
  if (visibleStart.isNull() || visibleEnd.isNull())
    return false;

  const PositionInFlatTree& start = visibleStart.deepEquivalent();
  const PositionInFlatTree& end = visibleEnd.deepEquivalent();
  const PositionInFlatTree& pos = visiblePos.deepEquivalent();
  return start.compareTo(pos) <= 0 && pos.compareTo(end) <= 0;
}

// Workaround for the fact that it's hard to delete a frame.
// Call this after doing user-triggered selections to make it easy to delete the
// frame you entirely selected. Can't do this implicitly as part of every
// setSelection call because in some contexts it might not be good for the focus
// to move to another frame. So instead we call it from places where we are
// selecting with the mouse or the keyboard after setting the selection.
void FrameSelection::selectFrameElementInParentIfFullySelected() {
  // Find the parent frame; if there is none, then we have nothing to do.
  Frame* parent = m_frame->tree().parent();
  if (!parent)
    return;
  Page* page = m_frame->page();
  if (!page)
    return;

  // Check if the selection contains the entire frame contents; if not, then
  // there is nothing to do.
  if (selectionInDOMTree().selectionTypeWithLegacyGranularity() !=
      RangeSelection) {
    return;
  }

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  if (!isStartOfDocument(computeVisibleSelectionInDOMTree().visibleStart()))
    return;
  if (!isEndOfDocument(computeVisibleSelectionInDOMTree().visibleEnd()))
    return;

  // FIXME: This is not yet implemented for cross-process frame relationships.
  if (!parent->isLocalFrame())
    return;

  // Get to the <iframe> or <frame> (or even <object>) element in the parent
  // frame.
  // FIXME: Doesn't work for OOPI.
  HTMLFrameOwnerElement* ownerElement = m_frame->deprecatedLocalOwner();
  if (!ownerElement)
    return;
  ContainerNode* ownerElementParent = ownerElement->parentNode();
  if (!ownerElementParent)
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  ownerElementParent->document().updateStyleAndLayoutIgnorePendingStylesheets();

  // This method's purpose is it to make it easier to select iframes (in order
  // to delete them).  Don't do anything if the iframe isn't deletable.
  if (!blink::hasEditableStyle(*ownerElementParent))
    return;

  // Create compute positions before and after the element.
  unsigned ownerElementNodeIndex = ownerElement->nodeIndex();
  VisiblePosition beforeOwnerElement = createVisiblePosition(
      Position(ownerElementParent, ownerElementNodeIndex));
  VisiblePosition afterOwnerElement = createVisiblePosition(
      Position(ownerElementParent, ownerElementNodeIndex + 1),
      VP_UPSTREAM_IF_POSSIBLE);

  SelectionInDOMTree::Builder builder;
  builder
      .setBaseAndExtentDeprecated(beforeOwnerElement.deepEquivalent(),
                                  afterOwnerElement.deepEquivalent())
      .setAffinity(beforeOwnerElement.affinity());

  // Focus on the parent frame, and then select from before this element to
  // after.
  VisibleSelection newSelection = createVisibleSelection(builder.build());
  // TODO(yosin): We should call |FocusController::setFocusedFrame()| before
  // |createVisibleSelection()|.
  page->focusController().setFocusedFrame(parent);
  // setFocusedFrame can dispatch synchronous focus/blur events.  The document
  // tree might be modified.
  if (newSelection.isNonOrphanedCaretOrRange())
    toLocalFrame(parent)->selection().setSelection(newSelection.asSelection());
}

// Returns a shadow tree node for legacy shadow trees, a child of the
// ShadowRoot node for new shadow trees, or 0 for non-shadow trees.
static Node* nonBoundaryShadowTreeRootNode(const Position& position) {
  return position.anchorNode() && !position.anchorNode()->isShadowRoot()
             ? position.anchorNode()->nonBoundaryShadowTreeRootNode()
             : nullptr;
}

void FrameSelection::selectAll() {
  if (isHTMLSelectElement(document().focusedElement())) {
    HTMLSelectElement* selectElement =
        toHTMLSelectElement(document().focusedElement());
    if (selectElement->canSelectAll()) {
      selectElement->selectAll();
      return;
    }
  }

  Node* root = nullptr;
  Node* selectStartTarget = nullptr;
  if (computeVisibleSelectionInDOMTreeDeprecated().isContentEditable()) {
    root = highestEditableRoot(
        computeVisibleSelectionInDOMTreeDeprecated().start());
    if (Node* shadowRoot = nonBoundaryShadowTreeRootNode(
            computeVisibleSelectionInDOMTreeDeprecated().start()))
      selectStartTarget = shadowRoot->ownerShadowHost();
    else
      selectStartTarget = root;
  } else {
    root = nonBoundaryShadowTreeRootNode(
        computeVisibleSelectionInDOMTreeDeprecated().start());
    if (root) {
      selectStartTarget = root->ownerShadowHost();
    } else {
      root = document().documentElement();
      selectStartTarget = document().body();
    }
  }
  if (!root || editingIgnoresContent(*root))
    return;

  if (selectStartTarget) {
    const Document& expectedDocument = document();
    if (selectStartTarget->dispatchEvent(Event::createCancelableBubble(
            EventTypeNames::selectstart)) != DispatchEventResult::NotCanceled)
      return;
    // |root| may be detached due to selectstart event.
    if (!root->isConnected() || expectedDocument != root->document())
      return;
  }

  setSelection(SelectionInDOMTree::Builder()
                   .selectAllChildren(*root)
                   .setIsHandleVisible(isHandleVisible())
                   .build());
  selectFrameElementInParentIfFullySelected();
  notifyLayoutObjectOfSelectionChange(UserTriggered);
}

bool FrameSelection::setSelectedRange(const EphemeralRange& range,
                                      TextAffinity affinity,
                                      SelectionDirectionalMode directional,
                                      SetSelectionOptions options) {
  if (range.isNull())
    return false;
  setSelection(SelectionInDOMTree::Builder()
                   .setBaseAndExtent(range)
                   .setAffinity(affinity)
                   .setIsHandleVisible(isHandleVisible())
                   .setIsDirectional(directional ==
                                     SelectionDirectionalMode::Directional)
                   .build(),
               options);
  return true;
}

void FrameSelection::notifyAccessibilityForSelectionChange() {
  if (selectionInDOMTree().isNone())
    return;
  AXObjectCache* cache = document().existingAXObjectCache();
  if (!cache)
    return;
  const Position& start = selectionInDOMTree().computeStartPosition();
  cache->selectionChanged(start.computeContainerNode());
}

void FrameSelection::notifyCompositorForSelectionChange() {
  if (!RuntimeEnabledFeatures::compositedSelectionUpdateEnabled())
    return;

  scheduleVisualUpdate();
}

void FrameSelection::notifyEventHandlerForSelectionChange() {
  m_frame->eventHandler().selectionController().notifySelectionChanged();
}

void FrameSelection::focusedOrActiveStateChanged() {
  bool activeAndFocused = isFocusedAndActive();

  // Trigger style invalidation from the focused element. Even though
  // the focused element hasn't changed, the evaluation of focus pseudo
  // selectors are dependent on whether the frame is focused and active.
  if (Element* element = document().focusedElement())
    element->focusStateChanged();

  document().updateStyleAndLayoutTree();

  // Because LayoutObject::selectionBackgroundColor() and
  // LayoutObject::selectionForegroundColor() check if the frame is active,
  // we have to update places those colors were painted.
  LayoutViewItem view = document().layoutViewItem();
  if (!view.isNull())
    view.invalidatePaintForSelection();

  // Caret appears in the active frame.
  if (activeAndFocused)
    setSelectionFromNone();
  else
    m_frame->spellChecker().spellCheckAfterBlur();
  m_frameCaret->setCaretVisibility(activeAndFocused ? CaretVisibility::Visible
                                                    : CaretVisibility::Hidden);

  // Update for caps lock state
  m_frame->eventHandler().capsLockStateMayHaveChanged();

  // Secure keyboard entry is set by the active frame.
  if (m_useSecureKeyboardEntryWhenActive)
    setUseSecureKeyboardEntry(activeAndFocused);
}

void FrameSelection::pageActivationChanged() {
  focusedOrActiveStateChanged();
}

void FrameSelection::updateSecureKeyboardEntryIfActive() {
  if (!isFocusedAndActive())
    return;
  setUseSecureKeyboardEntry(m_useSecureKeyboardEntryWhenActive);
}

void FrameSelection::setUseSecureKeyboardEntryWhenActive(
    bool usesSecureKeyboard) {
  if (m_useSecureKeyboardEntryWhenActive == usesSecureKeyboard)
    return;
  m_useSecureKeyboardEntryWhenActive = usesSecureKeyboard;
  updateSecureKeyboardEntryIfActive();
}

void FrameSelection::setUseSecureKeyboardEntry(bool enable) {
  if (enable)
    enableSecureTextInput();
  else
    disableSecureTextInput();
}

void FrameSelection::setFocused(bool flag) {
  if (m_focused == flag)
    return;
  m_focused = flag;

  focusedOrActiveStateChanged();
}

bool FrameSelection::isFocusedAndActive() const {
  return m_focused && m_frame->page() &&
         m_frame->page()->focusController().isActive();
}

bool FrameSelection::isAppearanceDirty() const {
  return m_pendingSelection->hasPendingSelection();
}

void FrameSelection::commitAppearanceIfNeeded(LayoutView& layoutView) {
  return m_pendingSelection->commit(layoutView);
}

void FrameSelection::didLayout() {
  updateAppearance();
}

void FrameSelection::updateAppearance() {
  DCHECK(!m_frame->contentLayoutItem().isNull());
  m_frameCaret->scheduleVisualUpdateForPaintInvalidationIfNeeded();
  m_pendingSelection->setHasPendingSelection();
}

void FrameSelection::notifyLayoutObjectOfSelectionChange(
    EUserTriggered userTriggered) {
  TextControlElement* textControl =
      enclosingTextControl(selectionInDOMTree().base());
  if (!textControl)
    return;
  textControl->selectionChanged(userTriggered == UserTriggered);
}

// Helper function that tells whether a particular node is an element that has
// an entire LocalFrame and FrameView, a <frame>, <iframe>, or <object>.
static bool isFrameElement(const Node* n) {
  if (!n)
    return false;
  LayoutObject* layoutObject = n->layoutObject();
  if (!layoutObject || !layoutObject->isLayoutPart())
    return false;
  FrameViewBase* frameViewBase = toLayoutPart(layoutObject)->frameViewBase();
  return frameViewBase && frameViewBase->isFrameView();
}

void FrameSelection::setFocusedNodeIfNeeded() {
  if (computeVisibleSelectionInDOMTreeDeprecated().isNone() || !isFocused())
    return;

  if (Element* target =
          computeVisibleSelectionInDOMTreeDeprecated().rootEditableElement()) {
    // Walk up the DOM tree to search for a node to focus.
    document().updateStyleAndLayoutTreeIgnorePendingStylesheets();
    while (target) {
      // We don't want to set focus on a subframe when selecting in a parent
      // frame, so add the !isFrameElement check here. There's probably a better
      // way to make this work in the long term, but this is the safest fix at
      // this time.
      if (target->isMouseFocusable() && !isFrameElement(target)) {
        m_frame->page()->focusController().setFocusedElement(target, m_frame);
        return;
      }
      target = target->parentOrShadowHostElement();
    }
    document().clearFocusedElement();
  }
}

static String extractSelectedText(const FrameSelection& selection,
                                  TextIteratorBehavior behavior) {
  const VisibleSelectionInFlatTree& visibleSelection =
      selection.computeVisibleSelectionInFlatTree();
  const EphemeralRangeInFlatTree& range =
      visibleSelection.toNormalizedEphemeralRange();
  // We remove '\0' characters because they are not visibly rendered to the
  // user.
  return plainText(range, behavior).replace(0, "");
}

String FrameSelection::selectedHTMLForClipboard() const {
  const VisibleSelectionInFlatTree& visibleSelection =
      computeVisibleSelectionInFlatTree();
  const EphemeralRangeInFlatTree& range =
      visibleSelection.toNormalizedEphemeralRange();
  return createMarkup(range.startPosition(), range.endPosition(),
                      AnnotateForInterchange,
                      ConvertBlocksToInlines::NotConvert, ResolveNonLocalURLs);
}

String FrameSelection::selectedText(
    const TextIteratorBehavior& behavior) const {
  return extractSelectedText(*this, behavior);
}

String FrameSelection::selectedText() const {
  return selectedText(TextIteratorBehavior());
}

String FrameSelection::selectedTextForClipboard() const {
  return extractSelectedText(
      *this, TextIteratorBehavior::Builder()
                 .setEmitsImageAltText(
                     m_frame->settings() &&
                     m_frame->settings()->getSelectionIncludesAltImageText())
                 .build());
}

LayoutRect FrameSelection::bounds() const {
  FrameView* view = m_frame->view();
  if (!view)
    return LayoutRect();

  return intersection(unclippedBounds(),
                      LayoutRect(view->visibleContentRect()));
}

LayoutRect FrameSelection::unclippedBounds() const {
  FrameView* view = m_frame->view();
  LayoutViewItem layoutView = m_frame->contentLayoutItem();

  if (!view || layoutView.isNull())
    return LayoutRect();

  view->updateLifecycleToLayoutClean();
  return LayoutRect(layoutView.selectionBounds());
}

static inline HTMLFormElement* associatedFormElement(HTMLElement& element) {
  if (isHTMLFormElement(element))
    return &toHTMLFormElement(element);
  return element.formOwner();
}

// Scans logically forward from "start", including any child frames.
static HTMLFormElement* scanForForm(Node* start) {
  if (!start)
    return 0;

  for (HTMLElement& element : Traversal<HTMLElement>::startsAt(
           start->isHTMLElement() ? toHTMLElement(start)
                                  : Traversal<HTMLElement>::next(*start))) {
    if (HTMLFormElement* form = associatedFormElement(element))
      return form;

    if (isHTMLFrameElementBase(element)) {
      Node* childDocument = toHTMLFrameElementBase(element).contentDocument();
      if (HTMLFormElement* frameResult = scanForForm(childDocument))
        return frameResult;
    }
  }
  return 0;
}

// We look for either the form containing the current focus, or for one
// immediately after it
HTMLFormElement* FrameSelection::currentForm() const {
  // Start looking either at the active (first responder) node, or where the
  // selection is.
  Node* start = document().focusedElement();
  if (!start)
    start = computeVisibleSelectionInDOMTreeDeprecated().start().anchorNode();
  if (!start)
    return 0;

  // Try walking up the node tree to find a form element.
  for (HTMLElement* element =
           Traversal<HTMLElement>::firstAncestorOrSelf(*start);
       element; element = Traversal<HTMLElement>::firstAncestor(*element)) {
    if (HTMLFormElement* form = associatedFormElement(*element))
      return form;
  }

  // Try walking forward in the node tree to find a form element.
  return scanForForm(start);
}

void FrameSelection::revealSelection(const ScrollAlignment& alignment,
                                     RevealExtentOption revealExtentOption) {
  DCHECK(isAvailable());

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // Calculation of absolute caret bounds requires clean layout.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  LayoutRect rect;

  switch (computeVisibleSelectionInDOMTree().getSelectionType()) {
    case NoSelection:
      return;
    case CaretSelection:
      rect = LayoutRect(absoluteCaretBounds());
      break;
    case RangeSelection:
      rect = LayoutRect(revealExtentOption == RevealExtent
                            ? absoluteCaretBoundsOf(createVisiblePosition(
                                  computeVisibleSelectionInDOMTree().extent()))
                            : enclosingIntRect(unclippedBounds()));
      break;
  }

  Position start = computeVisibleSelectionInDOMTreeDeprecated().start();
  DCHECK(start.anchorNode());
  if (start.anchorNode() && start.anchorNode()->layoutObject()) {
    // FIXME: This code only handles scrolling the startContainer's layer, but
    // the selection rect could intersect more than just that.
    if (DocumentLoader* documentLoader = m_frame->loader().documentLoader())
      documentLoader->initialScrollState().wasScrolledByUser = true;
    if (start.anchorNode()->layoutObject()->scrollRectToVisible(rect, alignment,
                                                                alignment))
      updateAppearance();
  }
}

void FrameSelection::setSelectionFromNone() {
  // Put a caret inside the body if the entire frame is editable (either the
  // entire WebView is editable or designMode is on for this document).

  Document* document = m_frame->document();
  if (!computeVisibleSelectionInDOMTreeDeprecated().isNone() ||
      !(blink::hasEditableStyle(*document)))
    return;

  Element* documentElement = document->documentElement();
  if (!documentElement)
    return;
  if (HTMLBodyElement* body =
          Traversal<HTMLBodyElement>::firstChild(*documentElement)) {
    setSelection(SelectionInDOMTree::Builder()
                     .collapse(firstPositionInOrBeforeNode(body))
                     .build());
  }
}

// TODO(yoichio): We should have LocalFrame having FrameCaret,
// Editor and PendingSelection using FrameCaret directly
// and get rid of this.
bool FrameSelection::shouldShowBlockCursor() const {
  return m_frameCaret->shouldShowBlockCursor();
}

// TODO(yoichio): We should have LocalFrame having FrameCaret,
// Editor and PendingSelection using FrameCaret directly
// and get rid of this.
// TODO(yoichio): We should use "caret-shape" in "CSS Basic User Interface
// Module Level 4" https://drafts.csswg.org/css-ui-4/
// To use "caret-shape", we need to expose inserting mode information to CSS;
// https://github.com/w3c/csswg-drafts/issues/133
void FrameSelection::setShouldShowBlockCursor(bool shouldShowBlockCursor) {
  m_frameCaret->setShouldShowBlockCursor(shouldShowBlockCursor);
}

#ifndef NDEBUG

void FrameSelection::showTreeForThis() const {
  computeVisibleSelectionInDOMTreeDeprecated().showTreeForThis();
}

#endif

DEFINE_TRACE(FrameSelection) {
  visitor->trace(m_frame);
  visitor->trace(m_pendingSelection);
  visitor->trace(m_selectionEditor);
  visitor->trace(m_frameCaret);
  SynchronousMutationObserver::trace(visitor);
}

void FrameSelection::scheduleVisualUpdate() const {
  if (Page* page = m_frame->page())
    page->animator().scheduleVisualUpdate(m_frame->localFrameRoot());
}

void FrameSelection::scheduleVisualUpdateForPaintInvalidationIfNeeded() const {
  if (FrameView* frameView = m_frame->view())
    frameView->scheduleVisualUpdateForPaintInvalidationIfNeeded();
}

bool FrameSelection::selectWordAroundPosition(const VisiblePosition& position) {
  static const EWordSide wordSideList[2] = {RightWordIfOnBoundary,
                                            LeftWordIfOnBoundary};
  for (EWordSide wordSide : wordSideList) {
    // TODO(yoichio): We should have Position version of |start/endOfWord|
    // for avoiding unnecessary canonicalization.
    VisiblePosition start = startOfWord(position, wordSide);
    VisiblePosition end = endOfWord(position, wordSide);
    String text =
        plainText(EphemeralRange(start.deepEquivalent(), end.deepEquivalent()));
    if (!text.isEmpty() && !isSeparator(text.characterStartingAt(0))) {
      setSelection(SelectionInDOMTree::Builder()
                       .collapse(start.toPositionWithAffinity())
                       .extend(end.deepEquivalent())
                       .build(),
                   CloseTyping | ClearTypingStyle,
                   CursorAlignOnScroll::IfNeeded, WordGranularity);
      return true;
    }
  }

  return false;
}

GranularityStrategy* FrameSelection::granularityStrategy() {
  // We do lazy initalization for m_granularityStrategy, because if we
  // initialize it right in the constructor - the correct settings may not be
  // set yet.
  SelectionStrategy strategyType = SelectionStrategy::Character;
  Settings* settings = m_frame ? m_frame->settings() : 0;
  if (settings &&
      settings->getSelectionStrategy() == SelectionStrategy::Direction)
    strategyType = SelectionStrategy::Direction;

  if (m_granularityStrategy && m_granularityStrategy->GetType() == strategyType)
    return m_granularityStrategy.get();

  if (strategyType == SelectionStrategy::Direction)
    m_granularityStrategy = WTF::makeUnique<DirectionGranularityStrategy>();
  else
    m_granularityStrategy = WTF::makeUnique<CharacterGranularityStrategy>();
  return m_granularityStrategy.get();
}

void FrameSelection::moveRangeSelectionExtent(const IntPoint& contentsPoint) {
  if (computeVisibleSelectionInDOMTreeDeprecated().isNone())
    return;

  const SetSelectionOptions options =
      FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle |
      FrameSelection::DoNotClearStrategy | UserTriggered;
  setSelection(SelectionInDOMTree::Builder(
                   granularityStrategy()->updateExtent(contentsPoint, m_frame))
                   .setIsHandleVisible(true)
                   .build(),
               options);
}

// TODO(yosin): We should make |FrameSelection::moveRangeSelection()| to take
// two |IntPoint| instead of two |VisiblePosition| like
// |moveRangeSelectionExtent()|.
void FrameSelection::moveRangeSelection(const VisiblePosition& basePosition,
                                        const VisiblePosition& extentPosition,
                                        TextGranularity granularity) {
  SelectionInDOMTree newSelection =
      SelectionInDOMTree::Builder()
          .setBaseAndExtentDeprecated(basePosition.deepEquivalent(),
                                      extentPosition.deepEquivalent())
          .setAffinity(basePosition.affinity())
          .setGranularity(granularity)
          .setIsHandleVisible(isHandleVisible())
          .build();

  if (newSelection.isNone())
    return;

  setSelection(newSelection, CloseTyping | ClearTypingStyle,
               CursorAlignOnScroll::IfNeeded, granularity);
}

void FrameSelection::setCaretVisible(bool caretIsVisible) {
  m_frameCaret->setCaretVisibility(caretIsVisible ? CaretVisibility::Visible
                                                  : CaretVisibility::Hidden);
}

void FrameSelection::setCaretBlinkingSuspended(bool suspended) {
  m_frameCaret->setCaretBlinkingSuspended(suspended);
}

bool FrameSelection::isCaretBlinkingSuspended() const {
  return m_frameCaret->isCaretBlinkingSuspended();
}

void FrameSelection::cacheRangeOfDocument(Range* range) {
  m_selectionEditor->cacheRangeOfDocument(range);
}

Range* FrameSelection::documentCachedRange() const {
  return m_selectionEditor->documentCachedRange();
}

void FrameSelection::clearDocumentCachedRange() {
  m_selectionEditor->clearDocumentCachedRange();
}

}  // namespace blink

#ifndef NDEBUG

void showTree(const blink::FrameSelection& sel) {
  sel.showTreeForThis();
}

void showTree(const blink::FrameSelection* sel) {
  if (sel)
    sel->showTreeForThis();
  else
    LOG(INFO) << "Cannot showTree for <null> FrameSelection.";
}

#endif
