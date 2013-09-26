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

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ChildListMutationScope.h"
#include "core/dom/ContainerNodeAlgorithms.h"
#include "core/dom/ElementTraversal.h"
#include "core/events/EventNames.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/events/MutationEvent.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/NodeTraversal.h"
#include "core/html/HTMLCollection.h"
#include "core/rendering/InlineTextBox.h"
#include "core/rendering/RenderText.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/RenderWidget.h"
#include "wtf/Vector.h"

using namespace std;

namespace WebCore {

static void dispatchChildInsertionEvents(Node*);
static void dispatchChildRemovalEvents(Node*);
static void updateTreeAfterInsertion(ContainerNode*, Node*);

ChildNodesLazySnapshot* ChildNodesLazySnapshot::latestSnapshot = 0;

#ifndef NDEBUG
unsigned NoEventDispatchAssertion::s_count = 0;
#endif

static void collectChildrenAndRemoveFromOldParent(Node* node, NodeVector& nodes, ExceptionState& es)
{
    if (node->nodeType() != Node::DOCUMENT_FRAGMENT_NODE) {
        nodes.append(node);
        if (ContainerNode* oldParent = node->parentNode())
            oldParent->removeChild(node, es);
        return;
    }
    getChildNodes(node, nodes);
    toContainerNode(node)->removeChildren();
}

void ContainerNode::removeDetachedChildren()
{
    if (connectedSubframeCount()) {
        for (Node* child = firstChild(); child; child = child->nextSibling())
            child->updateAncestorConnectedSubframeCountForRemoval();
    }
    // FIXME: We should be able to ASSERT(!confusingAndOftenMisusedAttached()) here: https://bugs.webkit.org/show_bug.cgi?id=107801
    removeDetachedChildrenInContainer<Node, ContainerNode>(this);
}

void ContainerNode::takeAllChildrenFrom(ContainerNode* oldParent)
{
    NodeVector children;
    getChildNodes(oldParent, children);

    if (oldParent->document().hasMutationObserversOfType(MutationObserver::ChildList)) {
        ChildListMutationScope mutation(oldParent);
        for (unsigned i = 0; i < children.size(); ++i)
            mutation.willRemoveChild(children[i].get());
    }

    // FIXME: We need to do notifyMutationObserversNodeWillDetach() for each child,
    // probably inside removeDetachedChildrenInContainer.

    oldParent->removeDetachedChildren();

    for (unsigned i = 0; i < children.size(); ++i) {
        if (children[i]->confusingAndOftenMisusedAttached())
            children[i]->detach();
        // FIXME: We need a no mutation event version of adoptNode.
        RefPtr<Node> child = document().adoptNode(children[i].release(), ASSERT_NO_EXCEPTION);
        // FIXME: Together with adoptNode above, the tree scope might get updated recursively twice
        // (if the document changed or oldParent was in a shadow tree, AND *this is in a shadow tree).
        // Can we do better?
        treeScope().adoptIfNeeded(child.get());
        parserAppendChild(child.get());
    }
}

ContainerNode::~ContainerNode()
{
    if (Document* document = documentInternal())
        willBeDeletedFrom(document);
    removeDetachedChildren();
}

static inline bool isChildTypeAllowed(ContainerNode* newParent, Node* child)
{
    if (!child->isDocumentFragment())
        return newParent->childTypeAllowed(child->nodeType());

    for (Node* node = child->firstChild(); node; node = node->nextSibling()) {
        if (!newParent->childTypeAllowed(node->nodeType()))
            return false;
    }
    return true;
}

static inline bool isInTemplateContent(const Node* node)
{
    Document& document = node->document();
    return &document == document.templateDocument();
}

static inline bool containsConsideringHostElements(const Node* newChild, const Node* newParent)
{
    return (newParent->isInShadowTree() || isInTemplateContent(newParent))
        ? newChild->containsIncludingHostElements(newParent)
        : newChild->contains(newParent);
}

static inline bool checkAcceptChild(ContainerNode* newParent, Node* newChild, Node* oldChild, const String& method, ExceptionState& es)
{
    // Not mentioned in spec: throw NotFoundError if newChild is null
    if (!newChild) {
        es.throwDOMException(NotFoundError, ExceptionMessages::failedToExecute(method, "ContainerNode", "The new child element is null."));
        return false;
    }

    // Use common case fast path if possible.
    if ((newChild->isElementNode() || newChild->isTextNode()) && newParent->isElementNode()) {
        ASSERT(!newParent->isDocumentTypeNode());
        ASSERT(isChildTypeAllowed(newParent, newChild));
        if (containsConsideringHostElements(newChild, newParent)) {
            es.throwDOMException(HierarchyRequestError, ExceptionMessages::failedToExecute(method, "ContainerNode", "The new child element contains the parent."));
            return false;
        }
        return true;
    }

    // This should never happen, but also protect release builds from tree corruption.
    ASSERT(!newChild->isPseudoElement());
    if (newChild->isPseudoElement()) {
        es.throwDOMException(HierarchyRequestError, ExceptionMessages::failedToExecute(method, "ContainerNode", "The new child element is a pseudo-element."));
        return false;
    }

    if (containsConsideringHostElements(newChild, newParent)) {
        es.throwDOMException(HierarchyRequestError, ExceptionMessages::failedToExecute(method, "ContainerNode", "The new child element contains the parent."));
        return false;
    }

    if (oldChild && newParent->isDocumentNode()) {
        if (!toDocument(newParent)->canReplaceChild(newChild, oldChild)) {
            // FIXME: Adjust 'Document::canReplaceChild' to return some additional detail (or an error message).
            es.throwDOMException(HierarchyRequestError, ExceptionMessages::failedToExecute(method, "ContainerNode"));
            return false;
        }
    } else if (!isChildTypeAllowed(newParent, newChild)) {
        es.throwDOMException(HierarchyRequestError, ExceptionMessages::failedToExecute(method, "ContainerNode", "Nodes of type '" + newChild->nodeName() + "' may not be inserted inside nodes of type '" + newParent->nodeName() + "'."));
        return false;
    }

    return true;
}

static inline bool checkAcceptChildGuaranteedNodeTypes(ContainerNode* newParent, Node* newChild, const String& method, ExceptionState& es)
{
    ASSERT(!newParent->isDocumentTypeNode());
    ASSERT(isChildTypeAllowed(newParent, newChild));
    if (newChild->contains(newParent)) {
        es.throwDOMException(HierarchyRequestError, ExceptionMessages::failedToExecute(method, "ContainerNode", "The new child element contains the parent."));
        return false;
    }

    return true;
}

static inline bool checkAddChild(ContainerNode* newParent, Node* newChild, const String& method, ExceptionState& es)
{
    return checkAcceptChild(newParent, newChild, 0, method, es);
}

static inline bool checkReplaceChild(ContainerNode* newParent, Node* newChild, Node* oldChild, const String& method, ExceptionState& es)
{
    return checkAcceptChild(newParent, newChild, oldChild, method, es);
}

void ContainerNode::insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionState& es)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    RefPtr<Node> protect(this);

    // insertBefore(node, 0) is equivalent to appendChild(node)
    if (!refChild) {
        appendChild(newChild, es);
        return;
    }

    // Make sure adding the new child is OK.
    if (!checkAddChild(this, newChild.get(), "insertBefore", es))
        return;

    // NotFoundError: Raised if refChild is not a child of this node
    if (refChild->parentNode() != this) {
        es.throwDOMException(NotFoundError, ExceptionMessages::failedToExecute("insertBefore", "ContainerNode", "The node before which the new node is to be inserted is not a child of this node."));
        return;
    }

    if (refChild->previousSibling() == newChild || refChild == newChild) // nothing to do
        return;

    RefPtr<Node> next = refChild;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(newChild.get(), targets, es);
    if (es.hadException())
        return;
    if (targets.isEmpty())
        return;

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    if (!checkAcceptChildGuaranteedNodeTypes(this, newChild.get(), "insertBefore", es))
        return;

    InspectorInstrumentation::willInsertDOMNode(&document(), this);

    ChildListMutationScope mutation(this);
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        Node* child = it->get();

        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "next" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (next->parentNode() != this)
            break;
        if (child->parentNode())
            break;

        treeScope().adoptIfNeeded(child);

        insertBeforeCommon(next.get(), child);

        updateTreeAfterInsertion(this, child);
    }

    dispatchSubtreeModifiedEvent();
}

void ContainerNode::insertBeforeCommon(Node* nextChild, Node* newChild)
{
    NoEventDispatchAssertion assertNoEventDispatch;

    ASSERT(newChild);
    ASSERT(!newChild->parentNode()); // Use insertBefore if you need to handle reparenting (and want DOM mutation events).
    ASSERT(!newChild->nextSibling());
    ASSERT(!newChild->previousSibling());
    ASSERT(!newChild->isShadowRoot());

    Node* prev = nextChild->previousSibling();
    ASSERT(m_lastChild != prev);
    nextChild->setPreviousSibling(newChild);
    if (prev) {
        ASSERT(m_firstChild != nextChild);
        ASSERT(prev->nextSibling() == nextChild);
        prev->setNextSibling(newChild);
    } else {
        ASSERT(m_firstChild == nextChild);
        m_firstChild = newChild;
    }
    newChild->setParentOrShadowHostNode(this);
    newChild->setPreviousSibling(prev);
    newChild->setNextSibling(nextChild);
}

void ContainerNode::parserInsertBefore(PassRefPtr<Node> newChild, Node* nextChild)
{
    ASSERT(newChild);
    ASSERT(nextChild);
    ASSERT(nextChild->parentNode() == this);
    ASSERT(!newChild->isDocumentFragment());
    ASSERT(!hasTagName(HTMLNames::templateTag));

    if (nextChild->previousSibling() == newChild || nextChild == newChild) // nothing to do
        return;

    if (&document() != &newChild->document())
        document().adoptNode(newChild.get(), ASSERT_NO_EXCEPTION);

    insertBeforeCommon(nextChild, newChild.get());

    newChild->updateAncestorConnectedSubframeCountForInsertion();

    ChildListMutationScope(this).childAdded(newChild.get());

    childrenChanged(true, newChild->previousSibling(), nextChild, 1);

    ChildNodeInsertionNotifier(this).notify(newChild.get());
}

void ContainerNode::replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionState& es)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    RefPtr<Node> protect(this);

    if (oldChild == newChild) // nothing to do
        return;

    if (!oldChild) {
        es.throwDOMException(NotFoundError, ExceptionMessages::failedToExecute("replaceChild", "ContainerNode", "The node to be replaced is null."));
        return;
    }

    // Make sure replacing the old child with the new is ok
    if (!checkReplaceChild(this, newChild.get(), oldChild, "replaceChild", es))
        return;

    // NotFoundError: Raised if oldChild is not a child of this node.
    if (oldChild->parentNode() != this) {
        es.throwDOMException(NotFoundError, ExceptionMessages::failedToExecute("replaceChild", "ContainerNode", "The node to be replaced is not a child of this node."));
        return;
    }

    ChildListMutationScope mutation(this);

    RefPtr<Node> next = oldChild->nextSibling();

    // Remove the node we're replacing
    RefPtr<Node> removedChild = oldChild;
    removeChild(oldChild, es);
    if (es.hadException())
        return;

    if (next && (next->previousSibling() == newChild || next == newChild)) // nothing to do
        return;

    // Does this one more time because removeChild() fires a MutationEvent.
    if (!checkReplaceChild(this, newChild.get(), oldChild, "replaceChild", es))
        return;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(newChild.get(), targets, es);
    if (es.hadException())
        return;

    // Does this yet another check because collectChildrenAndRemoveFromOldParent() fires a MutationEvent.
    if (!checkReplaceChild(this, newChild.get(), oldChild, "replaceChild", es))
        return;

    InspectorInstrumentation::willInsertDOMNode(&document(), this);

    // Add the new child(ren)
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        Node* child = it->get();

        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "next" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (next && next->parentNode() != this)
            break;
        if (child->parentNode())
            break;

        treeScope().adoptIfNeeded(child);

        // Add child before "next".
        {
            NoEventDispatchAssertion assertNoEventDispatch;
            if (next)
                insertBeforeCommon(next.get(), child);
            else
                appendChildToContainer(child, this);
        }

        updateTreeAfterInsertion(this, child);
    }

    dispatchSubtreeModifiedEvent();
}

static void willRemoveChild(Node* child)
{
    ASSERT(child->parentNode());
    ChildListMutationScope(child->parentNode()).willRemoveChild(child);
    child->notifyMutationObserversNodeWillDetach();
    dispatchChildRemovalEvents(child);
    child->document().nodeWillBeRemoved(child); // e.g. mutation event listener can create a new range.
    ChildFrameDisconnector(child).disconnect();
}

static void willRemoveChildren(ContainerNode* container)
{
    NodeVector children;
    getChildNodes(container, children);

    container->document().nodeChildrenWillBeRemoved(container);

    ChildListMutationScope mutation(container);
    for (NodeVector::const_iterator it = children.begin(); it != children.end(); it++) {
        Node* child = it->get();
        mutation.willRemoveChild(child);
        child->notifyMutationObserversNodeWillDetach();

        // fire removed from document mutation events.
        dispatchChildRemovalEvents(child);
    }

    ChildFrameDisconnector(container).disconnect(ChildFrameDisconnector::DescendantsOnly);
}

void ContainerNode::disconnectDescendantFrames()
{
    ChildFrameDisconnector(this).disconnect();
}

void ContainerNode::removeChild(Node* oldChild, ExceptionState& es)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    RefPtr<Node> protect(this);

    // NotFoundError: Raised if oldChild is not a child of this node.
    if (!oldChild || oldChild->parentNode() != this) {
        es.throwDOMException(NotFoundError, ExceptionMessages::failedToExecute("removeChild", "ContainerNode", "The node to be removed is not a child of this node."));
        return;
    }

    RefPtr<Node> child = oldChild;

    document().removeFocusedElementOfSubtree(child.get());

    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(&document()))
        fullscreen->removeFullScreenElementOfSubtree(child.get());

    // Events fired when blurring currently focused node might have moved this
    // child into a different parent.
    if (child->parentNode() != this) {
        es.throwDOMException(NotFoundError, ExceptionMessages::failedToExecute("removeChild", "ContainerNode", "The node to be removed is no longer a child of this node. Perhaps it was moved in a 'blur' event handler?"));
        return;
    }

    willRemoveChild(child.get());

    // Mutation events might have moved this child into a different parent.
    if (child->parentNode() != this) {
        es.throwDOMException(NotFoundError, ExceptionMessages::failedToExecute("removeChild", "ContainerNode", "The node to be removed is no longer a child of this node. Perhaps it was moved in response to a mutation?"));
        return;
    }

    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

        Node* prev = child->previousSibling();
        Node* next = child->nextSibling();
        removeBetween(prev, next, child.get());
        childrenChanged(false, prev, next, -1);
        ChildNodeRemovalNotifier(this).notify(child.get());
    }
    dispatchSubtreeModifiedEvent();
}

void ContainerNode::removeBetween(Node* previousChild, Node* nextChild, Node* oldChild)
{
    NoEventDispatchAssertion assertNoEventDispatch;

    ASSERT(oldChild);
    ASSERT(oldChild->parentNode() == this);

    // Remove from rendering tree
    if (oldChild->confusingAndOftenMisusedAttached())
        oldChild->detach();

    if (nextChild)
        nextChild->setPreviousSibling(previousChild);
    if (previousChild)
        previousChild->setNextSibling(nextChild);
    if (m_firstChild == oldChild)
        m_firstChild = nextChild;
    if (m_lastChild == oldChild)
        m_lastChild = previousChild;

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParentOrShadowHostNode(0);

    document().adoptIfNeeded(oldChild);
}

void ContainerNode::parserRemoveChild(Node* oldChild)
{
    ASSERT(oldChild);
    ASSERT(oldChild->parentNode() == this);
    ASSERT(!oldChild->isDocumentFragment());

    Node* prev = oldChild->previousSibling();
    Node* next = oldChild->nextSibling();

    oldChild->updateAncestorConnectedSubframeCountForRemoval();

    ChildListMutationScope(this).willRemoveChild(oldChild);
    oldChild->notifyMutationObserversNodeWillDetach();

    removeBetween(prev, next, oldChild);

    childrenChanged(true, prev, next, -1);
    ChildNodeRemovalNotifier(this).notify(oldChild);
}

// this differs from other remove functions because it forcibly removes all the children,
// regardless of read-only status or event exceptions, e.g.
void ContainerNode::removeChildren()
{
    if (!m_firstChild)
        return;

    // The container node can be removed from event handlers.
    RefPtr<ContainerNode> protect(this);

    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(&document()))
        fullscreen->removeFullScreenElementOfSubtree(this, true);

    // Do any prep work needed before actually starting to detach
    // and remove... e.g. stop loading frames, fire unload events.
    willRemoveChildren(protect.get());

    {
        // Removing focus can cause frames to load, either via events (focusout, blur)
        // or widget updates (e.g., for <embed>).
        SubframeLoadingDisabler disabler(this);

        // Exclude this node when looking for removed focusedElement since only
        // children will be removed.
        // This must be later than willRemoveChildren, which might change focus
        // state of a child.
        document().removeFocusedElementOfSubtree(this, true);
    }

    NodeVector removedChildren;
    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        {
            NoEventDispatchAssertion assertNoEventDispatch;
            removedChildren.reserveInitialCapacity(childNodeCount());
            while (m_firstChild) {
                removedChildren.append(m_firstChild);
                removeBetween(0, m_firstChild->nextSibling(), m_firstChild);
            }
        }

        childrenChanged(false, 0, 0, -static_cast<int>(removedChildren.size()));

        for (size_t i = 0; i < removedChildren.size(); ++i)
            ChildNodeRemovalNotifier(this).notify(removedChildren[i].get());
    }

    dispatchSubtreeModifiedEvent();
}

void ContainerNode::appendChild(PassRefPtr<Node> newChild, ExceptionState& es)
{
    RefPtr<ContainerNode> protect(this);

    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    // Make sure adding the new child is ok
    if (!checkAddChild(this, newChild.get(), "appendChild", es))
        return;

    if (newChild == m_lastChild) // nothing to do
        return;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(newChild.get(), targets, es);
    if (es.hadException())
        return;

    if (targets.isEmpty())
        return;

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    if (!checkAcceptChildGuaranteedNodeTypes(this, newChild.get(), "appendChild", es))
        return;

    InspectorInstrumentation::willInsertDOMNode(&document(), this);

    // Now actually add the child(ren)
    ChildListMutationScope mutation(this);
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        Node* child = it->get();

        // If the child has a parent again, just stop what we're doing, because
        // that means someone is doing something with DOM mutation -- can't re-parent
        // a child that already has a parent.
        if (child->parentNode())
            break;

        treeScope().adoptIfNeeded(child);

        // Append child to the end of the list
        {
            NoEventDispatchAssertion assertNoEventDispatch;
            appendChildToContainer(child, this);
        }

        updateTreeAfterInsertion(this, child);
    }

    dispatchSubtreeModifiedEvent();
}

void ContainerNode::parserAppendChild(PassRefPtr<Node> newChild)
{
    ASSERT(newChild);
    ASSERT(!newChild->parentNode()); // Use appendChild if you need to handle reparenting (and want DOM mutation events).
    ASSERT(!newChild->isDocumentFragment());
    ASSERT(!hasTagName(HTMLNames::templateTag));

    if (&document() != &newChild->document())
        document().adoptNode(newChild.get(), ASSERT_NO_EXCEPTION);

    Node* last = m_lastChild;
    {
        NoEventDispatchAssertion assertNoEventDispatch;
        // FIXME: This method should take a PassRefPtr.
        appendChildToContainer(newChild.get(), this);
        treeScope().adoptIfNeeded(newChild.get());
    }

    newChild->updateAncestorConnectedSubframeCountForInsertion();

    ChildListMutationScope(this).childAdded(newChild.get());

    childrenChanged(true, last, 0, 1);
    ChildNodeInsertionNotifier(this).notify(newChild.get());
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
}

void ContainerNode::cloneChildNodes(ContainerNode *clone)
{
    TrackExceptionState es;
    for (Node* n = firstChild(); n && !es.hadException(); n = n->nextSibling())
        clone->appendChild(n->cloneNode(true), es);
}


bool ContainerNode::getUpperLeftCorner(FloatPoint& point) const
{
    if (!renderer())
        return false;
    // What is this code really trying to do?
    RenderObject* o = renderer();
    RenderObject* p = o;

    if (!o->isInline() || o->isReplaced()) {
        point = o->localToAbsolute(FloatPoint(), UseTransforms);
        return true;
    }

    // find the next text/image child, to get a position
    while (o) {
        p = o;
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
                point.move(toRenderText(o)->linesBoundingBox().x(), toRenderText(o)->firstTextBox()->root()->lineTop());
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

void ContainerNode::setFocus(bool received)
{
    if (focused() == received)
        return;

    Node::setFocus(received);

    // note that we need to recalc the style
    setNeedsStyleRecalc();
}

void ContainerNode::setActive(bool down, bool pause)
{
    if (down == active()) return;

    Node::setActive(down);

    // note that we need to recalc the style
    // FIXME: Move to Element
    if (renderer()) {
        if (renderStyle()->affectedByActive() || (isElementNode() && toElement(this)->childrenAffectedByActive()))
            setNeedsStyleRecalc();
        if (renderStyle()->hasAppearance())
            RenderTheme::theme().stateChanged(renderer(), PressedState);
    }
}

void ContainerNode::setHovered(bool over)
{
    if (over == hovered()) return;

    Node::setHovered(over);

    if (!renderer()) {
        // When setting hover to false, the style needs to be recalc'd even when
        // there's no renderer (imagine setting display:none in the :hover class,
        // if a nil renderer would prevent this element from recalculating its
        // style, it would never go back to its normal style and remain
        // stuck in its hovered style).
        if (!over)
            setNeedsStyleRecalc();

        return;
    }

    // note that we need to recalc the style
    // FIXME: Move to Element
    if (renderer()) {
        if (renderStyle()->affectedByHover() || (isElementNode() && toElement(this)->childrenAffectedByHover()))
            setNeedsStyleRecalc();
        if (renderer() && renderer()->style()->hasAppearance())
            RenderTheme::theme().stateChanged(renderer(), HoverState);
    }
}

PassRefPtr<HTMLCollection> ContainerNode::children()
{
    return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<HTMLCollection>(this, NodeChildren);
}

Element* ContainerNode::firstElementChild() const
{
    return ElementTraversal::firstWithin(this);
}

Element* ContainerNode::lastElementChild() const
{
    Node* n = lastChild();
    while (n && !n->isElementNode())
        n = n->previousSibling();
    return toElement(n);
}

unsigned ContainerNode::childElementCount() const
{
    unsigned count = 0;
    Node* n = firstChild();
    while (n) {
        count += n->isElementNode();
        n = n->nextSibling();
    }
    return count;
}

unsigned ContainerNode::childNodeCount() const
{
    unsigned count = 0;
    Node *n;
    for (n = firstChild(); n; n = n->nextSibling())
        count++;
    return count;
}

Node *ContainerNode::childNode(unsigned index) const
{
    unsigned i;
    Node *n = firstChild();
    for (i = 0; n != 0 && i < index; i++)
        n = n->nextSibling();
    return n;
}

static void dispatchChildInsertionEvents(Node* child)
{
    if (child->isInShadowTree())
        return;

    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    RefPtr<Node> c = child;
    RefPtr<Document> document = &child->document();

    if (c->parentNode() && document->hasListenerType(Document::DOMNODEINSERTED_LISTENER))
        c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeInsertedEvent, true, c->parentNode()));

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEINSERTEDINTODOCUMENT_LISTENER)) {
        for (; c; c = NodeTraversal::next(c.get(), child))
            c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeInsertedIntoDocumentEvent, false));
    }
}

static void dispatchChildRemovalEvents(Node* child)
{
    if (child->isInShadowTree()) {
        InspectorInstrumentation::willRemoveDOMNode(&child->document(), child);
        return;
    }

    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    InspectorInstrumentation::willRemoveDOMNode(&child->document(), child);

    RefPtr<Node> c = child;
    RefPtr<Document> document = &child->document();

    // dispatch pre-removal mutation events
    if (c->parentNode() && document->hasListenerType(Document::DOMNODEREMOVED_LISTENER))
        c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeRemovedEvent, true, c->parentNode()));

    // dispatch the DOMNodeRemovedFromDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEREMOVEDFROMDOCUMENT_LISTENER)) {
        for (; c; c = NodeTraversal::next(c.get(), child))
            c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeRemovedFromDocumentEvent, false));
    }
}

static void updateTreeAfterInsertion(ContainerNode* parent, Node* child)
{
    ASSERT(parent->refCount());
    ASSERT(child->refCount());

    ChildListMutationScope(parent).childAdded(child);

    parent->childrenChanged(false, child->previousSibling(), child->nextSibling(), 1);

    ChildNodeInsertionNotifier(parent).notify(child);

    dispatchChildInsertionEvents(child);
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
