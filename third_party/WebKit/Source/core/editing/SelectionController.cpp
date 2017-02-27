/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
 * Copyright (C) 2015 Google Inc. All rights reserved.
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

#include "core/editing/SelectionController.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/RenderedPosition.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/AutoReset.h"

namespace blink {
SelectionController* SelectionController::create(LocalFrame& frame) {
  return new SelectionController(frame);
}

SelectionController::SelectionController(LocalFrame& frame)
    : m_frame(&frame),
      m_mouseDownMayStartSelect(false),
      m_mouseDownWasSingleClickInSelection(false),
      m_mouseDownAllowsMultiClick(false),
      m_selectionState(SelectionState::HaveNotStartedSelection) {}

DEFINE_TRACE(SelectionController) {
  visitor->trace(m_frame);
  visitor->trace(m_originalBaseInFlatTree);
  SynchronousMutationObserver::trace(visitor);
}

namespace {

DispatchEventResult dispatchSelectStart(Node* node) {
  if (!node || !node->layoutObject())
    return DispatchEventResult::NotCanceled;

  return node->dispatchEvent(
      Event::createCancelableBubble(EventTypeNames::selectstart));
}

VisibleSelectionInFlatTree expandSelectionToRespectUserSelectAll(
    Node* targetNode,
    const VisibleSelectionInFlatTree& selection) {
  Node* const rootUserSelectAll =
      EditingInFlatTreeStrategy::rootUserSelectAllForNode(targetNode);
  if (!rootUserSelectAll)
    return selection;

  return createVisibleSelection(
      SelectionInFlatTree::Builder(selection.asSelection())
          .collapse(mostBackwardCaretPosition(
              PositionInFlatTree::beforeNode(rootUserSelectAll),
              CanCrossEditingBoundary))
          .extend(mostForwardCaretPosition(
              PositionInFlatTree::afterNode(rootUserSelectAll),
              CanCrossEditingBoundary))
          .build());
}

static int textDistance(const PositionInFlatTree& start,
                        const PositionInFlatTree& end) {
  return TextIteratorInFlatTree::rangeLength(start, end, true);
}

bool canMouseDownStartSelect(Node* node) {
  if (!node || !node->layoutObject())
    return true;

  if (!node->canStartSelection())
    return false;

  return true;
}

VisiblePositionInFlatTree visiblePositionOfHitTestResult(
    const HitTestResult& hitTestResult) {
  return createVisiblePosition(fromPositionInDOMTree<EditingInFlatTreeStrategy>(
      hitTestResult.innerNode()->layoutObject()->positionForPoint(
          hitTestResult.localPoint())));
}

}  // namespace

SelectionController::~SelectionController() = default;

Document& SelectionController::document() const {
  DCHECK(m_frame->document());
  return *m_frame->document();
}

void SelectionController::contextDestroyed(Document*) {
  m_originalBaseInFlatTree = VisiblePositionInFlatTree();
}

static PositionInFlatTree adjustPositionRespectUserSelectAll(
    Node* innerNode,
    const PositionInFlatTree& selectionStart,
    const PositionInFlatTree& selectionEnd,
    const PositionInFlatTree& position) {
  const VisibleSelectionInFlatTree& selectionInUserSelectAll =
      expandSelectionToRespectUserSelectAll(
          innerNode,
          position.isNull()
              ? VisibleSelectionInFlatTree()
              : createVisibleSelection(
                    SelectionInFlatTree::Builder().collapse(position).build()));
  if (!selectionInUserSelectAll.isRange())
    return position;
  if (selectionInUserSelectAll.start().compareTo(selectionStart) < 0)
    return selectionInUserSelectAll.start();
  if (selectionEnd.compareTo(selectionInUserSelectAll.end()) < 0)
    return selectionInUserSelectAll.end();
  return position;
}

// Updating the selection is considered side-effect of the event and so it
// doesn't impact the handled state.
bool SelectionController::handleMousePressEventSingleClick(
    const MouseEventWithHitTestResults& event) {
  TRACE_EVENT0("blink",
               "SelectionController::handleMousePressEventSingleClick");

  DCHECK(!m_frame->document()->needsLayoutTreeUpdate());
  Node* innerNode = event.innerNode();
  if (!(innerNode && innerNode->layoutObject() && m_mouseDownMayStartSelect))
    return false;

  // Extend the selection if the Shift key is down, unless the click is in a
  // link or image.
  bool extendSelection = isExtendingSelection(event);

  const VisiblePositionInFlatTree& visibleHitPos =
      visiblePositionOfHitTestResult(event.hitTestResult());
  const VisiblePositionInFlatTree& visiblePos =
      visibleHitPos.isNull()
          ? createVisiblePosition(
                PositionInFlatTree::firstPositionInOrBeforeNode(innerNode))
          : visibleHitPos;
  const VisibleSelectionInFlatTree& selection =
      this->selection().computeVisibleSelectionInFlatTree();

  // Don't restart the selection when the mouse is pressed on an
  // existing selection so we can allow for text dragging.
  if (FrameView* view = m_frame->view()) {
    const LayoutPoint vPoint = view->rootFrameToContents(
        flooredIntPoint(event.event().positionInRootFrame()));
    if (!extendSelection && this->selection().contains(vPoint)) {
      m_mouseDownWasSingleClickInSelection = true;
      if (!event.event().fromTouch())
        return false;

      if (!this->selection().isHandleVisible()) {
        updateSelectionForMouseDownDispatchingSelectStart(
            innerNode, selection, CharacterGranularity,
            HandleVisibility::Visible);
        return false;
      }
    }
  }

  if (extendSelection && !selection.isNone()) {
    // Note: "fast/events/shift-click-user-select-none.html" makes
    // |pos.isNull()| true.
    const PositionInFlatTree& pos = adjustPositionRespectUserSelectAll(
        innerNode, selection.start(), selection.end(),
        visiblePos.deepEquivalent());
    SelectionInFlatTree::Builder builder;
    builder.setGranularity(this->selection().granularity());
    if (m_frame->editor().behavior().shouldConsiderSelectionAsDirectional()) {
      builder.setBaseAndExtent(selection.base(), pos);
    } else if (pos.isNull()) {
      builder.setBaseAndExtent(selection.base(), selection.extent());
    } else {
      // Shift+Click deselects when selection was created right-to-left
      const PositionInFlatTree& start = selection.start();
      const PositionInFlatTree& end = selection.end();
      const int distanceToStart = textDistance(start, pos);
      const int distanceToEnd = textDistance(pos, end);
      builder.setBaseAndExtent(distanceToStart <= distanceToEnd ? end : start,
                               pos);
    }

    updateSelectionForMouseDownDispatchingSelectStart(
        innerNode, createVisibleSelection(builder.build()),
        this->selection().granularity(), HandleVisibility::NotVisible);
    return false;
  }

  if (m_selectionState == SelectionState::ExtendedSelection) {
    updateSelectionForMouseDownDispatchingSelectStart(
        innerNode, selection, CharacterGranularity,
        HandleVisibility::NotVisible);
    return false;
  }

  if (visiblePos.isNull()) {
    updateSelectionForMouseDownDispatchingSelectStart(
        innerNode, VisibleSelectionInFlatTree(), CharacterGranularity,
        HandleVisibility::NotVisible);
    return false;
  }

  bool isHandleVisible = false;
  if (hasEditableStyle(*innerNode)) {
    const bool isTextBoxEmpty =
        createVisibleSelection(SelectionInFlatTree::Builder()
                                   .selectAllChildren(*innerNode)
                                   .build())
            .isCaret();
    const bool notLeftClick =
        event.event().button != WebPointerProperties::Button::Left;
    if (!isTextBoxEmpty || notLeftClick)
      isHandleVisible = event.event().fromTouch();
  }

  updateSelectionForMouseDownDispatchingSelectStart(
      innerNode,
      expandSelectionToRespectUserSelectAll(
          innerNode, createVisibleSelection(
                         SelectionInFlatTree::Builder()
                             .collapse(visiblePos.toPositionWithAffinity())
                             .build())),
      CharacterGranularity, isHandleVisible ? HandleVisibility::Visible
                                            : HandleVisibility::NotVisible);
  return false;
}

static bool targetPositionIsBeforeDragStartPosition(
    Node* dragStartNode,
    const LayoutPoint& dragStartPoint,
    Node* target,
    const LayoutPoint& hitTestPoint) {
  const PositionInFlatTree& targetPosition = toPositionInFlatTree(
      target->layoutObject()->positionForPoint(hitTestPoint).position());
  const PositionInFlatTree& dragStartPosition =
      toPositionInFlatTree(dragStartNode->layoutObject()
                               ->positionForPoint(dragStartPoint)
                               .position());

  return targetPosition.compareTo(dragStartPosition) < 0;
}

static SelectionInFlatTree applySelectAll(
    const PositionInFlatTree& basePosition,
    const PositionInFlatTree& targetPosition,
    Node* mousePressNode,
    const LayoutPoint& dragStartPoint,
    Node* target,
    const LayoutPoint& hitTestPoint) {
  Node* const rootUserSelectAllForMousePressNode =
      EditingInFlatTreeStrategy::rootUserSelectAllForNode(mousePressNode);
  Node* const rootUserSelectAllForTarget =
      EditingInFlatTreeStrategy::rootUserSelectAllForNode(target);

  if (rootUserSelectAllForMousePressNode &&
      rootUserSelectAllForMousePressNode == rootUserSelectAllForTarget) {
    return SelectionInFlatTree::Builder()
        .setBaseAndExtent(
            PositionInFlatTree::beforeNode(rootUserSelectAllForMousePressNode),
            PositionInFlatTree::afterNode(rootUserSelectAllForMousePressNode))
        .build();
  }

  SelectionInFlatTree::Builder builder;
  // Reset base for user select all when base is inside user-select-all area
  // and extent < base.
  if (rootUserSelectAllForMousePressNode &&
      targetPositionIsBeforeDragStartPosition(mousePressNode, dragStartPoint,
                                              target, hitTestPoint)) {
    builder.collapse(
        PositionInFlatTree::afterNode(rootUserSelectAllForMousePressNode));
  } else {
    builder.collapse(basePosition);
  }

  if (rootUserSelectAllForTarget && mousePressNode->layoutObject()) {
    if (targetPositionIsBeforeDragStartPosition(mousePressNode, dragStartPoint,
                                                target, hitTestPoint)) {
      builder.extend(
          PositionInFlatTree::beforeNode(rootUserSelectAllForTarget));
      return builder.build();
    }

    builder.extend(PositionInFlatTree::afterNode(rootUserSelectAllForTarget));
    return builder.build();
  }

  builder.extend(targetPosition);
  return builder.build();
}

void SelectionController::updateSelectionForMouseDrag(
    const HitTestResult& hitTestResult,
    Node* mousePressNode,
    const LayoutPoint& dragStartPos,
    const IntPoint& lastKnownMousePosition) {
  if (!m_mouseDownMayStartSelect)
    return;

  Node* target = hitTestResult.innerNode();
  if (!target)
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  const PositionWithAffinity& rawTargetPosition =
      positionRespectingEditingBoundary(
          selection().computeVisibleSelectionInDOMTreeDeprecated().start(),
          hitTestResult.localPoint(), target);
  VisiblePositionInFlatTree targetPosition = createVisiblePosition(
      fromPositionInDOMTree<EditingInFlatTreeStrategy>(rawTargetPosition));
  // Don't modify the selection if we're not on a node.
  if (targetPosition.isNull())
    return;

  // Restart the selection if this is the first mouse move. This work is usually
  // done in handleMousePressEvent, but not if the mouse press was on an
  // existing selection.

  // Special case to limit selection to the containing block for SVG text.
  // FIXME: Isn't there a better non-SVG-specific way to do this?
  if (Node* selectionBaseNode =
          selection().computeVisibleSelectionInFlatTree().base().anchorNode()) {
    if (LayoutObject* selectionBaseLayoutObject =
            selectionBaseNode->layoutObject()) {
      if (selectionBaseLayoutObject->isSVGText()) {
        if (target->layoutObject()->containingBlock() !=
            selectionBaseLayoutObject->containingBlock())
          return;
      }
    }
  }

  if (m_selectionState == SelectionState::HaveNotStartedSelection &&
      dispatchSelectStart(target) != DispatchEventResult::NotCanceled)
    return;

  // TODO(yosin) We should check |mousePressNode|, |targetPosition|, and
  // |newSelection| are valid for |m_frame->document()|.
  // |dispatchSelectStart()| can change them by "selectstart" event handler.

  PositionInFlatTree basePosition;
  if (m_selectionState != SelectionState::ExtendedSelection) {
    // Always extend selection here because it's caused by a mouse drag
    m_selectionState = SelectionState::ExtendedSelection;
    basePosition = targetPosition.deepEquivalent();
  } else {
    basePosition = selection().computeVisibleSelectionInFlatTree().base();
  }
  const SelectionInFlatTree& appliedSelection = applySelectAll(
      basePosition, targetPosition.deepEquivalent(), mousePressNode,
      dragStartPos, target, hitTestResult.localPoint());
  SelectionInFlatTree::Builder builder(appliedSelection);

  if (selection().granularity() != CharacterGranularity)
    builder.setGranularity(selection().granularity());

  setNonDirectionalSelectionIfNeeded(
      createVisibleSelection(builder.build()), selection().granularity(),
      AdjustEndpointsAtBidiBoundary, HandleVisibility::NotVisible);
}

bool SelectionController::updateSelectionForMouseDownDispatchingSelectStart(
    Node* targetNode,
    const VisibleSelectionInFlatTree& selection,
    TextGranularity granularity,
    HandleVisibility handleVisibility) {
  if (targetNode && targetNode->layoutObject() &&
      !targetNode->layoutObject()->isSelectable())
    return false;

  if (dispatchSelectStart(targetNode) != DispatchEventResult::NotCanceled)
    return false;

  // |dispatchSelectStart()| can change document hosted by |m_frame|.
  if (!this->selection().isAvailable())
    return false;

  if (!selection.isValidFor(this->selection().document()))
    return false;

  if (selection.isRange()) {
    m_selectionState = SelectionState::ExtendedSelection;
  } else {
    granularity = CharacterGranularity;
    m_selectionState = SelectionState::PlacedCaret;
  }

  setNonDirectionalSelectionIfNeeded(selection, granularity,
                                     DoNotAdjustEndpoints, handleVisibility);

  return true;
}

bool SelectionController::selectClosestWordFromHitTestResult(
    const HitTestResult& result,
    AppendTrailingWhitespace appendTrailingWhitespace,
    SelectInputEventType selectInputEventType) {
  Node* innerNode = result.innerNode();
  VisibleSelectionInFlatTree newSelection;

  if (!innerNode || !innerNode->layoutObject())
    return false;

  // Special-case image local offset to always be zero, to avoid triggering
  // LayoutReplaced::positionFromPoint's advancement of the position at the
  // mid-point of the the image (which was intended for mouse-drag selection
  // and isn't desirable for touch).
  HitTestResult adjustedHitTestResult = result;
  if (selectInputEventType == SelectInputEventType::Touch && result.image())
    adjustedHitTestResult.setNodeAndPosition(result.innerNode(),
                                             LayoutPoint(0, 0));

  const VisiblePositionInFlatTree& pos =
      visiblePositionOfHitTestResult(adjustedHitTestResult);
  if (pos.isNotNull()) {
    newSelection =
        createVisibleSelection(SelectionInFlatTree::Builder()
                                   .collapse(pos.toPositionWithAffinity())
                                   .setGranularity(WordGranularity)
                                   .build());
  }

  HandleVisibility visibility = HandleVisibility::NotVisible;
  if (selectInputEventType == SelectInputEventType::Touch) {
    // If node doesn't have text except space, tab or line break, do not
    // select that 'empty' area.
    EphemeralRangeInFlatTree range(newSelection.start(), newSelection.end());
    const String& str = plainText(
        range,
        TextIteratorBehavior::Builder()
            .setEmitsObjectReplacementCharacter(hasEditableStyle(*innerNode))
            .build());
    if (str.isEmpty() || str.simplifyWhiteSpace().containsOnlyWhitespace())
      return false;

    if (newSelection.rootEditableElement() &&
        pos.deepEquivalent() ==
            VisiblePositionInFlatTree::lastPositionInNode(
                newSelection.rootEditableElement())
                .deepEquivalent())
      return false;

    visibility = HandleVisibility::Visible;
  }

  if (appendTrailingWhitespace == AppendTrailingWhitespace::ShouldAppend)
    newSelection.appendTrailingWhitespace();

  return updateSelectionForMouseDownDispatchingSelectStart(
      innerNode, expandSelectionToRespectUserSelectAll(innerNode, newSelection),
      WordGranularity, visibility);
}

void SelectionController::selectClosestMisspellingFromHitTestResult(
    const HitTestResult& result,
    AppendTrailingWhitespace appendTrailingWhitespace) {
  Node* innerNode = result.innerNode();
  VisibleSelectionInFlatTree newSelection;

  if (!innerNode || !innerNode->layoutObject())
    return;

  const VisiblePositionInFlatTree& pos = visiblePositionOfHitTestResult(result);
  if (pos.isNotNull()) {
    const PositionInFlatTree& markerPosition =
        pos.deepEquivalent().parentAnchoredEquivalent();
    DocumentMarkerVector markers =
        innerNode->document().markers().markersInRange(
            EphemeralRange(toPositionInDOMTree(markerPosition)),
            DocumentMarker::MisspellingMarkers());
    if (markers.size() == 1) {
      Node* containerNode = markerPosition.computeContainerNode();
      const PositionInFlatTree start(containerNode, markers[0]->startOffset());
      const PositionInFlatTree end(containerNode, markers[0]->endOffset());
      newSelection = createVisibleSelection(
          SelectionInFlatTree::Builder().collapse(start).extend(end).build());
    }
  }

  if (appendTrailingWhitespace == AppendTrailingWhitespace::ShouldAppend)
    newSelection.appendTrailingWhitespace();

  updateSelectionForMouseDownDispatchingSelectStart(
      innerNode, expandSelectionToRespectUserSelectAll(innerNode, newSelection),
      WordGranularity, HandleVisibility::NotVisible);
}

void SelectionController::selectClosestWordFromMouseEvent(
    const MouseEventWithHitTestResults& result) {
  if (!m_mouseDownMayStartSelect)
    return;

  AppendTrailingWhitespace appendTrailingWhitespace =
      (result.event().clickCount == 2 &&
       m_frame->editor().isSelectTrailingWhitespaceEnabled())
          ? AppendTrailingWhitespace::ShouldAppend
          : AppendTrailingWhitespace::DontAppend;

  DCHECK(!m_frame->document()->needsLayoutTreeUpdate());

  selectClosestWordFromHitTestResult(
      result.hitTestResult(), appendTrailingWhitespace,
      result.event().fromTouch() ? SelectInputEventType::Touch
                                 : SelectInputEventType::Mouse);
}

void SelectionController::selectClosestMisspellingFromMouseEvent(
    const MouseEventWithHitTestResults& result) {
  if (!m_mouseDownMayStartSelect)
    return;

  selectClosestMisspellingFromHitTestResult(
      result.hitTestResult(),
      (result.event().clickCount == 2 &&
       m_frame->editor().isSelectTrailingWhitespaceEnabled())
          ? AppendTrailingWhitespace::ShouldAppend
          : AppendTrailingWhitespace::DontAppend);
}

void SelectionController::selectClosestWordOrLinkFromMouseEvent(
    const MouseEventWithHitTestResults& result) {
  if (!result.hitTestResult().isLiveLink())
    return selectClosestWordFromMouseEvent(result);

  Node* innerNode = result.innerNode();

  if (!innerNode || !innerNode->layoutObject() || !m_mouseDownMayStartSelect)
    return;

  VisibleSelectionInFlatTree newSelection;
  Element* URLElement = result.hitTestResult().URLElement();
  const VisiblePositionInFlatTree pos =
      visiblePositionOfHitTestResult(result.hitTestResult());
  if (pos.isNotNull() &&
      pos.deepEquivalent().anchorNode()->isDescendantOf(URLElement)) {
    newSelection = createVisibleSelection(
        SelectionInFlatTree::Builder().selectAllChildren(*URLElement).build());
  }

  updateSelectionForMouseDownDispatchingSelectStart(
      innerNode, expandSelectionToRespectUserSelectAll(innerNode, newSelection),
      WordGranularity, HandleVisibility::NotVisible);
}

// TODO(xiaochengh): We should not use reference to return value.
static void adjustEndpointsAtBidiBoundary(
    VisiblePositionInFlatTree& visibleBase,
    VisiblePositionInFlatTree& visibleExtent) {
  DCHECK(visibleBase.isValid());
  DCHECK(visibleExtent.isValid());

  RenderedPosition base(visibleBase);
  RenderedPosition extent(visibleExtent);

  if (base.isNull() || extent.isNull() || base.isEquivalent(extent))
    return;

  if (base.atLeftBoundaryOfBidiRun()) {
    if (!extent.atRightBoundaryOfBidiRun(base.bidiLevelOnRight()) &&
        base.isEquivalent(
            extent.leftBoundaryOfBidiRun(base.bidiLevelOnRight()))) {
      visibleBase = createVisiblePosition(
          toPositionInFlatTree(base.positionAtLeftBoundaryOfBiDiRun()));
      return;
    }
    return;
  }

  if (base.atRightBoundaryOfBidiRun()) {
    if (!extent.atLeftBoundaryOfBidiRun(base.bidiLevelOnLeft()) &&
        base.isEquivalent(
            extent.rightBoundaryOfBidiRun(base.bidiLevelOnLeft()))) {
      visibleBase = createVisiblePosition(
          toPositionInFlatTree(base.positionAtRightBoundaryOfBiDiRun()));
      return;
    }
    return;
  }

  if (extent.atLeftBoundaryOfBidiRun() &&
      extent.isEquivalent(
          base.leftBoundaryOfBidiRun(extent.bidiLevelOnRight()))) {
    visibleExtent = createVisiblePosition(
        toPositionInFlatTree(extent.positionAtLeftBoundaryOfBiDiRun()));
    return;
  }

  if (extent.atRightBoundaryOfBidiRun() &&
      extent.isEquivalent(
          base.rightBoundaryOfBidiRun(extent.bidiLevelOnLeft()))) {
    visibleExtent = createVisiblePosition(
        toPositionInFlatTree(extent.positionAtRightBoundaryOfBiDiRun()));
    return;
  }
}

// TODO(yosin): We should make |setNonDirectionalSelectionIfNeeded()| to take
// |SelectionInFlatTree| instead of |VisibleSelectionInFlatTree|.
void SelectionController::setNonDirectionalSelectionIfNeeded(
    const VisibleSelectionInFlatTree& newSelection,
    TextGranularity granularity,
    EndPointsAdjustmentMode endpointsAdjustmentMode,
    HandleVisibility handleVisibility) {
  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  const PositionInFlatTree& basePosition =
      m_originalBaseInFlatTree.deepEquivalent();
  const VisiblePositionInFlatTree& originalBase =
      basePosition.isConnected() ? createVisiblePosition(basePosition)
                                 : VisiblePositionInFlatTree();
  const VisiblePositionInFlatTree& base =
      originalBase.isNotNull() ? originalBase
                               : createVisiblePosition(newSelection.base());
  VisiblePositionInFlatTree newBase = base;
  const VisiblePositionInFlatTree& extent =
      createVisiblePosition(newSelection.extent());
  VisiblePositionInFlatTree newExtent = extent;
  if (endpointsAdjustmentMode == AdjustEndpointsAtBidiBoundary)
    adjustEndpointsAtBidiBoundary(newBase, newExtent);

  SelectionInFlatTree::Builder builder(newSelection.asSelection());
  if (newBase.deepEquivalent() != base.deepEquivalent() ||
      newExtent.deepEquivalent() != extent.deepEquivalent()) {
    m_originalBaseInFlatTree = base;
    setContext(&document());
    builder.setBaseAndExtent(newBase.deepEquivalent(),
                             newExtent.deepEquivalent());
  } else if (originalBase.isNotNull()) {
    if (selection().computeVisibleSelectionInFlatTree().base() ==
        newSelection.base()) {
      builder.setBaseAndExtent(originalBase.deepEquivalent(),
                               newSelection.extent());
    }
    m_originalBaseInFlatTree = VisiblePositionInFlatTree();
  }

  builder.setIsHandleVisible(handleVisibility == HandleVisibility::Visible)
      .setIsDirectional(
          m_frame->editor().behavior().shouldConsiderSelectionAsDirectional() ||
          newSelection.isDirectional());
  const SelectionInFlatTree& selectionInFlatTree = builder.build();
  if (selection().computeVisibleSelectionInFlatTree() ==
          createVisibleSelection(selectionInFlatTree) &&
      selection().isHandleVisible() == selectionInFlatTree.isHandleVisible())
    return;
  selection().setSelection(
      selectionInFlatTree,
      FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle,
      CursorAlignOnScroll::IfNeeded, granularity);
}

void SelectionController::setCaretAtHitTestResult(
    const HitTestResult& hitTestResult) {
  Node* innerNode = hitTestResult.innerNode();
  const VisiblePositionInFlatTree& visibleHitPos =
      visiblePositionOfHitTestResult(hitTestResult);
  const VisiblePositionInFlatTree& visiblePos =
      visibleHitPos.isNull()
          ? createVisiblePosition(
                PositionInFlatTree::firstPositionInOrBeforeNode(innerNode))
          : visibleHitPos;

  updateSelectionForMouseDownDispatchingSelectStart(
      innerNode,
      expandSelectionToRespectUserSelectAll(
          innerNode, createVisibleSelection(
                         SelectionInFlatTree::Builder()
                             .collapse(visiblePos.toPositionWithAffinity())
                             .build())),
      CharacterGranularity, HandleVisibility::Visible);
}

bool SelectionController::handleMousePressEventDoubleClick(
    const MouseEventWithHitTestResults& event) {
  TRACE_EVENT0("blink",
               "SelectionController::handleMousePressEventDoubleClick");

  if (!selection().isAvailable())
    return false;

  if (!m_mouseDownAllowsMultiClick)
    return handleMousePressEventSingleClick(event);

  if (event.event().button != WebPointerProperties::Button::Left)
    return false;

  if (selection().computeVisibleSelectionInDOMTreeDeprecated().isRange()) {
    // A double-click when range is already selected
    // should not change the selection.  So, do not call
    // selectClosestWordFromMouseEvent, but do set
    // m_beganSelectingText to prevent handleMouseReleaseEvent
    // from setting caret selection.
    m_selectionState = SelectionState::ExtendedSelection;
  } else {
    selectClosestWordFromMouseEvent(event);
  }
  return true;
}

bool SelectionController::handleMousePressEventTripleClick(
    const MouseEventWithHitTestResults& event) {
  TRACE_EVENT0("blink",
               "SelectionController::handleMousePressEventTripleClick");

  if (!selection().isAvailable()) {
    // editing/shadow/doubleclick-on-meter-in-shadow-crash.html reach here.
    return false;
  }

  if (!m_mouseDownAllowsMultiClick)
    return handleMousePressEventSingleClick(event);

  if (event.event().button != WebPointerProperties::Button::Left)
    return false;

  Node* innerNode = event.innerNode();
  if (!(innerNode && innerNode->layoutObject() && m_mouseDownMayStartSelect))
    return false;

  VisibleSelectionInFlatTree newSelection;
  const VisiblePositionInFlatTree& pos =
      visiblePositionOfHitTestResult(event.hitTestResult());
  if (pos.isNotNull()) {
    newSelection =
        createVisibleSelection(SelectionInFlatTree::Builder()
                                   .collapse(pos.toPositionWithAffinity())
                                   .setGranularity(ParagraphGranularity)
                                   .build());
  }

  const bool isHandleVisible =
      event.event().fromTouch() && newSelection.isRange();

  return updateSelectionForMouseDownDispatchingSelectStart(
      innerNode, expandSelectionToRespectUserSelectAll(innerNode, newSelection),
      ParagraphGranularity, isHandleVisible ? HandleVisibility::Visible
                                            : HandleVisibility::NotVisible);
}

void SelectionController::handleMousePressEvent(
    const MouseEventWithHitTestResults& event) {
  // If we got the event back, that must mean it wasn't prevented,
  // so it's allowed to start a drag or selection if it wasn't in a scrollbar.
  m_mouseDownMayStartSelect =
      (canMouseDownStartSelect(event.innerNode()) || isLinkSelection(event)) &&
      !event.scrollbar();
  m_mouseDownWasSingleClickInSelection = false;
  if (!selection().isAvailable()) {
    // "gesture-tap-frame-removed.html" reaches here.
    m_mouseDownAllowsMultiClick = !event.event().fromTouch();
    return;
  }

  // Avoid double-tap touch gesture confusion by restricting multi-click side
  // effects, e.g., word selection, to editable regions.
  m_mouseDownAllowsMultiClick =
      !event.event().fromTouch() ||
      selection()
          .computeVisibleSelectionInDOMTreeDeprecated()
          .hasEditableStyle();
}

void SelectionController::handleMouseDraggedEvent(
    const MouseEventWithHitTestResults& event,
    const IntPoint& mouseDownPos,
    const LayoutPoint& dragStartPos,
    Node* mousePressNode,
    const IntPoint& lastKnownMousePosition) {
  if (!selection().isAvailable())
    return;
  if (m_selectionState != SelectionState::ExtendedSelection) {
    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
    HitTestResult result(request, mouseDownPos);
    m_frame->document()->layoutViewItem().hitTest(result);

    updateSelectionForMouseDrag(result, mousePressNode, dragStartPos,
                                lastKnownMousePosition);
  }
  updateSelectionForMouseDrag(event.hitTestResult(), mousePressNode,
                              dragStartPos, lastKnownMousePosition);
}

void SelectionController::updateSelectionForMouseDrag(
    Node* mousePressNode,
    const LayoutPoint& dragStartPos,
    const IntPoint& lastKnownMousePosition) {
  FrameView* view = m_frame->view();
  if (!view)
    return;
  LayoutViewItem layoutItem = m_frame->contentLayoutItem();
  if (layoutItem.isNull())
    return;

  HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active |
                         HitTestRequest::Move);
  HitTestResult result(request,
                       view->rootFrameToContents(lastKnownMousePosition));
  layoutItem.hitTest(result);
  updateSelectionForMouseDrag(result, mousePressNode, dragStartPos,
                              lastKnownMousePosition);
}

bool SelectionController::handleMouseReleaseEvent(
    const MouseEventWithHitTestResults& event,
    const LayoutPoint& dragStartPos) {
  if (!selection().isAvailable())
    return false;

  bool handled = false;
  m_mouseDownMayStartSelect = false;
  // Clear the selection if the mouse didn't move after the last mouse
  // press and it's not a context menu click.  We do this so when clicking
  // on the selection, the selection goes away.  However, if we are
  // editing, place the caret.
  if (m_mouseDownWasSingleClickInSelection &&
      m_selectionState != SelectionState::ExtendedSelection &&
      dragStartPos == flooredIntPoint(event.event().positionInRootFrame()) &&
      selection().computeVisibleSelectionInDOMTreeDeprecated().isRange() &&
      event.event().button != WebPointerProperties::Button::Right) {
    // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    SelectionInFlatTree::Builder builder;
    Node* node = event.innerNode();
    if (node && node->layoutObject() && hasEditableStyle(*node)) {
      const VisiblePositionInFlatTree pos =
          visiblePositionOfHitTestResult(event.hitTestResult());
      if (pos.isNotNull())
        builder.collapse(pos.toPositionWithAffinity());
    }

    if (selection().computeVisibleSelectionInFlatTree() !=
        createVisibleSelection(builder.build())) {
      selection().setSelection(builder.build());
    }

    handled = true;
  }

  selection().notifyLayoutObjectOfSelectionChange(UserTriggered);

  selection().selectFrameElementInParentIfFullySelected();

  if (event.event().button == WebPointerProperties::Button::Middle &&
      !event.isOverLink()) {
    // Ignore handled, since we want to paste to where the caret was placed
    // anyway.
    handled = handlePasteGlobalSelection(event.event()) || handled;
  }

  return handled;
}

bool SelectionController::handlePasteGlobalSelection(
    const WebMouseEvent& mouseEvent) {
  // If the event was a middle click, attempt to copy global selection in after
  // the newly set caret position.
  //
  // This code is called from either the mouse up or mouse down handling. There
  // is some debate about when the global selection is pasted:
  //   xterm: pastes on up.
  //   GTK: pastes on down.
  //   Qt: pastes on up.
  //   Firefox: pastes on up.
  //   Chromium: pastes on up.
  //
  // There is something of a webcompat angle to this well, as highlighted by
  // crbug.com/14608. Pages can clear text boxes 'onclick' and, if we paste on
  // down then the text is pasted just before the onclick handler runs and
  // clears the text box. So it's important this happens after the event
  // handlers have been fired.
  if (mouseEvent.type() != WebInputEvent::MouseUp)
    return false;

  if (!m_frame->page())
    return false;
  Frame* focusFrame = m_frame->page()->focusController().focusedOrMainFrame();
  // Do not paste here if the focus was moved somewhere else.
  if (m_frame == focusFrame &&
      m_frame->editor().behavior().supportsGlobalSelection())
    return m_frame->editor().createCommand("PasteGlobalSelection").execute();

  return false;
}

bool SelectionController::handleGestureLongPress(
    const WebGestureEvent& gestureEvent,
    const HitTestResult& hitTestResult) {
  if (!selection().isAvailable())
    return false;
  if (hitTestResult.isLiveLink())
    return false;

  Node* innerNode = hitTestResult.innerNode();
  innerNode->document().updateStyleAndLayoutTree();
  bool innerNodeIsSelectable = hasEditableStyle(*innerNode) ||
                               innerNode->isTextNode() ||
                               innerNode->canStartSelection();
  if (!innerNodeIsSelectable)
    return false;

  if (selectClosestWordFromHitTestResult(hitTestResult,
                                         AppendTrailingWhitespace::DontAppend,
                                         SelectInputEventType::Touch))
    return selection().isAvailable();

  if (!innerNode->isConnected() || !innerNode->layoutObject())
    return false;
  setCaretAtHitTestResult(hitTestResult);
  return false;
}

void SelectionController::handleGestureTwoFingerTap(
    const GestureEventWithHitTestResults& targetedEvent) {
  setCaretAtHitTestResult(targetedEvent.hitTestResult());
}

void SelectionController::handleGestureLongTap(
    const GestureEventWithHitTestResults& targetedEvent) {
  setCaretAtHitTestResult(targetedEvent.hitTestResult());
}

static bool hitTestResultIsMisspelled(const HitTestResult& result) {
  Node* innerNode = result.innerNode();
  if (!innerNode || !innerNode->layoutObject())
    return false;
  VisiblePosition pos = createVisiblePosition(
      innerNode->layoutObject()->positionForPoint(result.localPoint()));
  if (pos.isNull())
    return false;
  return innerNode->document()
             .markers()
             .markersInRange(
                 EphemeralRange(
                     pos.deepEquivalent().parentAnchoredEquivalent()),
                 DocumentMarker::MisspellingMarkers())
             .size() > 0;
}

void SelectionController::sendContextMenuEvent(
    const MouseEventWithHitTestResults& mev,
    const LayoutPoint& position) {
  if (!selection().isAvailable())
    return;
  if (selection().contains(position) || mev.scrollbar() ||
      // FIXME: In the editable case, word selection sometimes selects content
      // that isn't underneath the mouse.
      // If the selection is non-editable, we do word selection to make it
      // easier to use the contextual menu items available for text selections.
      // But only if we're above text.
      !(selection()
            .computeVisibleSelectionInDOMTreeDeprecated()
            .isContentEditable() ||
        (mev.innerNode() && mev.innerNode()->isTextNode())))
    return;

  // Context menu events are always allowed to perform a selection.
  AutoReset<bool> mouseDownMayStartSelectChange(&m_mouseDownMayStartSelect,
                                                true);

  if (hitTestResultIsMisspelled(mev.hitTestResult()))
    return selectClosestMisspellingFromMouseEvent(mev);

  if (!m_frame->editor().behavior().shouldSelectOnContextualMenuClick())
    return;

  selectClosestWordOrLinkFromMouseEvent(mev);
}

void SelectionController::passMousePressEventToSubframe(
    const MouseEventWithHitTestResults& mev) {
  // If we're clicking into a frame that is selected, the frame will appear
  // greyed out even though we're clicking on the selection.  This looks
  // really strange (having the whole frame be greyed out), so we deselect the
  // selection.
  IntPoint p = m_frame->view()->rootFrameToContents(
      flooredIntPoint(mev.event().positionInRootFrame()));
  if (!selection().contains(p))
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  const VisiblePositionInFlatTree& visiblePos =
      visiblePositionOfHitTestResult(mev.hitTestResult());
  if (visiblePos.isNull()) {
    selection().setSelection(SelectionInFlatTree());
    return;
  }
  selection().setSelection(SelectionInFlatTree::Builder()
                               .collapse(visiblePos.toPositionWithAffinity())
                               .build());
}

void SelectionController::initializeSelectionState() {
  m_selectionState = SelectionState::HaveNotStartedSelection;
}

void SelectionController::setMouseDownMayStartSelect(bool mayStartSelect) {
  m_mouseDownMayStartSelect = mayStartSelect;
}

bool SelectionController::mouseDownMayStartSelect() const {
  return m_mouseDownMayStartSelect;
}

bool SelectionController::mouseDownWasSingleClickInSelection() const {
  return m_mouseDownWasSingleClickInSelection;
}

void SelectionController::notifySelectionChanged() {
  // To avoid regression on speedometer benchmark[1] test, we should not
  // update layout tree in this code block.
  // [1] http://browserbench.org/Speedometer/
  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      m_frame->document()->lifecycle());

  const SelectionInDOMTree& selection = this->selection().selectionInDOMTree();
  switch (selection.selectionTypeWithLegacyGranularity()) {
    case NoSelection:
      m_selectionState = SelectionState::HaveNotStartedSelection;
      return;
    case CaretSelection:
      m_selectionState = SelectionState::PlacedCaret;
      return;
    case RangeSelection:
      m_selectionState = SelectionState::ExtendedSelection;
      return;
  }
  NOTREACHED() << "We should handle all SelectionType" << selection;
}

FrameSelection& SelectionController::selection() const {
  return m_frame->selection();
}

bool isLinkSelection(const MouseEventWithHitTestResults& event) {
  return (event.event().modifiers() & WebInputEvent::Modifiers::AltKey) != 0 &&
         event.isOverLink();
}

bool isExtendingSelection(const MouseEventWithHitTestResults& event) {
  bool isMouseDownOnLinkOrImage =
      event.isOverLink() || event.hitTestResult().image();
  return (event.event().modifiers() & WebInputEvent::Modifiers::ShiftKey) !=
             0 &&
         !isMouseDownOnLinkOrImage;
}

}  // namespace blink
