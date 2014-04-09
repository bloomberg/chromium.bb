/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/dom/ContainerNode.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ChildListMutationScope.h"
#include "core/dom/ClassCollection.h"
#include "core/dom/ContainerNodeAlgorithms.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/NameNodeList.h"
#include "core/dom/NodeChildRemovalTracker.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/SelectorQuery.h"
#include "core/events/MutationEvent.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/RadioNodeList.h"
#include "core/rendering/InlineTextBox.h"
#include "core/rendering/RenderText.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

static void dispatchChildInsertionEvents(Node& child);
static void dispatchChildRemovalEvents(Node& child);

ChildNodesLazySnapshot* ChildNodesLazySnapshot::latestSnapshot = 0;

#ifndef NDEBUG
unsigned NoEventDispatchAssertion::s_count = 0;
#endif

static void collectChildrenAndRemoveFromOldParent(Node& node, NodeVector& nodes, ExceptionState& exceptionState)
{
    if (!node.isDocumentFragment()) {
        nodes.append(&node);
        if (ContainerNode* oldParent = node.parentNode())
            oldParent->removeChild(&node, exceptionState);
        return;
    }
    getChildNodes(node, nodes);
    toContainerNode(node).removeChildren();
}

void ContainerNode::removeDetachedChildren()
{
    if (connectedSubframeCount()) {
        for (Node* child = firstChild(); child; child = child->nextSibling())
            child->updateAncestorConnectedSubframeCountForRemoval();
    }
    ASSERT(needsAttach());
    removeDetachedChildrenInContainer<Node, ContainerNode>(*this);
}

void ContainerNode::parserTakeAllChildrenFrom(ContainerNode& oldParent)
{
    while (RefPtr<Node> child = oldParent.firstChild()) {
        oldParent.parserRemoveChild(*child);
        treeScope().adoptIfNeeded(*child);
        parserAppendChild(child.get());
    }
}

ContainerNode::~ContainerNode()
{
    willBeDeletedFromDocument();
    removeDetachedChildren();
}

bool ContainerNode::isChildTypeAllowed(const Node& child) const
{
    if (!child.isDocumentFragment())
        return childTypeAllowed(child.nodeType());

    for (Node* node = child.firstChild(); node; node = node->nextSibling()) {
        if (!childTypeAllowed(node->nodeType()))
            return false;
    }
    return true;
}

bool ContainerNode::containsConsideringHostElements(const Node& newChild) const
{
    if (isInShadowTree() || document().isTemplateDocument())
        return newChild.containsIncludingHostElements(*this);
    return newChild.contains(this);
}

bool ContainerNode::checkAcceptChild(const Node* newChild, const Node* oldChild, ExceptionState& exceptionState) const
{
    // Not mentioned in spec: throw NotFoundError if newChild is null
    if (!newChild) {
        exceptionState.throwDOMException(NotFoundError, "The new child element is null.");
        return false;
    }

    // Use common case fast path if possible.
    if ((newChild->isElementNode() || newChild->isTextNode()) && isElementNode()) {
        ASSERT(isChildTypeAllowed(*newChild));
        if (containsConsideringHostElements(*newChild)) {
            exceptionState.throwDOMException(HierarchyRequestError, "The new child element contains the parent.");
            return false;
        }
        return true;
    }

    // This should never happen, but also protect release builds from tree corruption.
    ASSERT(!newChild->isPseudoElement());
    if (newChild->isPseudoElement()) {
        exceptionState.throwDOMException(HierarchyRequestError, "The new child element is a pseudo-element.");
        return false;
    }

    if (containsConsideringHostElements(*newChild)) {
        exceptionState.throwDOMException(HierarchyRequestError, "The new child element contains the parent.");
        return false;
    }

    if (oldChild && isDocumentNode()) {
        if (!toDocument(this)->canReplaceChild(*newChild, *oldChild)) {
            // FIXME: Adjust 'Document::canReplaceChild' to return some additional detail (or an error message).
            exceptionState.throwDOMException(HierarchyRequestError, "Failed to replace child.");
            return false;
        }
    } else if (!isChildTypeAllowed(*newChild)) {
        exceptionState.throwDOMException(HierarchyRequestError, "Nodes of type '" + newChild->nodeName() + "' may not be inserted inside nodes of type '" + nodeName() + "'.");
        return false;
    }

    return true;
}

bool ContainerNode::checkAcceptChildGuaranteedNodeTypes(const Node& newChild, ExceptionState& exceptionState) const
{
    ASSERT(isChildTypeAllowed(newChild));
    if (newChild.contains(this)) {
        exceptionState.throwDOMException(HierarchyRequestError, "The new child element contains the parent.");
        return false;
    }
    return true;
}

void ContainerNode::insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionState& exceptionState)
{
#if !ENABLE(OILPAN)
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());
#endif

    RefPtr<Node> protect(this);

    // insertBefore(node, 0) is equivalent to appendChild(node)
    if (!refChild) {
        appendChild(newChild, exceptionState);
        return;
    }

    // Make sure adding the new child is OK.
    if (!checkAcceptChild(newChild.get(), 0, exceptionState))
        return;
    ASSERT(newChild);

    // NotFoundError: Raised if refChild is not a child of this node
    if (refChild->parentNode() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node before which the new node is to be inserted is not a child of this node.");
        return;
    }

    if (refChild->previousSibling() == newChild || refChild == newChild) // nothing to do
        return;

    RefPtr<Node> next = refChild;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(*newChild, targets, exceptionState);
    if (exceptionState.hadException())
        return;
    if (targets.isEmpty())
        return;

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    if (!checkAcceptChildGuaranteedNodeTypes(*newChild, exceptionState))
        return;

    InspectorInstrumentation::willInsertDOMNode(this);

    ChildListMutationScope mutation(*this);
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        ASSERT(*it);
        Node& child = **it;

        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "next" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (next->parentNode() != this)
            break;
        if (child.parentNode())
            break;

        treeScope().adoptIfNeeded(child);

        insertBeforeCommon(*next, child);

        updateTreeAfterInsertion(child);
    }

    dispatchSubtreeModifiedEvent();
}

void ContainerNode::insertBeforeCommon(Node& nextChild, Node& newChild)
{
    NoEventDispatchAssertion assertNoEventDispatch;

    ASSERT(!newChild.parentNode()); // Use insertBefore if you need to handle reparenting (and want DOM mutation events).
    ASSERT(!newChild.nextSibling());
    ASSERT(!newChild.previousSibling());
    ASSERT(!newChild.isShadowRoot());

    Node* prev = nextChild.previousSibling();
    ASSERT(m_lastChild != prev);
    nextChild.setPreviousSibling(&newChild);
    if (prev) {
        ASSERT(m_firstChild != nextChild);
        ASSERT(prev->nextSibling() == nextChild);
        prev->setNextSibling(&newChild);
    } else {
        ASSERT(m_firstChild == nextChild);
        m_firstChild = &newChild;
    }
    newChild.setParentOrShadowHostNode(this);
    newChild.setPreviousSibling(prev);
    newChild.setNextSibling(&nextChild);
}

void ContainerNode::parserInsertBefore(PassRefPtr<Node> newChild, Node& nextChild)
{
    ASSERT(newChild);
    ASSERT(nextChild.parentNode() == this);
    ASSERT(!newChild->isDocumentFragment());
    ASSERT(!isHTMLTemplateElement(this));

    if (nextChild.previousSibling() == newChild || nextChild == newChild) // nothing to do
        return;

    if (document() != newChild->document())
        document().adoptNode(newChild.get(), ASSERT_NO_EXCEPTION);

    insertBeforeCommon(nextChild, *newChild);

    newChild->updateAncestorConnectedSubframeCountForInsertion();

    ChildListMutationScope(*this).childAdded(*newChild);

    childrenChanged(true, newChild->previousSibling(), &nextChild, 1);

    ChildNodeInsertionNotifier(*this).notify(*newChild);
}

void ContainerNode::replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionState& exceptionState)
{
#if !ENABLE(OILPAN)
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());
#endif

    RefPtr<Node> protect(this);

    if (oldChild == newChild) // nothing to do
        return;

    if (!oldChild) {
        exceptionState.throwDOMException(NotFoundError, "The node to be replaced is null.");
        return;
    }

    // Make sure replacing the old child with the new is ok
    if (!checkAcceptChild(newChild.get(), oldChild, exceptionState))
        return;

    // NotFoundError: Raised if oldChild is not a child of this node.
    if (oldChild->parentNode() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node to be replaced is not a child of this node.");
        return;
    }

    ChildListMutationScope mutation(*this);

    RefPtr<Node> next = oldChild->nextSibling();

    // Remove the node we're replacing
    RefPtr<Node> removedChild = oldChild;
    removeChild(oldChild, exceptionState);
    if (exceptionState.hadException())
        return;

    if (next && (next->previousSibling() == newChild || next == newChild)) // nothing to do
        return;

    // Does this one more time because removeChild() fires a MutationEvent.
    if (!checkAcceptChild(newChild.get(), oldChild, exceptionState))
        return;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(*newChild, targets, exceptionState);
    if (exceptionState.hadException())
        return;

    // Does this yet another check because collectChildrenAndRemoveFromOldParent() fires a MutationEvent.
    if (!checkAcceptChild(newChild.get(), oldChild, exceptionState))
        return;

    InspectorInstrumentation::willInsertDOMNode(this);

    // Add the new child(ren)
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        ASSERT(*it);
        Node& child = **it;

        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "next" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (next && next->parentNode() != this)
            break;
        if (child.parentNode())
            break;

        treeScope().adoptIfNeeded(child);

        // Add child before "next".
        {
            NoEventDispatchAssertion assertNoEventDispatch;
            if (next)
                insertBeforeCommon(*next, child);
            else
                appendChildToContainer(child, *this);
        }

        updateTreeAfterInsertion(child);
    }

    dispatchSubtreeModifiedEvent();
}

void ContainerNode::willRemoveChild(Node& child)
{
    ASSERT(child.parentNode() == this);
    ChildListMutationScope(*this).willRemoveChild(child);
    child.notifyMutationObserversNodeWillDetach();
    dispatchChildRemovalEvents(child);
    document().nodeWillBeRemoved(child); // e.g. mutation event listener can create a new range.
    ChildFrameDisconnector(child).disconnect();
}

void ContainerNode::willRemoveChildren()
{
    NodeVector children;
    getChildNodes(*this, children);

    ChildListMutationScope mutation(*this);
    for (NodeVector::const_iterator it = children.begin(); it != children.end(); ++it) {
        ASSERT(*it);
        Node& child = **it;
        mutation.willRemoveChild(child);
        child.notifyMutationObserversNodeWillDetach();
        dispatchChildRemovalEvents(child);
    }

    ChildFrameDisconnector(*this).disconnect(ChildFrameDisconnector::DescendantsOnly);
}

void ContainerNode::disconnectDescendantFrames()
{
    ChildFrameDisconnector(*this).disconnect();
}

void ContainerNode::removeChild(Node* oldChild, ExceptionState& exceptionState)
{
#if !ENABLE(OILPAN)
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());
#endif

    RefPtr<Node> protect(this);

    // NotFoundError: Raised if oldChild is not a child of this node.
    // FIXME: We should never really get PseudoElements in here, but editing will sometimes
    // attempt to remove them still. We should fix that and enable this ASSERT.
    // ASSERT(!oldChild->isPseudoElement())
    if (!oldChild || oldChild->parentNode() != this || oldChild->isPseudoElement()) {
        exceptionState.throwDOMException(NotFoundError, "The node to be removed is not a child of this node.");
        return;
    }

    RefPtr<Node> child = oldChild;

    document().removeFocusedElementOfSubtree(child.get());

    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(document()))
        fullscreen->removeFullScreenElementOfSubtree(child.get());

    // Events fired when blurring currently focused node might have moved this
    // child into a different parent.
    if (child->parentNode() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node to be removed is no longer a child of this node. Perhaps it was moved in a 'blur' event handler?");
        return;
    }

    willRemoveChild(*child);

    // Mutation events might have moved this child into a different parent.
    if (child->parentNode() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node to be removed is no longer a child of this node. Perhaps it was moved in response to a mutation?");
        return;
    }

    {
        HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;

        Node* prev = child->previousSibling();
        Node* next = child->nextSibling();
        removeBetween(prev, next, *child);
        childrenChanged(false, prev, next, -1);
        ChildNodeRemovalNotifier(*this).notify(*child);
    }
    dispatchSubtreeModifiedEvent();
}

void ContainerNode::removeBetween(Node* previousChild, Node* nextChild, Node& oldChild)
{
    NoEventDispatchAssertion assertNoEventDispatch;

    ASSERT(oldChild.parentNode() == this);

    if (!oldChild.needsAttach())
        oldChild.detach();

    if (nextChild)
        nextChild->setPreviousSibling(previousChild);
    if (previousChild)
        previousChild->setNextSibling(nextChild);
    if (m_firstChild == oldChild)
        m_firstChild = nextChild;
    if (m_lastChild == oldChild)
        m_lastChild = previousChild;

    oldChild.setPreviousSibling(0);
    oldChild.setNextSibling(0);
    oldChild.setParentOrShadowHostNode(0);

    document().adoptIfNeeded(oldChild);
}

void ContainerNode::parserRemoveChild(Node& oldChild)
{
    ASSERT(oldChild.parentNode() == this);
    ASSERT(!oldChild.isDocumentFragment());

    Node* prev = oldChild.previousSibling();
    Node* next = oldChild.nextSibling();

    oldChild.updateAncestorConnectedSubframeCountForRemoval();

    ChildListMutationScope(*this).willRemoveChild(oldChild);
    oldChild.notifyMutationObserversNodeWillDetach();

    removeBetween(prev, next, oldChild);

    childrenChanged(true, prev, next, -1);
    ChildNodeRemovalNotifier(*this).notify(oldChild);
}

// this differs from other remove functions because it forcibly removes all the children,
// regardless of read-only status or event exceptions, e.g.
void ContainerNode::removeChildren()
{
    if (!m_firstChild)
        return;

    // The container node can be removed from event handlers.
    RefPtr<ContainerNode> protect(this);

    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(document()))
        fullscreen->removeFullScreenElementOfSubtree(this, true);

    // Do any prep work needed before actually starting to detach
    // and remove... e.g. stop loading frames, fire unload events.
    willRemoveChildren();

    {
        // Removing focus can cause frames to load, either via events (focusout, blur)
        // or widget updates (e.g., for <embed>).
        SubframeLoadingDisabler disabler(*this);

        // Exclude this node when looking for removed focusedElement since only
        // children will be removed.
        // This must be later than willRemoveChildren, which might change focus
        // state of a child.
        document().removeFocusedElementOfSubtree(this, true);

        // Removing a node from a selection can cause widget updates.
        document().nodeChildrenWillBeRemoved(*this);
    }


    NodeVector removedChildren;
    {
        HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;
        {
            NoEventDispatchAssertion assertNoEventDispatch;
            removedChildren.reserveInitialCapacity(countChildren());
            while (m_firstChild) {
                removedChildren.append(m_firstChild);
                removeBetween(0, m_firstChild->nextSibling(), *m_firstChild);
            }
        }

        childrenChanged(false, 0, 0, -static_cast<int>(removedChildren.size()));

        for (size_t i = 0; i < removedChildren.size(); ++i)
            ChildNodeRemovalNotifier(*this).notify(*removedChildren[i]);
    }

    dispatchSubtreeModifiedEvent();
}

void ContainerNode::appendChild(PassRefPtr<Node> newChild, ExceptionState& exceptionState)
{
    RefPtr<ContainerNode> protect(this);

    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    // Make sure adding the new child is ok
    if (!checkAcceptChild(newChild.get(), 0, exceptionState))
        return;
    ASSERT(newChild);

    if (newChild == m_lastChild) // nothing to do
        return;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(*newChild, targets, exceptionState);
    if (exceptionState.hadException())
        return;

    if (targets.isEmpty())
        return;

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    if (!checkAcceptChildGuaranteedNodeTypes(*newChild, exceptionState))
        return;

    InspectorInstrumentation::willInsertDOMNode(this);

    // Now actually add the child(ren)
    ChildListMutationScope mutation(*this);
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        ASSERT(*it);
        Node& child = **it;

        // If the child has a parent again, just stop what we're doing, because
        // that means someone is doing something with DOM mutation -- can't re-parent
        // a child that already has a parent.
        if (child.parentNode())
            break;

        treeScope().adoptIfNeeded(child);

        // Append child to the end of the list
        {
            NoEventDispatchAssertion assertNoEventDispatch;
            appendChildToContainer(child, *this);
        }

        updateTreeAfterInsertion(child);
    }

    dispatchSubtreeModifiedEvent();
}

void ContainerNode::parserAppendChild(PassRefPtr<Node> newChild)
{
    ASSERT(newChild);
    ASSERT(!newChild->parentNode()); // Use appendChild if you need to handle reparenting (and want DOM mutation events).
    ASSERT(!newChild->isDocumentFragment());
    ASSERT(!isHTMLTemplateElement(this));

    if (document() != newChild->document())
        document().adoptNode(newChild.get(), ASSERT_NO_EXCEPTION);

    Node* last = m_lastChild;
    {
        NoEventDispatchAssertion assertNoEventDispatch;
        // FIXME: This method should take a PassRefPtr.
        appendChildToContainer(*newChild, *this);
        treeScope().adoptIfNeeded(*newChild);
    }

    newChild->updateAncestorConnectedSubframeCountForInsertion();

    ChildListMutationScope(*this).childAdded(*newChild);

    childrenChanged(true, last, 0, 1);
    ChildNodeInsertionNotifier(*this).notify(*newChild);
}

void ContainerNode::attach(const AttachContext& context)
{
    attachChildren(context);
    clearChildNeedsStyleRecalc();
    Node::attach(context);
}

void ContainerNode::detach(const AttachContext& context)
{
    detachChildren(context);
    clearChildNeedsStyleRecalc();
    Node::detach(context);
}

void ContainerNode::childrenChanged(bool changedByParser, Node*, Node*, int childCountDelta)
{
    document().incDOMTreeVersion();
    if (!changedByParser && childCountDelta)
        document().updateRangesAfterChildrenChanged(this);
    invalidateNodeListCachesInAncestors();
    if (childCountDelta > 0 && !childNeedsStyleRecalc()) {
        setChildNeedsStyleRecalc();
        markAncestorsWithChildNeedsStyleRecalc();
    }
}

void ContainerNode::cloneChildNodes(ContainerNode *clone)
{
    TrackExceptionState exceptionState;
    for (Node* n = firstChild(); n && !exceptionState.hadException(); n = n->nextSibling())
        clone->appendChild(n->cloneNode(true), exceptionState);
}


bool ContainerNode::getUpperLeftCorner(FloatPoint& point) const
{
    if (!renderer())
        return false;
    // What is this code really trying to do?
    RenderObject* o = renderer();

    if (!o->isInline() || o->isReplaced()) {
        point = o->localToAbsolute(FloatPoint(), UseTransforms);
        return true;
    }

    // find the next text/image child, to get a position
    while (o) {
        RenderObject* p = o;
        if (o->firstChild()) {
            o = o->firstChild();
        } else if (o->nextSibling()) {
            o = o->nextSibling();
        } else {
            RenderObject* next = 0;
            while (!next && o->parent()) {
                o = o->parent();
                next = o->nextSibling();
            }
            o = next;

            if (!o)
                break;
        }
        ASSERT(o);

        if (!o->isInline() || o->isReplaced()) {
            point = o->localToAbsolute(FloatPoint(), UseTransforms);
            return true;
        }

        if (p->node() && p->node() == this && o->isText() && !o->isBR() && !toRenderText(o)->firstTextBox()) {
            // do nothing - skip unrendered whitespace that is a child or next sibling of the anchor
        } else if ((o->isText() && !o->isBR()) || o->isReplaced()) {
            point = FloatPoint();
            if (o->isText() && toRenderText(o)->firstTextBox()) {
                point.move(toRenderText(o)->linesBoundingBox().x(), toRenderText(o)->firstTextBox()->root().lineTop().toFloat());
            } else if (o->isBox()) {
                RenderBox* box = toRenderBox(o);
                point.moveBy(box->location());
            }
            point = o->container()->localToAbsolute(point, UseTransforms);
            return true;
        }
    }

    // If the target doesn't have any children or siblings that could be used to calculate the scroll position, we must be
    // at the end of the document. Scroll to the bottom. FIXME: who said anything about scrolling?
    if (!o && document().view()) {
        point = FloatPoint(0, document().view()->contentsHeight());
        return true;
    }
    return false;
}

bool ContainerNode::getLowerRightCorner(FloatPoint& point) const
{
    if (!renderer())
        return false;

    RenderObject* o = renderer();
    if (!o->isInline() || o->isReplaced()) {
        RenderBox* box = toRenderBox(o);
        point = o->localToAbsolute(LayoutPoint(box->size()), UseTransforms);
        return true;
    }

    // find the last text/image child, to get a position
    while (o) {
        if (o->lastChild()) {
            o = o->lastChild();
        } else if (o->previousSibling()) {
            o = o->previousSibling();
        } else {
            RenderObject* prev = 0;
        while (!prev) {
            o = o->parent();
            if (!o)
                return false;
            prev = o->previousSibling();
        }
        o = prev;
        }
        ASSERT(o);
        if (o->isText() || o->isReplaced()) {
            point = FloatPoint();
            if (o->isText()) {
                RenderText* text = toRenderText(o);
                IntRect linesBox = text->linesBoundingBox();
                if (!linesBox.maxX() && !linesBox.maxY())
                    continue;
                point.moveBy(linesBox.maxXMaxYCorner());
            } else {
                RenderBox* box = toRenderBox(o);
                point.moveBy(box->frameRect().maxXMaxYCorner());
            }
            point = o->container()->localToAbsolute(point, UseTransforms);
            return true;
        }
    }
    return true;
}

// FIXME: This override is only needed for inline anchors without an
// InlineBox and it does not belong in ContainerNode as it reaches into
// the render and line box trees.
// https://code.google.com/p/chromium/issues/detail?id=248354
LayoutRect ContainerNode::boundingBox() const
{
    FloatPoint upperLeft, lowerRight;
    bool foundUpperLeft = getUpperLeftCorner(upperLeft);
    bool foundLowerRight = getLowerRightCorner(lowerRight);

    // If we've found one corner, but not the other,
    // then we should just return a point at the corner that we did find.
    if (foundUpperLeft != foundLowerRight) {
        if (foundUpperLeft)
            lowerRight = upperLeft;
        else
            upperLeft = lowerRight;
    }

    return enclosingLayoutRect(FloatRect(upperLeft, lowerRight.expandedTo(upperLeft) - upperLeft));
}

// This is used by FrameSelection to denote when the active-state of the page has changed
// independent of the focused element changing.
void ContainerNode::focusStateChanged()
{
    // If we're just changing the window's active state and the focused node has no
    // renderer we can just ignore the state change.
    if (!renderer())
        return;

    if ((isElementNode() && toElement(this)->childrenAffectedByFocus())
        || (renderStyle()->affectedByFocus() && renderStyle()->hasPseudoStyle(FIRST_LETTER)))
        setNeedsStyleRecalc(SubtreeStyleChange);
    else if (renderStyle()->affectedByFocus())
        setNeedsStyleRecalc(LocalStyleChange);

    if (renderer() && renderer()->style()->hasAppearance())
        RenderTheme::theme().stateChanged(renderer(), FocusState);
}

void ContainerNode::setFocus(bool received)
{
    if (focused() == received)
        return;

    Node::setFocus(received);

    focusStateChanged();

    if (renderer() || received)
        return;

    // If :focus sets display: none, we lose focus but still need to recalc our style.
    if (isElementNode() && toElement(this)->childrenAffectedByFocus())
        setNeedsStyleRecalc(SubtreeStyleChange);
    else
        setNeedsStyleRecalc(LocalStyleChange);
}

void ContainerNode::setActive(bool down)
{
    if (down == active())
        return;

    Node::setActive(down);

    // FIXME: Why does this not need to handle the display: none transition like :hover does?
    if (renderer()) {
        if ((isElementNode() && toElement(this)->childrenAffectedByActive())
            || (renderStyle()->affectedByActive() && renderStyle()->hasPseudoStyle(FIRST_LETTER)))
            setNeedsStyleRecalc(SubtreeStyleChange);
        else if (renderStyle()->affectedByActive())
            setNeedsStyleRecalc(LocalStyleChange);

        if (renderStyle()->hasAppearance())
            RenderTheme::theme().stateChanged(renderer(), PressedState);
    }
}

void ContainerNode::setHovered(bool over)
{
    if (over == hovered())
        return;

    Node::setHovered(over);

    // If :hover sets display: none we lose our hover but still need to recalc our style.
    if (!renderer()) {
        if (over)
            return;
        if (isElementNode() && toElement(this)->childrenAffectedByHover())
            setNeedsStyleRecalc(SubtreeStyleChange);
        else
            setNeedsStyleRecalc(LocalStyleChange);
        return;
    }

    if ((isElementNode() && toElement(this)->childrenAffectedByHover())
        || (renderStyle()->affectedByHover() && renderStyle()->hasPseudoStyle(FIRST_LETTER)))
        setNeedsStyleRecalc(SubtreeStyleChange);
    else if (renderStyle()->affectedByHover())
        setNeedsStyleRecalc(LocalStyleChange);

    if (renderer()->style()->hasAppearance())
        RenderTheme::theme().stateChanged(renderer(), HoverState);
}

PassRefPtr<HTMLCollection> ContainerNode::children()
{
    return ensureRareData().ensureNodeLists().addCache<HTMLCollection>(*this, NodeChildren);
}

unsigned ContainerNode::countChildren() const
{
    unsigned count = 0;
    Node *n;
    for (n = firstChild(); n; n = n->nextSibling())
        count++;
    return count;
}

Node* ContainerNode::traverseToChildAt(unsigned index) const
{
    unsigned i;
    Node *n = firstChild();
    for (i = 0; n != 0 && i < index; i++)
        n = n->nextSibling();
    return n;
}

PassRefPtr<Element> ContainerNode::querySelector(const AtomicString& selectors, ExceptionState& exceptionState)
{
    if (selectors.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "The provided selector is empty.");
        return nullptr;
    }

    SelectorQuery* selectorQuery = document().selectorQueryCache().add(selectors, document(), exceptionState);
    if (!selectorQuery)
        return nullptr;
    return selectorQuery->queryFirst(*this);
}

PassRefPtr<NodeList> ContainerNode::querySelectorAll(const AtomicString& selectors, ExceptionState& exceptionState)
{
    if (selectors.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "The provided selector is empty.");
        return nullptr;
    }

    SelectorQuery* selectorQuery = document().selectorQueryCache().add(selectors, document(), exceptionState);
    if (!selectorQuery)
        return nullptr;
    return selectorQuery->queryAll(*this);
}

static void dispatchChildInsertionEvents(Node& child)
{
    if (child.isInShadowTree())
        return;

    if (!child.document().canDispatchEvents())
        return;

    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    RefPtr<Node> c(child);
    RefPtr<Document> document(child.document());

    if (c->parentNode() && document->hasListenerType(Document::DOMNODEINSERTED_LISTENER))
        c->dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMNodeInserted, true, c->parentNode()));

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEINSERTEDINTODOCUMENT_LISTENER)) {
        for (; c; c = NodeTraversal::next(*c, &child))
            c->dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMNodeInsertedIntoDocument, false));
    }
}

static void dispatchChildRemovalEvents(Node& child)
{
    InspectorInstrumentation::willRemoveDOMNode(&child);

    if (child.isInShadowTree())
        return;

    if (!child.document().canDispatchEvents())
        return;

    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    RefPtr<Node> c(child);
    RefPtr<Document> document(child.document());

    // dispatch pre-removal mutation events
    if (c->parentNode() && document->hasListenerType(Document::DOMNODEREMOVED_LISTENER)) {
        NodeChildRemovalTracker scope(child);
        c->dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMNodeRemoved, true, c->parentNode()));
    }

    // dispatch the DOMNodeRemovedFromDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEREMOVEDFROMDOCUMENT_LISTENER)) {
        NodeChildRemovalTracker scope(child);
        for (; c; c = NodeTraversal::next(*c, &child))
            c->dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMNodeRemovedFromDocument, false));
    }
}

void ContainerNode::updateTreeAfterInsertion(Node& child)
{
    ASSERT(refCount());
    ASSERT(child.refCount());

    ChildListMutationScope(*this).childAdded(child);

    childrenChanged(false, child.previousSibling(), child.nextSibling(), 1);

    ChildNodeInsertionNotifier(*this).notify(child);

    dispatchChildInsertionEvents(child);
}

bool ContainerNode::hasRestyleFlagInternal(DynamicRestyleFlags mask) const
{
    return rareData()->hasRestyleFlag(mask);
}

bool ContainerNode::hasRestyleFlagsInternal() const
{
    return rareData()->hasRestyleFlags();
}

void ContainerNode::setRestyleFlag(DynamicRestyleFlags mask)
{
    ASSERT(isElementNode() || isShadowRoot());
    ensureRareData().setRestyleFlag(mask);
}

void ContainerNode::checkForChildrenAdjacentRuleChanges()
{
    bool hasDirectAdjacentRules = childrenAffectedByDirectAdjacentRules();
    bool hasIndirectAdjacentRules = childrenAffectedByIndirectAdjacentRules();

    if (!hasDirectAdjacentRules && !hasIndirectAdjacentRules)
        return;

    unsigned forceCheckOfNextElementCount = 0;
    bool forceCheckOfAnyElementSibling = false;
    Document& document = this->document();

    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isElementNode())
            continue;
        Element* element = toElement(child);
        bool childRulesChanged = element->needsStyleRecalc() && element->styleChangeType() >= SubtreeStyleChange;

        if (forceCheckOfNextElementCount || forceCheckOfAnyElementSibling)
            element->setNeedsStyleRecalc(SubtreeStyleChange);

        if (forceCheckOfNextElementCount)
            forceCheckOfNextElementCount--;

        if (childRulesChanged && hasDirectAdjacentRules)
            forceCheckOfNextElementCount = document.styleEngine()->maxDirectAdjacentSelectors();

        forceCheckOfAnyElementSibling = forceCheckOfAnyElementSibling || (childRulesChanged && hasIndirectAdjacentRules);
    }
}

PassRefPtr<HTMLCollection> ContainerNode::getElementsByTagName(const AtomicString& localName)
{
    if (localName.isNull())
        return nullptr;

    if (document().isHTMLDocument())
        return ensureRareData().ensureNodeLists().addCache<HTMLTagCollection>(*this, HTMLTagCollectionType, localName);
    return ensureRareData().ensureNodeLists().addCache<TagCollection>(*this, TagCollectionType, localName);
}

PassRefPtr<HTMLCollection> ContainerNode::getElementsByTagNameNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    if (localName.isNull())
        return nullptr;

    if (namespaceURI == starAtom)
        return getElementsByTagName(localName);

    return ensureRareData().ensureNodeLists().addCache(*this, namespaceURI.isEmpty() ? nullAtom : namespaceURI, localName);
}

// Takes an AtomicString in argument because it is common for elements to share the same name attribute.
// Therefore, the NameNodeList factory function expects an AtomicString type.
PassRefPtr<NodeList> ContainerNode::getElementsByName(const AtomicString& elementName)
{
    return ensureRareData().ensureNodeLists().addCache<NameNodeList>(*this, NameNodeListType, elementName);
}

// Takes an AtomicString in argument because it is common for elements to share the same set of class names.
// Therefore, the ClassNodeList factory function expects an AtomicString type.
PassRefPtr<HTMLCollection> ContainerNode::getElementsByClassName(const AtomicString& classNames)
{
    return ensureRareData().ensureNodeLists().addCache<ClassCollection>(*this, ClassCollectionType, classNames);
}

PassRefPtr<RadioNodeList> ContainerNode::radioNodeList(const AtomicString& name, bool onlyMatchImgElements)
{
    ASSERT(isHTMLFormElement(this) || isHTMLFieldSetElement(this));
    CollectionType type = onlyMatchImgElements ? RadioImgNodeListType : RadioNodeListType;
    return ensureRareData().ensureNodeLists().addCache<RadioNodeList>(*this, type, name);
}

#ifndef NDEBUG
bool childAttachedAllowedWhenAttachingChildren(ContainerNode* node)
{
    if (node->isShadowRoot())
        return true;

    if (node->isInsertionPoint())
        return true;

    if (node->isElementNode() && toElement(node)->shadow())
        return true;

    return false;
}
#endif

} // namespace WebCore
