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
#include "core/InputTypeNames.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/CharacterData.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/GranularityStrategy.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/PendingSelection.h"
#include "core/editing/RenderedPosition.h"
#include "core/editing/SelectionController.h"
#include "core/editing/SelectionEditor.h"
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
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/EditorClient.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"
#include "platform/SecureTextInput.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/text/UnicodeUtilities.h"
#include "wtf/text/CString.h"
#include <stdio.h>

#define EDIT_DEBUG 0

namespace blink {

using namespace HTMLNames;

static inline bool shouldAlwaysUseDirectionalSelection(LocalFrame* frame)
{
    return !frame || frame->editor().behavior().shouldConsiderSelectionAsDirectional();
}

FrameSelection::FrameSelection(LocalFrame* frame)
    : m_frame(frame)
    , m_pendingSelection(PendingSelection::create(*this))
    , m_selectionEditor(SelectionEditor::create(*this))
    , m_granularity(CharacterGranularity)
    , m_previousCaretVisibility(Hidden)
    , m_caretBlinkTimer(this, &FrameSelection::caretBlinkTimerFired)
    , m_caretRectDirty(true)
    , m_shouldPaintCaret(true)
    , m_isCaretBlinkingSuspended(false)
    , m_focused(frame && frame->page() && frame->page()->focusController().focusedFrame() == frame)
    , m_shouldShowBlockCursor(false)
{
    if (shouldAlwaysUseDirectionalSelection(m_frame))
        m_selectionEditor->setIsDirectional(true);
}

FrameSelection::~FrameSelection()
{
}

template <>
VisiblePosition FrameSelection::originalBase<EditingStrategy>() const
{
    return m_originalBase;
}

template <>
VisiblePositionInComposedTree FrameSelection::originalBase<EditingInComposedTreeStrategy>() const
{
    return m_originalBaseInComposedTree;
}

// TODO(yosin): To avoid undefined symbols in clang, we explicitly
// have specialized version of |FrameSelection::visibleSelection<Strategy>|
// before |FrameSelection::selection()| which refers this.
template <>
const VisibleSelection& FrameSelection::visibleSelection<EditingStrategy>() const
{
    return m_selectionEditor->visibleSelection<EditingStrategy>();
}

template <>
const VisibleSelectionInComposedTree& FrameSelection::visibleSelection<EditingInComposedTreeStrategy>() const
{
    return m_selectionEditor->visibleSelection<EditingInComposedTreeStrategy>();
}

Element* FrameSelection::rootEditableElementOrDocumentElement() const
{
    Element* selectionRoot = selection().rootEditableElement();
    return selectionRoot ? selectionRoot : m_frame->document()->documentElement();
}

ContainerNode* FrameSelection::rootEditableElementOrTreeScopeRootNode() const
{
    Element* selectionRoot = selection().rootEditableElement();
    if (selectionRoot)
        return selectionRoot;

    Node* node = selection().base().computeContainerNode();
    return node ? &node->treeScope().rootNode() : 0;
}

const VisibleSelection& FrameSelection::selection() const
{
    return visibleSelection<EditingStrategy>();
}

const VisibleSelectionInComposedTree& FrameSelection::selectionInComposedTree() const
{
    return visibleSelection<EditingInComposedTreeStrategy>();
}

void FrameSelection::moveTo(const VisiblePosition &pos, EUserTriggered userTriggered, CursorAlignOnScroll align)
{
    SetSelectionOptions options = CloseTyping | ClearTypingStyle | userTriggered;
    setSelection(VisibleSelection(pos, pos, selection().isDirectional()), options, align);
}

void FrameSelection::moveTo(const VisiblePosition &base, const VisiblePosition &extent, EUserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    SetSelectionOptions options = CloseTyping | ClearTypingStyle | userTriggered;
    setSelection(VisibleSelection(base, extent, selectionHasDirection), options);
}

void FrameSelection::moveTo(const Position &pos, TextAffinity affinity, EUserTriggered userTriggered)
{
    SetSelectionOptions options = CloseTyping | ClearTypingStyle | userTriggered;
    setSelection(VisibleSelection(pos, affinity, selection().isDirectional()), options);
}

template <typename Strategy>
static void adjustEndpointsAtBidiBoundary(VisiblePositionTemplate<Strategy>& visibleBase, VisiblePositionTemplate<Strategy>& visibleExtent)
{
    RenderedPosition base(visibleBase);
    RenderedPosition extent(visibleExtent);

    if (base.isNull() || extent.isNull() || base.isEquivalent(extent))
        return;

    if (base.atLeftBoundaryOfBidiRun()) {
        if (!extent.atRightBoundaryOfBidiRun(base.bidiLevelOnRight())
            && base.isEquivalent(extent.leftBoundaryOfBidiRun(base.bidiLevelOnRight()))) {
            visibleBase = createVisiblePosition(fromPositionInDOMTree<Strategy>(base.positionAtLeftBoundaryOfBiDiRun()));
            return;
        }
        return;
    }

    if (base.atRightBoundaryOfBidiRun()) {
        if (!extent.atLeftBoundaryOfBidiRun(base.bidiLevelOnLeft())
            && base.isEquivalent(extent.rightBoundaryOfBidiRun(base.bidiLevelOnLeft()))) {
            visibleBase = createVisiblePosition(fromPositionInDOMTree<Strategy>(base.positionAtRightBoundaryOfBiDiRun()));
            return;
        }
        return;
    }

    if (extent.atLeftBoundaryOfBidiRun() && extent.isEquivalent(base.leftBoundaryOfBidiRun(extent.bidiLevelOnRight()))) {
        visibleExtent = createVisiblePosition(fromPositionInDOMTree<Strategy>(extent.positionAtLeftBoundaryOfBiDiRun()));
        return;
    }

    if (extent.atRightBoundaryOfBidiRun() && extent.isEquivalent(base.rightBoundaryOfBidiRun(extent.bidiLevelOnLeft()))) {
        visibleExtent = createVisiblePosition(fromPositionInDOMTree<Strategy>(extent.positionAtRightBoundaryOfBiDiRun()));
        return;
    }
}

template <typename Strategy>
void FrameSelection::setNonDirectionalSelectionIfNeededAlgorithm(const VisibleSelectionTemplate<Strategy>& passedNewSelection, TextGranularity granularity,
    EndPointsAdjustmentMode endpointsAdjustmentMode)
{
    VisibleSelectionTemplate<Strategy> newSelection = passedNewSelection;
    bool isDirectional = shouldAlwaysUseDirectionalSelection(m_frame) || newSelection.isDirectional();

    const VisiblePositionTemplate<Strategy> originalBase = this->originalBase<Strategy>();
    const VisiblePositionTemplate<Strategy> base = originalBase.isNotNull() ? originalBase : createVisiblePosition(newSelection.base());
    VisiblePositionTemplate<Strategy> newBase = base;
    const VisiblePositionTemplate<Strategy> extent = createVisiblePosition(newSelection.extent());
    VisiblePositionTemplate<Strategy> newExtent = extent;
    if (endpointsAdjustmentMode == AdjustEndpointsAtBidiBoundary)
        adjustEndpointsAtBidiBoundary(newBase, newExtent);

    if (newBase.deepEquivalent() != base.deepEquivalent() || newExtent.deepEquivalent() != extent.deepEquivalent()) {
        setOriginalBase(base);
        newSelection.setBase(newBase);
        newSelection.setExtent(newExtent);
    } else if (originalBase.isNotNull()) {
        if (visibleSelection<Strategy>().base() == newSelection.base())
            newSelection.setBase(originalBase);
        setOriginalBase(VisiblePositionTemplate<Strategy>());
    }

    // Adjusting base and extent will make newSelection always directional
    newSelection.setIsDirectional(isDirectional);
    if (visibleSelection<Strategy>() == newSelection)
        return;

    setSelection(newSelection, granularity);
}

void FrameSelection::setNonDirectionalSelectionIfNeeded(const VisibleSelection& passedNewSelection, TextGranularity granularity, EndPointsAdjustmentMode endpointsAdjustmentMode)
{
    setNonDirectionalSelectionIfNeededAlgorithm<EditingStrategy>(passedNewSelection, granularity, endpointsAdjustmentMode);
}

void FrameSelection::setNonDirectionalSelectionIfNeeded(const VisibleSelectionInComposedTree& passedNewSelection, TextGranularity granularity, EndPointsAdjustmentMode endpointsAdjustmentMode)
{
    setNonDirectionalSelectionIfNeededAlgorithm<EditingInComposedTreeStrategy>(passedNewSelection, granularity, endpointsAdjustmentMode);
}

template <typename Strategy>
void FrameSelection::setSelectionAlgorithm(const VisibleSelectionTemplate<Strategy>& newSelection, SetSelectionOptions options, CursorAlignOnScroll align, TextGranularity granularity)
{
    if (m_granularityStrategy && (options & FrameSelection::DoNotClearStrategy) == 0)
        m_granularityStrategy->Clear();
    bool closeTyping = options & CloseTyping;
    bool shouldClearTypingStyle = options & ClearTypingStyle;
    EUserTriggered userTriggered = selectionOptionsToUserTriggered(options);

    VisibleSelectionTemplate<Strategy> s = validateSelection(newSelection);
    if (shouldAlwaysUseDirectionalSelection(m_frame))
        s.setIsDirectional(true);

    if (!m_frame) {
        m_selectionEditor->setVisibleSelection(s, options);
        return;
    }

    // <http://bugs.webkit.org/show_bug.cgi?id=23464>: Infinite recursion at
    // |FrameSelection::setSelection|
    // if |document->frame()| == |m_frame| we can get into an infinite loop
    if (s.base().anchorNode()) {
        Document& document = *s.base().document();
        // TODO(hajimehoshi): validateSelection already checks if the selection
        // is valid, thus we don't need this 'if' clause any more.
        if (document.frame() && document.frame() != m_frame && document != m_frame->document()) {
            RefPtrWillBeRawPtr<LocalFrame> guard(document.frame());
            document.frame()->selection().setSelection(s, options, align, granularity);
            // It's possible that during the above set selection, this
            // |FrameSelection| has been modified by
            // |selectFrameElementInParentIfFullySelected|, but that the
            // selection is no longer valid since the frame is about to be
            // destroyed. If this is the case, clear our selection.
            if (!guard->host() && !selection().isNonOrphanedCaretOrRange())
                clear();
            return;
        }
    }

    m_granularity = granularity;

    // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
    // |Editor| class.
    if (closeTyping)
        TypingCommand::closeTyping(m_frame);

    if (shouldClearTypingStyle)
        clearTypingStyle();

    if (m_selectionEditor->visibleSelection<Strategy>() == s) {
        // Even if selection was not changed, selection offsets may have been
        // changed.
        m_frame->inputMethodController().cancelCompositionIfSelectionIsInvalid();
        notifyLayoutObjectOfSelectionChange(userTriggered);
        return;
    }

    const VisibleSelectionTemplate<Strategy> oldSelection = visibleSelection<Strategy>();
    const VisibleSelection oldSelectionInDOMTree = selection();

    m_selectionEditor->setVisibleSelection(s, options);
    setCaretRectNeedsUpdate();

    if (!s.isNone() && !(options & DoNotSetFocus))
        setFocusedNodeIfNeeded();

    if (!(options & DoNotUpdateAppearance)) {
        // Hits in compositing/overflow/do-not-paint-outline-into-composited-scrolling-contents.html
        DisableCompositingQueryAsserts disabler;
        stopCaretBlinkTimer();
        updateAppearance();
    }

    // Always clear the x position used for vertical arrow navigation.
    // It will be restored by the vertical arrow navigation code if necessary.
    m_selectionEditor->resetXPosForVerticalArrowNavigation();
    RefPtrWillBeRawPtr<LocalFrame> protector(m_frame.get());
    // This may dispatch a synchronous focus-related events.
    selectFrameElementInParentIfFullySelected();
    notifyLayoutObjectOfSelectionChange(userTriggered);
    // If the selections are same in the DOM tree but not in the composed tree,
    // don't fire events. For example, if the selection crosses shadow tree
    // boundary, selection for the DOM tree is shrunk while that for the
    // composed tree is not. Additionally, this case occurs in some edge cases.
    // See also: editing/pasteboard/4076267-3.html
    if (oldSelection == m_selectionEditor->visibleSelection<Strategy>()) {
        m_frame->inputMethodController().cancelCompositionIfSelectionIsInvalid();
        return;
    }
    m_frame->editor().respondToChangedSelection(oldSelectionInDOMTree, options);
    if (userTriggered == UserTriggered) {
        ScrollAlignment alignment;

        if (m_frame->editor().behavior().shouldCenterAlignWhenSelectionIsRevealed())
            alignment = (align == CursorAlignOnScroll::Always) ? ScrollAlignment::alignCenterAlways : ScrollAlignment::alignCenterIfNeeded;
        else
            alignment = (align == CursorAlignOnScroll::Always) ? ScrollAlignment::alignTopAlways : ScrollAlignment::alignToEdgeIfNeeded;

        revealSelection(alignment, RevealExtent);
    }

    notifyAccessibilityForSelectionChange();
    notifyCompositorForSelectionChange();
    notifyEventHandlerForSelectionChange();
    m_frame->localDOMWindow()->enqueueDocumentEvent(Event::create(EventTypeNames::selectionchange));
}

void FrameSelection::setSelection(const VisibleSelection& newSelection, SetSelectionOptions options, CursorAlignOnScroll align, TextGranularity granularity)
{
    setSelectionAlgorithm<EditingStrategy>(newSelection, options, align, granularity);
}

void FrameSelection::setSelection(const VisibleSelectionInComposedTree& newSelection, SetSelectionOptions options, CursorAlignOnScroll align, TextGranularity granularity)
{
    setSelectionAlgorithm<EditingInComposedTreeStrategy>(newSelection, options, align, granularity);
}

static bool removingNodeRemovesPosition(Node& node, const Position& position)
{
    if (!position.anchorNode())
        return false;

    if (position.anchorNode() == node)
        return true;

    if (!node.isElementNode())
        return false;

    Element& element = toElement(node);
    return element.containsIncludingShadowDOM(position.anchorNode());
}

void FrameSelection::nodeWillBeRemoved(Node& node)
{
    // There can't be a selection inside a fragment, so if a fragment's node is being removed,
    // the selection in the document that created the fragment needs no adjustment.
    if (isNone() || !node.inActiveDocument())
        return;

    respondToNodeModification(node, removingNodeRemovesPosition(node, selection().base()), removingNodeRemovesPosition(node, selection().extent()),
        removingNodeRemovesPosition(node, selection().start()), removingNodeRemovesPosition(node, selection().end()));
}

static bool intersectsNode(const VisibleSelection& selection, Node* node)
{
    if (selection.isNone())
        return false;
    Position start = selection.start().parentAnchoredEquivalent();
    Position end = selection.end().parentAnchoredEquivalent();
    TrackExceptionState exceptionState;
    // TODO(yosin) We should avoid to use |Range::intersectsNode()|.
    return Range::intersectsNode(node, start, end, exceptionState) && !exceptionState.hadException();
}

void FrameSelection::respondToNodeModification(Node& node, bool baseRemoved, bool extentRemoved, bool startRemoved, bool endRemoved)
{
    ASSERT(node.document().isActive());

    bool clearLayoutTreeSelection = false;
    bool clearDOMTreeSelection = false;

    if (startRemoved || endRemoved) {
        Position start = selection().start();
        Position end = selection().end();
        if (startRemoved)
            updatePositionForNodeRemoval(start, node);
        if (endRemoved)
            updatePositionForNodeRemoval(end, node);

        if (Position::commonAncestorTreeScope(start, end) && start.isNotNull() && end.isNotNull()) {
            if (selection().isBaseFirst())
                m_selectionEditor->setWithoutValidation(start, end);
            else
                m_selectionEditor->setWithoutValidation(end, start);
        } else {
            clearDOMTreeSelection = true;
        }

        clearLayoutTreeSelection = true;
    } else if (baseRemoved || extentRemoved) {
        // The base and/or extent are about to be removed, but the start and end aren't.
        // Change the base and extent to the start and end, but don't re-validate the
        // selection, since doing so could move the start and end into the node
        // that is about to be removed.
        if (selection().isBaseFirst())
            m_selectionEditor->setWithoutValidation(selection().start(), selection().end());
        else
            m_selectionEditor->setWithoutValidation(selection().end(), selection().start());
    } else if (intersectsNode(selection(), &node)) {
        // If we did nothing here, when this node's layoutObject was destroyed, the rect that it
        // occupied would be invalidated, but, selection gaps that change as a result of
        // the removal wouldn't be invalidated.
        // FIXME: Don't do so much unnecessary invalidation.
        clearLayoutTreeSelection = true;
    }

    if (clearLayoutTreeSelection)
        selection().start().document()->layoutView()->clearSelection();

    if (clearDOMTreeSelection)
        setSelection(VisibleSelection(), DoNotSetFocus);

    // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
    // |Editor| class.
    if (!m_frame->document()->isRunningExecCommand())
        TypingCommand::closeTyping(m_frame);
}

static Position updatePositionAfterAdoptingTextReplacement(const Position& position, CharacterData* node, unsigned offset, unsigned oldLength, unsigned newLength)
{
    if (!position.anchorNode() || position.anchorNode() != node || !position.isOffsetInAnchor())
        return position;

    // See: http://www.w3.org/TR/DOM-Level-2-Traversal-Range/ranges.html#Level-2-Range-Mutation
    ASSERT(position.offsetInContainerNode() >= 0);
    unsigned positionOffset = static_cast<unsigned>(position.offsetInContainerNode());
    // Replacing text can be viewed as a deletion followed by insertion.
    if (positionOffset >= offset && positionOffset <= offset + oldLength)
        positionOffset = offset;

    // Adjust the offset if the position is after the end of the deleted contents
    // (positionOffset > offset + oldLength) to avoid having a stale offset.
    if (positionOffset > offset + oldLength)
        positionOffset = positionOffset - oldLength + newLength;

    // Due to case folding (http://unicode.org/Public/UCD/latest/ucd/CaseFolding.txt),
    // LayoutText length may be different from Text length.  A correct implementation
    // would translate the LayoutText offset to a Text offset; this is just a safety
    // precaution to avoid offset values that run off the end of the Text.
    if (positionOffset > node->length())
        positionOffset = node->length();

    // CharacterNode in VisibleSelection must be Text node, because Comment
    // and ProcessingInstruction node aren't visible.
    return Position(toText(node), positionOffset);
}

void FrameSelection::didUpdateCharacterData(CharacterData* node, unsigned offset, unsigned oldLength, unsigned newLength)
{
    // The fragment check is a performance optimization. See http://trac.webkit.org/changeset/30062.
    if (isNone() || !node || !node->inDocument())
        return;

    Position base = updatePositionAfterAdoptingTextReplacement(selection().base(), node, offset, oldLength, newLength);
    Position extent = updatePositionAfterAdoptingTextReplacement(selection().extent(), node, offset, oldLength, newLength);
    Position start = updatePositionAfterAdoptingTextReplacement(selection().start(), node, offset, oldLength, newLength);
    Position end = updatePositionAfterAdoptingTextReplacement(selection().end(), node, offset, oldLength, newLength);
    updateSelectionIfNeeded(base, extent, start, end);
}

static Position updatePostionAfterAdoptingTextNodesMerged(const Position& position, const Text& oldNode, unsigned offset)
{
    if (!position.anchorNode() || !position.isOffsetInAnchor())
        return position;

    ASSERT(position.offsetInContainerNode() >= 0);
    unsigned positionOffset = static_cast<unsigned>(position.offsetInContainerNode());

    if (position.anchorNode() == &oldNode)
        return Position(toText(oldNode.previousSibling()), positionOffset + offset);

    if (position.anchorNode() == oldNode.parentNode() && positionOffset == offset)
        return Position(toText(oldNode.previousSibling()), offset);

    return position;
}

void FrameSelection::didMergeTextNodes(const Text& oldNode, unsigned offset)
{
    if (isNone() || !oldNode.inDocument())
        return;
    Position base = updatePostionAfterAdoptingTextNodesMerged(selection().base(), oldNode, offset);
    Position extent = updatePostionAfterAdoptingTextNodesMerged(selection().extent(), oldNode, offset);
    Position start = updatePostionAfterAdoptingTextNodesMerged(selection().start(), oldNode, offset);
    Position end = updatePostionAfterAdoptingTextNodesMerged(selection().end(), oldNode, offset);
    updateSelectionIfNeeded(base, extent, start, end);
}

static Position updatePostionAfterAdoptingTextNodeSplit(const Position& position, const Text& oldNode)
{
    if (!position.anchorNode() || position.anchorNode() != &oldNode || !position.isOffsetInAnchor())
        return position;
    // See: http://www.w3.org/TR/DOM-Level-2-Traversal-Range/ranges.html#Level-2-Range-Mutation
    ASSERT(position.offsetInContainerNode() >= 0);
    unsigned positionOffset = static_cast<unsigned>(position.offsetInContainerNode());
    unsigned oldLength = oldNode.length();
    if (positionOffset <= oldLength)
        return position;
    return Position(toText(oldNode.nextSibling()), positionOffset - oldLength);
}

void FrameSelection::didSplitTextNode(const Text& oldNode)
{
    if (isNone() || !oldNode.inDocument())
        return;
    Position base = updatePostionAfterAdoptingTextNodeSplit(selection().base(), oldNode);
    Position extent = updatePostionAfterAdoptingTextNodeSplit(selection().extent(), oldNode);
    Position start = updatePostionAfterAdoptingTextNodeSplit(selection().start(), oldNode);
    Position end = updatePostionAfterAdoptingTextNodeSplit(selection().end(), oldNode);
    updateSelectionIfNeeded(base, extent, start, end);
}

void FrameSelection::updateSelectionIfNeeded(const Position& base, const Position& extent, const Position& start, const Position& end)
{
    if (base == selection().base() && extent == selection().extent() && start == selection().start() && end == selection().end())
        return;
    // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
    // |Editor| class.
    if (!m_frame->document()->isRunningExecCommand())
        TypingCommand::closeTyping(m_frame);
    VisibleSelection newSelection;
    if (selection().isBaseFirst())
        newSelection.setWithoutValidation(start, end);
    else
        newSelection.setWithoutValidation(end, start);
    setSelection(newSelection, DoNotSetFocus);
}

void FrameSelection::didChangeFocus()
{
    // Hits in virtual/gpu/compositedscrolling/scrollbars/scrollbar-miss-mousemove-disabled.html
    DisableCompositingQueryAsserts disabler;
    updateAppearance();
}

bool FrameSelection::modify(EAlteration alter, SelectionDirection direction, TextGranularity granularity, EUserTriggered userTriggered)
{
    if (!m_selectionEditor->modify(alter, direction, granularity, userTriggered))
        return false;

    if (userTriggered == UserTriggered)
        m_granularity = CharacterGranularity;

    setCaretRectNeedsUpdate();

    return true;
}

bool FrameSelection::modify(EAlteration alter, unsigned verticalDistance, VerticalDirection direction, EUserTriggered userTriggered, CursorAlignOnScroll align)
{
    if (!m_selectionEditor->modify(alter, verticalDistance, direction, userTriggered, align))
        return false;

    if (userTriggered == UserTriggered)
        m_granularity = CharacterGranularity;

    return true;
}

void FrameSelection::clear()
{
    m_granularity = CharacterGranularity;
    if (m_granularityStrategy)
        m_granularityStrategy->Clear();
    setSelection(VisibleSelection());
}

void FrameSelection::prepareForDestruction()
{
    m_granularity = CharacterGranularity;

    m_caretBlinkTimer.stop();

    LayoutView* view = m_frame->contentLayoutObject();
    if (view)
        view->clearSelection();

    setSelection(VisibleSelection(), CloseTyping | ClearTypingStyle | DoNotUpdateAppearance);
    m_selectionEditor->dispose();
    m_previousCaretNode.clear();
}

void FrameSelection::setStart(const VisiblePosition &pos, EUserTriggered trigger)
{
    if (selection().isBaseFirst())
        setBase(pos, trigger);
    else
        setExtent(pos, trigger);
}

void FrameSelection::setEnd(const VisiblePosition &pos, EUserTriggered trigger)
{
    if (selection().isBaseFirst())
        setExtent(pos, trigger);
    else
        setBase(pos, trigger);
}

void FrameSelection::setBase(const VisiblePosition &pos, EUserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(pos.deepEquivalent(), selection().extent(), pos.affinity(), selectionHasDirection), CloseTyping | ClearTypingStyle | userTriggered);
}

void FrameSelection::setExtent(const VisiblePosition &pos, EUserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(selection().base(), pos.deepEquivalent(), pos.affinity(), selectionHasDirection), CloseTyping | ClearTypingStyle | userTriggered);
}

static bool isTextFormControl(const VisibleSelection& selection)
{
    return enclosingTextFormControl(selection.start());
}

LayoutBlock* FrameSelection::caretLayoutObject() const
{
    ASSERT(selection().isValidFor(*m_frame->document()));
    if (!isCaret())
        return nullptr;
    return CaretBase::caretLayoutObject(selection().start().anchorNode());
}

IntRect FrameSelection::absoluteCaretBounds()
{
    ASSERT(selection().isValidFor(*m_frame->document()));
    ASSERT(m_frame->document()->lifecycle().state() != DocumentLifecycle::InPaintInvalidation);
    m_frame->document()->updateLayoutIgnorePendingStylesheets();
    if (!isCaret()) {
        clearCaretRect();
    } else {
        if (isTextFormControl(selection()))
            updateCaretRect(PositionWithAffinity(isVisuallyEquivalentCandidate(selection().start()) ? selection().start() : Position(), selection().affinity()));
        else
            updateCaretRect(createVisiblePosition(selection().start(), selection().affinity()));
    }
    return absoluteBoundsForLocalRect(selection().start().anchorNode(), localCaretRectWithoutUpdate());
}

void FrameSelection::invalidateCaretRect()
{
    if (!m_caretRectDirty)
        return;
    m_caretRectDirty = false;

    ASSERT(selection().isValidFor(*m_frame->document()));
    LayoutObject* layoutObject = nullptr;
    LayoutRect newRect;
    if (selection().isCaret())
        newRect = localCaretRectOfPosition(PositionWithAffinity(selection().start(), selection().affinity()), layoutObject);
    Node* newNode = layoutObject ? layoutObject->node() : nullptr;

    if (!m_caretBlinkTimer.isActive()
        && newNode == m_previousCaretNode
        && newRect == m_previousCaretRect
        && caretVisibility() == m_previousCaretVisibility)
        return;

    LayoutView* view = m_frame->document()->layoutView();
    if (m_previousCaretNode && (shouldRepaintCaret(*m_previousCaretNode) || shouldRepaintCaret(view)))
        invalidateLocalCaretRect(m_previousCaretNode.get(), m_previousCaretRect);
    if (newNode && (shouldRepaintCaret(*newNode) || shouldRepaintCaret(view)))
        invalidateLocalCaretRect(newNode, newRect);
    m_previousCaretNode = newNode;
    m_previousCaretRect = newRect;
    m_previousCaretVisibility = caretVisibility();
}

void FrameSelection::paintCaret(GraphicsContext& context, const LayoutPoint& paintOffset)
{
    if (selection().isCaret() && m_shouldPaintCaret) {
        updateCaretRect(PositionWithAffinity(selection().start(), selection().affinity()));
        CaretBase::paintCaret(selection().start().anchorNode(), context, paintOffset);
    }
}

template <typename Strategy>
bool FrameSelection::containsAlgorithm(const LayoutPoint& point)
{
    Document* document = m_frame->document();
    if (!document->layoutView())
        return false;

    // Treat a collapsed selection like no selection.
    const VisibleSelectionTemplate<Strategy> visibleSelection = this->visibleSelection<Strategy>();
    if (!visibleSelection.isRange())
        return false;

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
    HitTestResult result(request, point);
    document->layoutView()->hitTest(result);
    Node* innerNode = result.innerNode();
    if (!innerNode || !innerNode->layoutObject())
        return false;

    const VisiblePositionTemplate<Strategy> visiblePos = createVisiblePosition(fromPositionInDOMTree<Strategy>(innerNode->layoutObject()->positionForPoint(result.localPoint())));
    if (visiblePos.isNull())
        return false;

    const VisiblePositionTemplate<Strategy> visibleStart = visibleSelection.visibleStart();
    const VisiblePositionTemplate<Strategy> visibleEnd = visibleSelection.visibleEnd();
    if (visibleStart.isNull() || visibleEnd.isNull())
        return false;

    const PositionTemplate<Strategy> start = visibleStart.deepEquivalent();
    const PositionTemplate<Strategy> end = visibleEnd.deepEquivalent();
    const PositionTemplate<Strategy> pos = visiblePos.deepEquivalent();
    return start.compareTo(pos) <= 0 && pos.compareTo(end) <= 0;
}

bool FrameSelection::contains(const LayoutPoint& point)
{
    if (RuntimeEnabledFeatures::selectionForComposedTreeEnabled())
        return containsAlgorithm<EditingInComposedTreeStrategy>(point);
    return containsAlgorithm<EditingStrategy>(point);
}

// Workaround for the fact that it's hard to delete a frame.
// Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
// Can't do this implicitly as part of every setSelection call because in some contexts it might not be good
// for the focus to move to another frame. So instead we call it from places where we are selecting with the
// mouse or the keyboard after setting the selection.
void FrameSelection::selectFrameElementInParentIfFullySelected()
{
    // Find the parent frame; if there is none, then we have nothing to do.
    Frame* parent = m_frame->tree().parent();
    if (!parent)
        return;
    Page* page = m_frame->page();
    if (!page)
        return;

    // Check if the selection contains the entire frame contents; if not, then there is nothing to do.
    if (!isRange())
        return;
    if (!isStartOfDocument(selection().visibleStart()))
        return;
    if (!isEndOfDocument(selection().visibleEnd()))
        return;

    // FIXME: This is not yet implemented for cross-process frame relationships.
    if (!parent->isLocalFrame())
        return;

    // Get to the <iframe> or <frame> (or even <object>) element in the parent frame.
    // FIXME: Doesn't work for OOPI.
    HTMLFrameOwnerElement* ownerElement = m_frame->deprecatedLocalOwner();
    if (!ownerElement)
        return;
    ContainerNode* ownerElementParent = ownerElement->parentNode();
    if (!ownerElementParent)
        return;

    // This method's purpose is it to make it easier to select iframes (in order to delete them).  Don't do anything if the iframe isn't deletable.
    if (!ownerElementParent->hasEditableStyle())
        return;

    // Create compute positions before and after the element.
    unsigned ownerElementNodeIndex = ownerElement->nodeIndex();
    VisiblePosition beforeOwnerElement = createVisiblePosition(Position(ownerElementParent, ownerElementNodeIndex));
    VisiblePosition afterOwnerElement = createVisiblePosition(Position(ownerElementParent, ownerElementNodeIndex + 1), VP_UPSTREAM_IF_POSSIBLE);

    // Focus on the parent frame, and then select from before this element to after.
    VisibleSelection newSelection(beforeOwnerElement, afterOwnerElement);
    page->focusController().setFocusedFrame(parent);
    // setFocusedFrame can dispatch synchronous focus/blur events.  The document
    // tree might be modified.
    if (newSelection.isNonOrphanedCaretOrRange())
        toLocalFrame(parent)->selection().setSelection(newSelection);
}

void FrameSelection::selectAll()
{
    Document* document = m_frame->document();

    if (isHTMLSelectElement(document->focusedElement())) {
        HTMLSelectElement* selectElement = toHTMLSelectElement(document->focusedElement());
        if (selectElement->canSelectAll()) {
            selectElement->selectAll();
            return;
        }
    }

    RefPtrWillBeRawPtr<Node> root = nullptr;
    Node* selectStartTarget = nullptr;
    if (isContentEditable()) {
        root = highestEditableRoot(selection().start());
        if (Node* shadowRoot = selection().nonBoundaryShadowTreeRootNode())
            selectStartTarget = shadowRoot->shadowHost();
        else
            selectStartTarget = root.get();
    } else {
        root = selection().nonBoundaryShadowTreeRootNode();
        if (root) {
            selectStartTarget = root->shadowHost();
        } else {
            root = document->documentElement();
            selectStartTarget = document->body();
        }
    }
    if (!root || editingIgnoresContent(root.get()))
        return;

    if (selectStartTarget && !selectStartTarget->dispatchEvent(Event::createCancelableBubble(EventTypeNames::selectstart)))
        return;

    VisibleSelection newSelection(VisibleSelection::selectionFromContentsOfNode(root.get()));
    setSelection(newSelection);
    selectFrameElementInParentIfFullySelected();
    notifyLayoutObjectOfSelectionChange(UserTriggered);
}

bool FrameSelection::setSelectedRange(Range* range, TextAffinity affinity, SelectionDirectionalMode directional, SetSelectionOptions options)
{
    if (!range || !range->inDocument())
        return false;
    ASSERT(range->startContainer()->document() == range->endContainer()->document());
    return setSelectedRange(EphemeralRange(range), affinity, directional, options);
}

bool FrameSelection::setSelectedRange(const EphemeralRange& range, TextAffinity affinity, SelectionDirectionalMode directional, SetSelectionOptions options)
{
    return m_selectionEditor->setSelectedRange(range, affinity, directional, options);
}

PassRefPtrWillBeRawPtr<Range> FrameSelection::firstRange() const
{
    return m_selectionEditor->firstRange();
}

bool FrameSelection::isInPasswordField() const
{
    HTMLTextFormControlElement* textControl = enclosingTextFormControl(start());
    return isHTMLInputElement(textControl) && toHTMLInputElement(textControl)->type() == InputTypeNames::password;
}

void FrameSelection::notifyAccessibilityForSelectionChange()
{
    if (selection().start().isNotNull() && selection().end().isNotNull()) {
        if (AXObjectCache* cache = m_frame->document()->existingAXObjectCache())
            cache->selectionChanged(selection().start().computeContainerNode());
    }
}

void FrameSelection::notifyCompositorForSelectionChange()
{
    if (!RuntimeEnabledFeatures::compositedSelectionUpdateEnabled())
        return;

    scheduleVisualUpdate();
}

void FrameSelection::notifyEventHandlerForSelectionChange()
{
    m_frame->eventHandler().selectionController().notifySelectionChanged();
}

void FrameSelection::focusedOrActiveStateChanged()
{
    bool activeAndFocused = isFocusedAndActive();
    RefPtrWillBeRawPtr<Document> document = m_frame->document();

    // Trigger style invalidation from the focused element. Even though
    // the focused element hasn't changed, the evaluation of focus pseudo
    // selectors are dependent on whether the frame is focused and active.
    if (Element* element = document->focusedElement())
        element->focusStateChanged();

    document->updateLayoutTreeIfNeeded();

    // Because LayoutObject::selectionBackgroundColor() and
    // LayoutObject::selectionForegroundColor() check if the frame is active,
    // we have to update places those colors were painted.
    if (LayoutView* view = document->layoutView())
        view->invalidatePaintForSelection();

    // Caret appears in the active frame.
    if (activeAndFocused)
        setSelectionFromNone();
    else
        m_frame->spellChecker().spellCheckAfterBlur();
    setCaretVisibility(activeAndFocused ? Visible : Hidden);

    // Update for caps lock state
    m_frame->eventHandler().capsLockStateMayHaveChanged();

    // Secure keyboard entry is set by the active frame.
    if (document->useSecureKeyboardEntryWhenActive())
        setUseSecureKeyboardEntry(activeAndFocused);
}

void FrameSelection::pageActivationChanged()
{
    focusedOrActiveStateChanged();
}

void FrameSelection::updateSecureKeyboardEntryIfActive()
{
    if (m_frame->document() && isFocusedAndActive())
        setUseSecureKeyboardEntry(m_frame->document()->useSecureKeyboardEntryWhenActive());
}

void FrameSelection::setUseSecureKeyboardEntry(bool enable)
{
    if (enable)
        enableSecureTextInput();
    else
        disableSecureTextInput();
}

void FrameSelection::setFocused(bool flag)
{
    if (m_focused == flag)
        return;
    m_focused = flag;

    focusedOrActiveStateChanged();
}

bool FrameSelection::isFocusedAndActive() const
{
    return m_focused && m_frame->page() && m_frame->page()->focusController().isActive();
}

inline static bool shouldStopBlinkingDueToTypingCommand(LocalFrame* frame)
{
    return frame->editor().lastEditCommand() && frame->editor().lastEditCommand()->shouldStopCaretBlinking();
}

bool FrameSelection::isAppearanceDirty() const
{
    return m_pendingSelection->hasPendingSelection();
}

void FrameSelection::commitAppearanceIfNeeded(LayoutView& layoutView)
{
    return m_pendingSelection->commit(layoutView);
}

void FrameSelection::updateAppearance()
{
    // Paint a block cursor instead of a caret in overtype mode unless the caret is at the end of a line (in this case
    // the FrameSelection will paint a blinking caret as usual).
    bool paintBlockCursor = m_shouldShowBlockCursor && selection().isCaret() && !isLogicalEndOfLine(selection().visibleEnd());

    bool shouldBlink = !paintBlockCursor && shouldBlinkCaret();

    // If the caret moved, stop the blink timer so we can restart with a
    // black caret in the new location.
    if (!shouldBlink || shouldStopBlinkingDueToTypingCommand(m_frame))
        stopCaretBlinkTimer();

    // Start blinking with a black caret. Be sure not to restart if we're
    // already blinking in the right location.
    if (shouldBlink && !m_caretBlinkTimer.isActive()) {
        if (double blinkInterval = LayoutTheme::theme().caretBlinkInterval())
            m_caretBlinkTimer.startRepeating(blinkInterval, BLINK_FROM_HERE);

        m_shouldPaintCaret = true;
        setCaretRectNeedsUpdate();
    }

    LayoutView* view = m_frame->contentLayoutObject();
    if (!view)
        return;
    m_pendingSelection->setHasPendingSelection();
}

void FrameSelection::setCaretVisibility(CaretVisibility visibility)
{
    if (caretVisibility() == visibility)
        return;

    CaretBase::setCaretVisibility(visibility);

    updateAppearance();
}

bool FrameSelection::shouldBlinkCaret() const
{
    if (!caretIsVisible() || !isCaret())
        return false;

    if (m_frame->settings() && m_frame->settings()->caretBrowsingEnabled())
        return false;

    Element* root = rootEditableElement();
    if (!root)
        return false;

    Element* focusedElement = root->document().focusedElement();
    if (!focusedElement)
        return false;

    return focusedElement->containsIncludingShadowDOM(selection().start().anchorNode());
}

void FrameSelection::caretBlinkTimerFired(Timer<FrameSelection>*)
{
    ASSERT(caretIsVisible());
    ASSERT(isCaret());
    if (isCaretBlinkingSuspended() && m_shouldPaintCaret)
        return;
    m_shouldPaintCaret = !m_shouldPaintCaret;
    setCaretRectNeedsUpdate();
}

void FrameSelection::stopCaretBlinkTimer()
{
    if (m_caretBlinkTimer.isActive() || m_shouldPaintCaret)
        setCaretRectNeedsUpdate();
    m_shouldPaintCaret = false;
    m_caretBlinkTimer.stop();
}

void FrameSelection::notifyLayoutObjectOfSelectionChange(EUserTriggered userTriggered)
{
    if (HTMLTextFormControlElement* textControl = enclosingTextFormControl(start()))
        textControl->selectionChanged(userTriggered == UserTriggered);
}

// Helper function that tells whether a particular node is an element that has an entire
// LocalFrame and FrameView, a <frame>, <iframe>, or <object>.
static bool isFrameElement(const Node* n)
{
    if (!n)
        return false;
    LayoutObject* layoutObject = n->layoutObject();
    if (!layoutObject || !layoutObject->isLayoutPart())
        return false;
    Widget* widget = toLayoutPart(layoutObject)->widget();
    return widget && widget->isFrameView();
}

void FrameSelection::setFocusedNodeIfNeeded()
{
    if (isNone() || !isFocused())
        return;

    bool caretBrowsing = m_frame->settings() && m_frame->settings()->caretBrowsingEnabled();
    if (caretBrowsing) {
        if (Element* anchor = enclosingAnchorElement(base())) {
            m_frame->page()->focusController().setFocusedElement(anchor, m_frame);
            return;
        }
    }

    if (Element* target = rootEditableElement()) {
        // Walk up the DOM tree to search for a node to focus.
        m_frame->document()->updateLayoutTreeIgnorePendingStylesheets();
        while (target) {
            // We don't want to set focus on a subframe when selecting in a parent frame,
            // so add the !isFrameElement check here. There's probably a better way to make this
            // work in the long term, but this is the safest fix at this time.
            if (target->isMouseFocusable() && !isFrameElement(target)) {
                m_frame->page()->focusController().setFocusedElement(target, m_frame);
                return;
            }
            target = target->parentOrShadowHostElement();
        }
        m_frame->document()->clearFocusedElement();
    }

    if (caretBrowsing)
        m_frame->page()->focusController().setFocusedElement(0, m_frame);
}

template <typename Strategy>
String extractSelectedTextAlgorithm(const FrameSelection& selection, TextIteratorBehavior behavior)
{
    const VisibleSelectionTemplate<Strategy> visibleSelection = selection.visibleSelection<Strategy>();
    const EphemeralRangeTemplate<Strategy> range = visibleSelection.toNormalizedEphemeralRange();
    // We remove '\0' characters because they are not visibly rendered to the user.
    return plainText(range, behavior).replace(0, "");
}

static String extractSelectedText(const FrameSelection& selection, TextIteratorBehavior behavior)
{
    if (RuntimeEnabledFeatures::selectionForComposedTreeEnabled())
        return extractSelectedTextAlgorithm<EditingInComposedTreeStrategy>(selection, behavior);
    return extractSelectedTextAlgorithm<EditingStrategy>(selection, behavior);
}

template <typename Strategy>
static String extractSelectedHTMLAlgorithm(const FrameSelection& selection)
{
    const VisibleSelectionTemplate<Strategy> visibleSelection = selection.visibleSelection<Strategy>();
    const EphemeralRangeTemplate<Strategy> range = visibleSelection.toNormalizedEphemeralRange();
    return createMarkup(range.startPosition(), range.endPosition(), AnnotateForInterchange, ConvertBlocksToInlines::NotConvert, ResolveNonLocalURLs);
}

String FrameSelection::selectedHTMLForClipboard() const
{
    if (!RuntimeEnabledFeatures::selectionForComposedTreeEnabled())
        return extractSelectedHTMLAlgorithm<EditingStrategy>(*this);
    return extractSelectedHTMLAlgorithm<EditingInComposedTreeStrategy>(*this);
}

String FrameSelection::selectedText(TextIteratorBehavior behavior) const
{
    return extractSelectedText(*this, behavior);
}

String FrameSelection::selectedTextForClipboard() const
{
    if (m_frame->settings() && m_frame->settings()->selectionIncludesAltImageText())
        return extractSelectedText(*this, TextIteratorEmitsImageAltText);
    return selectedText();
}

LayoutRect FrameSelection::bounds() const
{
    FrameView* view = m_frame->view();
    if (!view)
        return LayoutRect();

    return intersection(unclippedBounds(), LayoutRect(view->visibleContentRect()));
}

LayoutRect FrameSelection::unclippedBounds() const
{
    FrameView* view = m_frame->view();
    LayoutView* layoutView = m_frame->contentLayoutObject();

    if (!view || !layoutView)
        return LayoutRect();

    view->updateLifecycleToLayoutClean();
    return LayoutRect(layoutView->selectionBounds());
}

static inline HTMLFormElement* associatedFormElement(HTMLElement& element)
{
    if (isHTMLFormElement(element))
        return &toHTMLFormElement(element);
    return element.formOwner();
}

// Scans logically forward from "start", including any child frames.
static HTMLFormElement* scanForForm(Node* start)
{
    if (!start)
        return 0;

    for (HTMLElement& element : Traversal<HTMLElement>::startsAt(start->isHTMLElement() ? toHTMLElement(start) : Traversal<HTMLElement>::next(*start))) {
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

// We look for either the form containing the current focus, or for one immediately after it
HTMLFormElement* FrameSelection::currentForm() const
{
    // Start looking either at the active (first responder) node, or where the selection is.
    Node* start = m_frame->document()->focusedElement();
    if (!start)
        start = this->start().anchorNode();
    if (!start)
        return 0;

    // Try walking up the node tree to find a form element.
    for (HTMLElement* element = Traversal<HTMLElement>::firstAncestorOrSelf(*start); element; element = Traversal<HTMLElement>::firstAncestor(*element)) {
        if (HTMLFormElement* form = associatedFormElement(*element))
            return form;
    }

    // Try walking forward in the node tree to find a form element.
    return scanForForm(start);
}

void FrameSelection::revealSelection(const ScrollAlignment& alignment, RevealExtentOption revealExtentOption)
{
    LayoutRect rect;

    switch (selectionType()) {
    case NoSelection:
        return;
    case CaretSelection:
        rect = LayoutRect(absoluteCaretBounds());
        break;
    case RangeSelection:
        rect = LayoutRect(revealExtentOption == RevealExtent ? absoluteCaretBoundsOf(createVisiblePosition(extent())) : enclosingIntRect(unclippedBounds()));
        break;
    }

    Position start = this->start();
    ASSERT(start.anchorNode());
    if (start.anchorNode() && start.anchorNode()->layoutObject()) {
        // FIXME: This code only handles scrolling the startContainer's layer, but
        // the selection rect could intersect more than just that.
        if (DocumentLoader* documentLoader = m_frame->loader().documentLoader())
            documentLoader->initialScrollState().wasScrolledByUser = true;
        if (start.anchorNode()->layoutObject()->scrollRectToVisible(rect, alignment, alignment))
            updateAppearance();
    }
}

void FrameSelection::setSelectionFromNone()
{
    // Put a caret inside the body if the entire frame is editable (either the
    // entire WebView is editable or designMode is on for this document).

    Document* document = m_frame->document();
    bool caretBrowsing = m_frame->settings() && m_frame->settings()->caretBrowsingEnabled();
    if (!isNone() || !(document->hasEditableStyle() || caretBrowsing))
        return;

    Element* documentElement = document->documentElement();
    if (!documentElement)
        return;
    if (HTMLBodyElement* body = Traversal<HTMLBodyElement>::firstChild(*documentElement))
        setSelection(VisibleSelection(firstPositionInOrBeforeNode(body), TextAffinity::Downstream));
}

void FrameSelection::setShouldShowBlockCursor(bool shouldShowBlockCursor)
{
    m_shouldShowBlockCursor = shouldShowBlockCursor;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    updateAppearance();
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy> FrameSelection::validateSelection(const VisibleSelectionTemplate<Strategy>& selection)
{
    if (!m_frame || selection.isNone())
        return selection;

    const PositionTemplate<Strategy> base = selection.base();
    const PositionTemplate<Strategy> extent = selection.extent();
    bool isBaseValid = base.document() == m_frame->document();
    bool isExtentValid = extent.document() == m_frame->document();

    if (isBaseValid && isExtentValid)
        return selection;

    VisibleSelectionTemplate<Strategy> newSelection;
    if (isBaseValid) {
        newSelection.setWithoutValidation(base, base);
    } else if (isExtentValid) {
        newSelection.setWithoutValidation(extent, extent);
    }
    return newSelection;
}

#ifndef NDEBUG

void FrameSelection::formatForDebugger(char* buffer, unsigned length) const
{
    selection().formatForDebugger(buffer, length);
}

void FrameSelection::showTreeForThis() const
{
    selection().showTreeForThis();
}

#endif

DEFINE_TRACE(FrameSelection)
{
    visitor->trace(m_frame);
    visitor->trace(m_pendingSelection);
    visitor->trace(m_selectionEditor);
    visitor->trace(m_originalBase);
    visitor->trace(m_originalBaseInComposedTree);
    visitor->trace(m_previousCaretNode);
    visitor->trace(m_typingStyle);
}

void FrameSelection::setCaretRectNeedsUpdate()
{
    if (m_caretRectDirty)
        return;
    m_caretRectDirty = true;

    scheduleVisualUpdate();
}

void FrameSelection::scheduleVisualUpdate() const
{
    if (!m_frame)
        return;
    if (Page* page = m_frame->page())
        page->animator().scheduleVisualUpdate(m_frame->localFrameRoot());
}

bool FrameSelection::selectWordAroundPosition(const VisiblePosition& position)
{
    static const EWordSide wordSideList[2] = { RightWordIfOnBoundary, LeftWordIfOnBoundary };
    for (EWordSide wordSide : wordSideList) {
        VisiblePosition start = startOfWord(position, wordSide);
        VisiblePosition end = endOfWord(position, wordSide);
        String text = plainText(EphemeralRange(start.deepEquivalent(), end.deepEquivalent()));
        if (!text.isEmpty() && !isSeparator(text.characterStartingAt(0))) {
            setSelection(VisibleSelection(start, end), WordGranularity);
            return true;
        }
    }

    return false;
}

GranularityStrategy* FrameSelection::granularityStrategy()
{
    // We do lazy initalization for m_granularityStrategy, because if we
    // initialize it right in the constructor - the correct settings may not be
    // set yet.
    SelectionStrategy strategyType = SelectionStrategy::Character;
    Settings* settings = m_frame ? m_frame->settings() : 0;
    if (settings && settings->selectionStrategy() == SelectionStrategy::Direction)
        strategyType = SelectionStrategy::Direction;

    if (m_granularityStrategy && m_granularityStrategy->GetType() == strategyType)
        return m_granularityStrategy.get();

    if (strategyType == SelectionStrategy::Direction)
        m_granularityStrategy = adoptPtr(new DirectionGranularityStrategy());
    else
        m_granularityStrategy = adoptPtr(new CharacterGranularityStrategy());
    return m_granularityStrategy.get();
}

void FrameSelection::moveRangeSelectionExtent(const IntPoint& contentsPoint)
{
    if (isNone())
        return;

    VisibleSelection newSelection = granularityStrategy()->updateExtent(contentsPoint, m_frame);
    setSelection(
        newSelection,
        FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle | FrameSelection::DoNotClearStrategy | UserTriggered,
        CursorAlignOnScroll::IfNeeded,
        CharacterGranularity);
}

void FrameSelection::moveRangeSelection(const VisiblePosition& basePosition, const VisiblePosition& extentPosition, TextGranularity granularity)
{
    VisibleSelection newSelection(basePosition, extentPosition);
    newSelection.expandUsingGranularity(granularity);

    if (newSelection.isNone())
        return;

    setSelection(newSelection, granularity);
}

void FrameSelection::updateIfNeeded()
{
    m_selectionEditor->updateIfNeeded();
}

} // namespace blink

#ifndef NDEBUG

void showTree(const blink::FrameSelection& sel)
{
    sel.showTreeForThis();
}

void showTree(const blink::FrameSelection* sel)
{
    if (sel)
        sel->showTreeForThis();
    else
        fprintf(stderr, "Cannot showTree for (nil) FrameSelection.\n");
}

#endif
