/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nuanti Ltd.
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

#include "config.h"
#include "core/page/FocusController.h"

#include <limits>
#include "HTMLNames.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/events/Event.h"
#include "core/events/EventNames.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Range.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/htmlediting.h" // For firstPositionInOrBeforeNode
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/shadow/HTMLShadowElement.h"
#include "core/page/Chrome.h"
#include "core/page/EditorClient.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/page/FrameTree.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/page/SpatialNavigation.h"
#include "core/rendering/HitTestResult.h"

namespace WebCore {

using namespace HTMLNames;

// FIXME: Some of Node* return values and Node* arguments should be Element*.

FocusNavigationScope::FocusNavigationScope(TreeScope* treeScope)
    : m_rootTreeScope(treeScope)
{
    ASSERT(treeScope);
}

Node* FocusNavigationScope::rootNode() const
{
    return m_rootTreeScope->rootNode();
}

Element* FocusNavigationScope::owner() const
{
    Node* root = rootNode();
    if (root->isShadowRoot()) {
        ShadowRoot* shadowRoot = toShadowRoot(root);
        return shadowRoot->isYoungest() ? shadowRoot->host() : shadowRoot->insertionPoint();
    }
    if (Frame* frame = root->document().frame())
        return frame->ownerElement();
    return 0;
}

FocusNavigationScope FocusNavigationScope::focusNavigationScopeOf(Node* node)
{
    ASSERT(node);
    Node* root = node;
    for (Node* n = node; n; n = n->parentNode())
        root = n;
    // The result is not always a ShadowRoot nor a DocumentNode since
    // a starting node is in an orphaned tree in composed shadow tree.
    return FocusNavigationScope(&root->treeScope());
}

FocusNavigationScope FocusNavigationScope::ownedByNonFocusableFocusScopeOwner(Node* node)
{
    if (isShadowHost(node))
        return FocusNavigationScope::ownedByShadowHost(node);
    ASSERT(isActiveShadowInsertionPoint(node));
    return FocusNavigationScope::ownedByShadowInsertionPoint(toHTMLShadowElement(node));
}

FocusNavigationScope FocusNavigationScope::ownedByShadowHost(Node* node)
{
    ASSERT(isShadowHost(node));
    return FocusNavigationScope(toElement(node)->shadow()->youngestShadowRoot());
}

FocusNavigationScope FocusNavigationScope::ownedByIFrame(HTMLFrameOwnerElement* frame)
{
    ASSERT(frame && frame->contentFrame());
    return FocusNavigationScope(frame->contentFrame()->document());
}

FocusNavigationScope FocusNavigationScope::ownedByShadowInsertionPoint(HTMLShadowElement* shadowInsertionPoint)
{
    ASSERT(shadowInsertionPoint->isActive());
    return FocusNavigationScope(shadowInsertionPoint->olderShadowRoot());
}

static inline void dispatchEventsOnWindowAndFocusedNode(Document* document, bool focused)
{
    // If we have a focused node we should dispatch blur on it before we blur the window.
    // If we have a focused node we should dispatch focus on it after we focus the window.
    // https://bugs.webkit.org/show_bug.cgi?id=27105

    // Do not fire events while modal dialogs are up.  See https://bugs.webkit.org/show_bug.cgi?id=33962
    if (Page* page = document->page()) {
        if (page->defersLoading())
            return;
    }

    if (!focused && document->focusedElement())
        document->focusedElement()->dispatchBlurEvent(0);
    document->dispatchWindowEvent(Event::create(focused ? eventNames().focusEvent : eventNames().blurEvent));
    if (focused && document->focusedElement())
        document->focusedElement()->dispatchFocusEvent(0, FocusDirectionPage);
}

static inline bool hasCustomFocusLogic(Element* element)
{
    return element->isHTMLElement() && toHTMLElement(element)->hasCustomFocusLogic();
}

static inline bool isNonFocusableShadowHost(Node* node)
{
    ASSERT(node);
    if (!node->isElementNode())
        return false;
    Element* element = toElement(node);
    return !element->isFocusable() && isShadowHost(element) && !hasCustomFocusLogic(element);
}

static inline bool isNonKeyboardFocusableShadowHost(Node* node)
{
    ASSERT(node);
    if (!node->isElementNode())
        return false;
    Element* element = toElement(node);
    return !element->isKeyboardFocusable() && isShadowHost(element) && !hasCustomFocusLogic(element);
}

static inline bool isKeyboardFocusableShadowHost(Node* node)
{
    ASSERT(node);
    if (!node->isElementNode())
        return false;
    Element* element = toElement(node);
    return element->isKeyboardFocusable() && isShadowHost(element) && !hasCustomFocusLogic(element);
}

static inline bool isNonFocusableFocusScopeOwner(Node* node)
{
    ASSERT(node);
    return isNonKeyboardFocusableShadowHost(node) || isActiveShadowInsertionPoint(node);
}

static inline int adjustedTabIndex(Node* node)
{
    ASSERT(node);
    return isNonFocusableFocusScopeOwner(node) ? 0 : node->tabIndex();
}

static inline bool shouldVisit(Node* node)
{
    ASSERT(node);
    return (node->isElementNode() && toElement(node)->isKeyboardFocusable()) || isNonFocusableFocusScopeOwner(node);
}

FocusController::FocusController(Page* page)
    : m_page(page)
    , m_isActive(false)
    , m_isFocused(false)
    , m_isChangingFocusedFrame(false)
    , m_containingWindowIsVisible(false)
{
}

PassOwnPtr<FocusController> FocusController::create(Page* page)
{
    return adoptPtr(new FocusController(page));
}

void FocusController::setFocusedFrame(PassRefPtr<Frame> frame)
{
    ASSERT(!frame || frame->page() == m_page);
    if (m_focusedFrame == frame || m_isChangingFocusedFrame)
        return;

    m_isChangingFocusedFrame = true;

    RefPtr<Frame> oldFrame = m_focusedFrame;
    RefPtr<Frame> newFrame = frame;

    m_focusedFrame = newFrame;

    // Now that the frame is updated, fire events and update the selection focused states of both frames.
    if (oldFrame && oldFrame->view()) {
        oldFrame->selection().setFocused(false);
        oldFrame->document()->dispatchWindowEvent(Event::create(eventNames().blurEvent));
    }

    if (newFrame && newFrame->view() && isFocused()) {
        newFrame->selection().setFocused(true);
        newFrame->document()->dispatchWindowEvent(Event::create(eventNames().focusEvent));
    }

    m_isChangingFocusedFrame = false;
}

Frame* FocusController::focusedOrMainFrame() const
{
    if (Frame* frame = focusedFrame())
        return frame;
    return m_page->mainFrame();
}

void FocusController::setFocused(bool focused)
{
    if (isFocused() == focused)
        return;

    m_isFocused = focused;

    if (!m_isFocused)
        focusedOrMainFrame()->eventHandler()->stopAutoscrollTimer();

    if (!m_focusedFrame)
        setFocusedFrame(m_page->mainFrame());

    if (m_focusedFrame->view()) {
        m_focusedFrame->selection().setFocused(focused);
        dispatchEventsOnWindowAndFocusedNode(m_focusedFrame->document(), focused);
    }
}

Node* FocusController::findFocusableNodeDecendingDownIntoFrameDocument(FocusDirection direction, Node* node)
{
    // The node we found might be a HTMLFrameOwnerElement, so descend down the tree until we find either:
    // 1) a focusable node, or
    // 2) the deepest-nested HTMLFrameOwnerElement.
    while (node && node->isFrameOwnerElement()) {
        HTMLFrameOwnerElement* owner = toHTMLFrameOwnerElement(node);
        if (!owner->contentFrame())
            break;
        Node* foundNode = findFocusableNode(direction, FocusNavigationScope::ownedByIFrame(owner), 0);
        if (!foundNode)
            break;
        ASSERT(node != foundNode);
        node = foundNode;
    }
    return node;
}

bool FocusController::setInitialFocus(FocusDirection direction)
{
    bool didAdvanceFocus = advanceFocus(direction, true);

    // If focus is being set initially, accessibility needs to be informed that system focus has moved
    // into the web area again, even if focus did not change within WebCore. PostNotification is called instead
    // of handleFocusedUIElementChanged, because this will send the notification even if the element is the same.
    if (AXObjectCache* cache = focusedOrMainFrame()->document()->existingAXObjectCache())
        cache->postNotification(focusedOrMainFrame()->document(), AXObjectCache::AXFocusedUIElementChanged, true);

    return didAdvanceFocus;
}

bool FocusController::advanceFocus(FocusDirection direction, bool initialFocus)
{
    switch (direction) {
    case FocusDirectionForward:
    case FocusDirectionBackward:
        return advanceFocusInDocumentOrder(direction, initialFocus);
    case FocusDirectionLeft:
    case FocusDirectionRight:
    case FocusDirectionUp:
    case FocusDirectionDown:
        return advanceFocusDirectionally(direction);
    default:
        ASSERT_NOT_REACHED();
    }

    return false;
}

bool FocusController::advanceFocusInDocumentOrder(FocusDirection direction, bool initialFocus)
{
    Frame* frame = focusedOrMainFrame();
    ASSERT(frame);
    Document* document = frame->document();

    Node* currentNode = document->focusedElement();
    // FIXME: Not quite correct when it comes to focus transitions leaving/entering the WebView itself
    bool caretBrowsing = frame->settings() && frame->settings()->caretBrowsingEnabled();

    if (caretBrowsing && !currentNode)
        currentNode = frame->selection().start().deprecatedNode();

    document->updateLayoutIgnorePendingStylesheets();

    RefPtr<Node> node = findFocusableNodeAcrossFocusScope(direction, FocusNavigationScope::focusNavigationScopeOf(currentNode ? currentNode : document), currentNode);

    if (!node) {
        // We didn't find a node to focus, so we should try to pass focus to Chrome.
        if (!initialFocus && m_page->chrome().canTakeFocus(direction)) {
            document->setFocusedElement(0);
            setFocusedFrame(0);
            m_page->chrome().takeFocus(direction);
            return true;
        }

        // Chrome doesn't want focus, so we should wrap focus.
        node = findFocusableNodeRecursively(direction, FocusNavigationScope::focusNavigationScopeOf(m_page->mainFrame()->document()), 0);
        node = findFocusableNodeDecendingDownIntoFrameDocument(direction, node.get());

        if (!node)
            return false;
    }

    ASSERT(node);

    if (node == document->focusedElement())
        // Focus wrapped around to the same node.
        return true;

    if (!node->isElementNode())
        // FIXME: May need a way to focus a document here.
        return false;

    Element* element = toElement(node.get());
    if (element->isFrameOwnerElement() && (!element->isPluginElement() || !element->isKeyboardFocusable())) {
        // We focus frames rather than frame owners.
        // FIXME: We should not focus frames that have no scrollbars, as focusing them isn't useful to the user.
        HTMLFrameOwnerElement* owner = toHTMLFrameOwnerElement(element);
        if (!owner->contentFrame())
            return false;

        document->setFocusedElement(0);
        setFocusedFrame(owner->contentFrame());
        return true;
    }

    // FIXME: It would be nice to just be able to call setFocusedElement(node)
    // here, but we can't do that because some elements (e.g. HTMLInputElement
    // and HTMLTextAreaElement) do extra work in their focus() methods.
    Document& newDocument = element->document();

    if (&newDocument != document) {
        // Focus is going away from this document, so clear the focused node.
        document->setFocusedElement(0);
    }

    setFocusedFrame(newDocument.frame());

    if (caretBrowsing) {
        Position position = firstPositionInOrBeforeNode(element);
        VisibleSelection newSelection(position, position, DOWNSTREAM);
        if (frame->selection().shouldChangeSelection(newSelection))
            frame->selection().setSelection(newSelection);
    }

    element->focus(false, direction);
    return true;
}

Node* FocusController::findFocusableNodeAcrossFocusScope(FocusDirection direction, FocusNavigationScope scope, Node* currentNode)
{
    ASSERT(!currentNode || !isNonFocusableShadowHost(currentNode));
    Node* found;
    if (currentNode && direction == FocusDirectionForward && isKeyboardFocusableShadowHost(currentNode)) {
        Node* foundInInnerFocusScope = findFocusableNodeRecursively(direction, FocusNavigationScope::ownedByShadowHost(currentNode), 0);
        found = foundInInnerFocusScope ? foundInInnerFocusScope : findFocusableNodeRecursively(direction, scope, currentNode);
    } else {
        found = findFocusableNodeRecursively(direction, scope, currentNode);
    }

    // If there's no focusable node to advance to, move up the focus scopes until we find one.
    while (!found) {
        Node* owner = scope.owner();
        if (!owner)
            break;
        scope = FocusNavigationScope::focusNavigationScopeOf(owner);
        if (direction == FocusDirectionBackward && isKeyboardFocusableShadowHost(owner)) {
            found = owner;
            break;
        }
        found = findFocusableNodeRecursively(direction, scope, owner);
    }
    found = findFocusableNodeDecendingDownIntoFrameDocument(direction, found);
    return found;
}

Node* FocusController::findFocusableNodeRecursively(FocusDirection direction, FocusNavigationScope scope, Node* start)
{
    // Starting node is exclusive.
    Node* found = findFocusableNode(direction, scope, start);
    if (!found)
        return 0;
    if (direction == FocusDirectionForward) {
        if (!isNonFocusableFocusScopeOwner(found))
            return found;
        Node* foundInInnerFocusScope = findFocusableNodeRecursively(direction, FocusNavigationScope::ownedByNonFocusableFocusScopeOwner(found), 0);
        return foundInInnerFocusScope ? foundInInnerFocusScope : findFocusableNodeRecursively(direction, scope, found);
    }
    ASSERT(direction == FocusDirectionBackward);
    if (isKeyboardFocusableShadowHost(found)) {
        Node* foundInInnerFocusScope = findFocusableNodeRecursively(direction, FocusNavigationScope::ownedByShadowHost(found), 0);
        return foundInInnerFocusScope ? foundInInnerFocusScope : found;
    }
    if (isNonFocusableFocusScopeOwner(found)) {
        Node* foundInInnerFocusScope = findFocusableNodeRecursively(direction, FocusNavigationScope::ownedByNonFocusableFocusScopeOwner(found), 0);
        return foundInInnerFocusScope ? foundInInnerFocusScope :findFocusableNodeRecursively(direction, scope, found);
    }
    return found;
}

Node* FocusController::findFocusableNode(FocusDirection direction, FocusNavigationScope scope, Node* node)
{
    return (direction == FocusDirectionForward)
        ? nextFocusableNode(scope, node)
        : previousFocusableNode(scope, node);
}

Node* FocusController::findNodeWithExactTabIndex(Node* start, int tabIndex, FocusDirection direction)
{
    // Search is inclusive of start
    for (Node* node = start; node; node = direction == FocusDirectionForward ? NodeTraversal::next(node) : NodeTraversal::previous(node)) {
        if (shouldVisit(node) && adjustedTabIndex(node) == tabIndex)
            return node;
    }
    return 0;
}

static Node* nextNodeWithGreaterTabIndex(Node* start, int tabIndex)
{
    // Search is inclusive of start
    int winningTabIndex = std::numeric_limits<short>::max() + 1;
    Node* winner = 0;
    for (Node* node = start; node; node = NodeTraversal::next(node)) {
        if (shouldVisit(node) && node->tabIndex() > tabIndex && node->tabIndex() < winningTabIndex) {
            winner = node;
            winningTabIndex = node->tabIndex();
        }
    }

    return winner;
}

static Node* previousNodeWithLowerTabIndex(Node* start, int tabIndex)
{
    // Search is inclusive of start
    int winningTabIndex = 0;
    Node* winner = 0;
    for (Node* node = start; node; node = NodeTraversal::previous(node)) {
        int currentTabIndex = adjustedTabIndex(node);
        if ((shouldVisit(node) || isNonKeyboardFocusableShadowHost(node)) && currentTabIndex < tabIndex && currentTabIndex > winningTabIndex) {
            winner = node;
            winningTabIndex = currentTabIndex;
        }
    }
    return winner;
}

Node* FocusController::nextFocusableNode(FocusNavigationScope scope, Node* start)
{
    if (start) {
        int tabIndex = adjustedTabIndex(start);
        // If a node is excluded from the normal tabbing cycle, the next focusable node is determined by tree order
        if (tabIndex < 0) {
            for (Node* node = NodeTraversal::next(start); node; node = NodeTraversal::next(node)) {
                if (shouldVisit(node) && adjustedTabIndex(node) >= 0)
                    return node;
            }
        }

        // First try to find a node with the same tabindex as start that comes after start in the scope.
        if (Node* winner = findNodeWithExactTabIndex(NodeTraversal::next(start), tabIndex, FocusDirectionForward))
            return winner;

        if (!tabIndex)
            // We've reached the last node in the document with a tabindex of 0. This is the end of the tabbing order.
            return 0;
    }

    // Look for the first node in the scope that:
    // 1) has the lowest tabindex that is higher than start's tabindex (or 0, if start is null), and
    // 2) comes first in the scope, if there's a tie.
    if (Node* winner = nextNodeWithGreaterTabIndex(scope.rootNode(), start ? adjustedTabIndex(start) : 0))
        return winner;

    // There are no nodes with a tabindex greater than start's tabindex,
    // so find the first node with a tabindex of 0.
    return findNodeWithExactTabIndex(scope.rootNode(), 0, FocusDirectionForward);
}

Node* FocusController::previousFocusableNode(FocusNavigationScope scope, Node* start)
{
    Node* last = 0;
    for (Node* node = scope.rootNode(); node; node = node->lastChild())
        last = node;
    ASSERT(last);

    // First try to find the last node in the scope that comes before start and has the same tabindex as start.
    // If start is null, find the last node in the scope with a tabindex of 0.
    Node* startingNode;
    int startingTabIndex;
    if (start) {
        startingNode = NodeTraversal::previous(start);
        startingTabIndex = adjustedTabIndex(start);
    } else {
        startingNode = last;
        startingTabIndex = 0;
    }

    // However, if a node is excluded from the normal tabbing cycle, the previous focusable node is determined by tree order
    if (startingTabIndex < 0) {
        for (Node* node = startingNode; node; node = NodeTraversal::previous(node)) {
            if (shouldVisit(node) && adjustedTabIndex(node) >= 0)
                return node;
        }
    }

    if (Node* winner = findNodeWithExactTabIndex(startingNode, startingTabIndex, FocusDirectionBackward))
        return winner;

    // There are no nodes before start with the same tabindex as start, so look for a node that:
    // 1) has the highest non-zero tabindex (that is less than start's tabindex), and
    // 2) comes last in the scope, if there's a tie.
    startingTabIndex = (start && startingTabIndex) ? startingTabIndex : std::numeric_limits<short>::max();
    return previousNodeWithLowerTabIndex(last, startingTabIndex);
}

static bool relinquishesEditingFocus(Node *node)
{
    ASSERT(node);
    ASSERT(node->rendererIsEditable());

    Node* root = node->rootEditableElement();
    Frame* frame = node->document().frame();
    if (!frame || !root)
        return false;

    return frame->editor().shouldEndEditing(rangeOfContents(root).get());
}

static void clearSelectionIfNeeded(Frame* oldFocusedFrame, Frame* newFocusedFrame, Node* newFocusedNode)
{
    if (!oldFocusedFrame || !newFocusedFrame)
        return;

    if (oldFocusedFrame->document() != newFocusedFrame->document())
        return;

    FrameSelection& selection = oldFocusedFrame->selection();
    if (selection.isNone())
        return;

    bool caretBrowsing = oldFocusedFrame->settings()->caretBrowsingEnabled();
    if (caretBrowsing)
        return;

    Node* selectionStartNode = selection.selection().start().deprecatedNode();
    if (selectionStartNode == newFocusedNode || selectionStartNode->isDescendantOf(newFocusedNode) || selectionStartNode->deprecatedShadowAncestorNode() == newFocusedNode)
        return;

    if (Node* mousePressNode = newFocusedFrame->eventHandler()->mousePressNode()) {
        if (mousePressNode->renderer() && !mousePressNode->canStartSelection()) {
            // Don't clear the selection for contentEditable elements, but do clear it for input and textarea. See bug 38696.
            Node* root = selection.rootEditableElement();
            if (!root)
                return;

            if (Node* shadowAncestorNode = root->deprecatedShadowAncestorNode()) {
                if (!shadowAncestorNode->hasTagName(inputTag) && !isHTMLTextAreaElement(shadowAncestorNode))
                    return;
            }
        }
    }

    selection.clear();
}

bool FocusController::setFocusedElement(Element* element, PassRefPtr<Frame> newFocusedFrame, FocusDirection direction)
{
    RefPtr<Frame> oldFocusedFrame = focusedFrame();
    RefPtr<Document> oldDocument = oldFocusedFrame ? oldFocusedFrame->document() : 0;

    Element* oldFocusedElement = oldDocument ? oldDocument->focusedElement() : 0;
    if (oldFocusedElement == element)
        return true;

    // FIXME: Might want to disable this check for caretBrowsing
    if (oldFocusedElement && oldFocusedElement->isRootEditableElement() && !relinquishesEditingFocus(oldFocusedElement))
        return false;

    m_page->editorClient().willSetInputMethodState();

    clearSelectionIfNeeded(oldFocusedFrame.get(), newFocusedFrame.get(), element);

    if (!element) {
        if (oldDocument)
            oldDocument->setFocusedElement(0);
        return true;
    }

    RefPtr<Document> newDocument = &element->document();

    if (newDocument && newDocument->focusedElement() == element)
        return true;

    if (oldDocument && oldDocument != newDocument)
        oldDocument->setFocusedElement(0);

    if (newFocusedFrame && !newFocusedFrame->page()) {
        setFocusedFrame(0);
        return false;
    }
    setFocusedFrame(newFocusedFrame);

    // Setting the focused node can result in losing our last reft to node when JS event handlers fire.
    RefPtr<Element> protect = element;
    if (newDocument) {
        bool successfullyFocused = newDocument->setFocusedElement(element, direction);
        if (!successfullyFocused)
            return false;
    }

    return true;
}

void FocusController::setActive(bool active)
{
    if (m_isActive == active)
        return;

    m_isActive = active;

    if (FrameView* view = m_page->mainFrame()->view()) {
        view->updateLayoutAndStyleIfNeededRecursive();
        view->updateControlTints();
    }

    focusedOrMainFrame()->selection().pageActivationChanged();
}

static void contentAreaDidShowOrHide(ScrollableArea* scrollableArea, bool didShow)
{
    if (didShow)
        scrollableArea->contentAreaDidShow();
    else
        scrollableArea->contentAreaDidHide();
}

void FocusController::setContainingWindowIsVisible(bool containingWindowIsVisible)
{
    if (m_containingWindowIsVisible == containingWindowIsVisible)
        return;

    m_containingWindowIsVisible = containingWindowIsVisible;

    FrameView* view = m_page->mainFrame()->view();
    if (!view)
        return;

    contentAreaDidShowOrHide(view, containingWindowIsVisible);

    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        FrameView* frameView = frame->view();
        if (!frameView)
            continue;

        const HashSet<ScrollableArea*>* scrollableAreas = frameView->scrollableAreas();
        if (!scrollableAreas)
            continue;

        for (HashSet<ScrollableArea*>::const_iterator it = scrollableAreas->begin(), end = scrollableAreas->end(); it != end; ++it) {
            ScrollableArea* scrollableArea = *it;
            ASSERT(scrollableArea->scrollbarsCanBeActive());

            contentAreaDidShowOrHide(scrollableArea, containingWindowIsVisible);
        }
    }
}

static void updateFocusCandidateIfNeeded(FocusDirection direction, const FocusCandidate& current, FocusCandidate& candidate, FocusCandidate& closest)
{
    ASSERT(candidate.visibleNode->isElementNode());
    ASSERT(candidate.visibleNode->renderer());

    // Ignore iframes that don't have a src attribute
    if (frameOwnerElement(candidate) && (!frameOwnerElement(candidate)->contentFrame() || candidate.rect.isEmpty()))
        return;

    // Ignore off screen child nodes of containers that do not scroll (overflow:hidden)
    if (candidate.isOffscreen && !canBeScrolledIntoView(direction, candidate))
        return;

    distanceDataForNode(direction, current, candidate);
    if (candidate.distance == maxDistance())
        return;

    if (candidate.isOffscreenAfterScrolling && candidate.alignment < Full)
        return;

    if (closest.isNull()) {
        closest = candidate;
        return;
    }

    LayoutRect intersectionRect = intersection(candidate.rect, closest.rect);
    if (!intersectionRect.isEmpty() && !areElementsOnSameLine(closest, candidate)) {
        // If 2 nodes are intersecting, do hit test to find which node in on top.
        LayoutUnit x = intersectionRect.x() + intersectionRect.width() / 2;
        LayoutUnit y = intersectionRect.y() + intersectionRect.height() / 2;
        HitTestResult result = candidate.visibleNode->document().page()->mainFrame()->eventHandler()->hitTestResultAtPoint(IntPoint(x, y), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowShadowContent);
        if (candidate.visibleNode->contains(result.innerNode())) {
            closest = candidate;
            return;
        }
        if (closest.visibleNode->contains(result.innerNode()))
            return;
    }

    if (candidate.alignment == closest.alignment) {
        if (candidate.distance < closest.distance)
            closest = candidate;
        return;
    }

    if (candidate.alignment > closest.alignment)
        closest = candidate;
}

void FocusController::findFocusCandidateInContainer(Node* container, const LayoutRect& startingRect, FocusDirection direction, FocusCandidate& closest)
{
    ASSERT(container);
    Element* focusedElement = (focusedFrame() && focusedFrame()->document()) ? focusedFrame()->document()->focusedElement() : 0;

    Element* element = ElementTraversal::firstWithin(container);
    FocusCandidate current;
    current.rect = startingRect;
    current.focusableNode = focusedElement;
    current.visibleNode = focusedElement;

    for (; element; element = (element->isFrameOwnerElement() || canScrollInDirection(element, direction))
        ? ElementTraversal::nextSkippingChildren(element, container)
        : ElementTraversal::next(element, container)) {
        if (element == focusedElement)
            continue;

        if (!element->isKeyboardFocusable() && !element->isFrameOwnerElement() && !canScrollInDirection(element, direction))
            continue;

        FocusCandidate candidate = FocusCandidate(element, direction);
        if (candidate.isNull())
            continue;

        candidate.enclosingScrollableBox = container;
        updateFocusCandidateIfNeeded(direction, current, candidate, closest);
    }
}

bool FocusController::advanceFocusDirectionallyInContainer(Node* container, const LayoutRect& startingRect, FocusDirection direction)
{
    if (!container)
        return false;

    LayoutRect newStartingRect = startingRect;

    if (startingRect.isEmpty())
        newStartingRect = virtualRectForDirection(direction, nodeRectInAbsoluteCoordinates(container));

    // Find the closest node within current container in the direction of the navigation.
    FocusCandidate focusCandidate;
    findFocusCandidateInContainer(container, newStartingRect, direction, focusCandidate);

    if (focusCandidate.isNull()) {
        // Nothing to focus, scroll if possible.
        // NOTE: If no scrolling is performed (i.e. scrollInDirection returns false), the
        // spatial navigation algorithm will skip this container.
        return scrollInDirection(container, direction);
    }

    if (HTMLFrameOwnerElement* frameElement = frameOwnerElement(focusCandidate)) {
        // If we have an iframe without the src attribute, it will not have a contentFrame().
        // We ASSERT here to make sure that
        // updateFocusCandidateIfNeeded() will never consider such an iframe as a candidate.
        ASSERT(frameElement->contentFrame());

        if (focusCandidate.isOffscreenAfterScrolling) {
            scrollInDirection(&focusCandidate.visibleNode->document(), direction);
            return true;
        }
        // Navigate into a new frame.
        LayoutRect rect;
        Element* focusedElement = focusedOrMainFrame()->document()->focusedElement();
        if (focusedElement && !hasOffscreenRect(focusedElement))
            rect = nodeRectInAbsoluteCoordinates(focusedElement, true /* ignore border */);
        frameElement->contentFrame()->document()->updateLayoutIgnorePendingStylesheets();
        if (!advanceFocusDirectionallyInContainer(frameElement->contentFrame()->document(), rect, direction)) {
            // The new frame had nothing interesting, need to find another candidate.
            return advanceFocusDirectionallyInContainer(container, nodeRectInAbsoluteCoordinates(focusCandidate.visibleNode, true), direction);
        }
        return true;
    }

    if (canScrollInDirection(focusCandidate.visibleNode, direction)) {
        if (focusCandidate.isOffscreenAfterScrolling) {
            scrollInDirection(focusCandidate.visibleNode, direction);
            return true;
        }
        // Navigate into a new scrollable container.
        LayoutRect startingRect;
        Element* focusedElement = focusedOrMainFrame()->document()->focusedElement();
        if (focusedElement && !hasOffscreenRect(focusedElement))
            startingRect = nodeRectInAbsoluteCoordinates(focusedElement, true);
        return advanceFocusDirectionallyInContainer(focusCandidate.visibleNode, startingRect, direction);
    }
    if (focusCandidate.isOffscreenAfterScrolling) {
        Node* container = focusCandidate.enclosingScrollableBox;
        scrollInDirection(container, direction);
        return true;
    }

    // We found a new focus node, navigate to it.
    Element* element = toElement(focusCandidate.focusableNode);
    ASSERT(element);

    element->focus(false, direction);
    return true;
}

bool FocusController::advanceFocusDirectionally(FocusDirection direction)
{
    Frame* curFrame = focusedOrMainFrame();
    ASSERT(curFrame);

    Document* focusedDocument = curFrame->document();
    if (!focusedDocument)
        return false;

    Element* focusedElement = focusedDocument->focusedElement();
    Node* container = focusedDocument;

    if (container->isDocumentNode())
        toDocument(container)->updateLayoutIgnorePendingStylesheets();

    // Figure out the starting rect.
    LayoutRect startingRect;
    if (focusedElement) {
        if (!hasOffscreenRect(focusedElement)) {
            container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(direction, focusedElement);
            startingRect = nodeRectInAbsoluteCoordinates(focusedElement, true /* ignore border */);
        } else if (isHTMLAreaElement(focusedElement)) {
            HTMLAreaElement* area = toHTMLAreaElement(focusedElement);
            container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(direction, area->imageElement());
            startingRect = virtualRectForAreaElementAndDirection(area, direction);
        }
    }

    bool consumed = false;
    do {
        consumed = advanceFocusDirectionallyInContainer(container, startingRect, direction);
        startingRect = nodeRectInAbsoluteCoordinates(container, true /* ignore border */);
        container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(direction, container);
        if (container && container->isDocumentNode())
            toDocument(container)->updateLayoutIgnorePendingStylesheets();
    } while (!consumed && container);

    return consumed;
}

} // namespace WebCore
