/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/dom/ContainerNodeAlgorithms.h"

#include "core/dom/Element.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"

namespace WebCore {

class ShadowRootVector : public WillBeHeapVector<RefPtrWillBeMember<ShadowRoot> > {
public:
    explicit ShadowRootVector(ElementShadow* tree)
    {
        for (ShadowRoot* root = tree->youngestShadowRoot(); root; root = root->olderShadowRoot())
            append(root);
    }
};

void ChildNodeInsertionNotifier::notifyDescendantInsertedIntoDocument(ContainerNode& node)
{
    ChildNodesLazySnapshot snapshot(node);
    while (RefPtr<Node> child = snapshot.nextNode()) {
        // If we have been removed from the document during this loop, then
        // we don't want to tell the rest of our children that they've been
        // inserted into the document because they haven't.
        if (node.inDocument() && child->parentNode() == node)
            notifyNodeInsertedIntoDocument(*child);
    }

    if (!node.isElementNode())
        return;

    if (ElementShadow* shadow = toElement(node).shadow()) {
        ShadowRootVector roots(shadow);
        for (size_t i = 0; i < roots.size(); ++i) {
            if (node.inDocument() && roots[i]->host() == node)
                notifyNodeInsertedIntoDocument(*roots[i]);
        }
    }
}

void ChildNodeInsertionNotifier::notifyNodeInsertedIntoTree(ContainerNode& root)
{
    ASSERT(!m_insertionPoint.inDocument());

    for (Node* node = &root; node; node = NodeTraversal::next(*node, &root)) {
        // As an optimization we don't notify leaf nodes when when inserting
        // into detached subtrees.
        if (!node->isContainerNode())
            continue;
        if (Node::InsertionShouldCallDidNotifySubtreeInsertions == node->insertedInto(&m_insertionPoint))
            m_postInsertionNotificationTargets.append(node);
        for (ShadowRoot* shadowRoot = node->youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot())
            notifyNodeInsertedIntoTree(*shadowRoot);
    }
}

void ChildNodeRemovalNotifier::notifyDescendantRemovedFromDocument(ContainerNode& node)
{
    ChildNodesLazySnapshot snapshot(node);
    while (RefPtr<Node> child = snapshot.nextNode()) {
        // If we have been added to the document during this loop, then we
        // don't want to tell the rest of our children that they've been
        // removed from the document because they haven't.
        if (!node.inDocument() && child->parentNode() == node)
            notifyNodeRemovedFromDocument(*child);
    }

    if (!node.isElementNode())
        return;

    if (node.document().cssTarget() == node)
        node.document().setCSSTarget(0);

    if (ElementShadow* shadow = toElement(node).shadow()) {
        ShadowRootVector roots(shadow);
        for (size_t i = 0; i < roots.size(); ++i) {
            if (!node.inDocument() && roots[i]->host() == node)
                notifyNodeRemovedFromDocument(*roots[i]);
        }
    }
}

void ChildNodeRemovalNotifier::notifyDescendantRemovedFromTree(ContainerNode& node)
{
    for (Node* child = node.firstChild(); child; child = child->nextSibling()) {
        if (child->isContainerNode())
            notifyNodeRemovedFromTree(toContainerNode(*child));
    }

    if (!node.isElementNode())
        return;

    if (ElementShadow* shadow = toElement(node).shadow()) {
        ShadowRootVector roots(shadow);
        for (size_t i = 0; i < roots.size(); ++i)
            notifyNodeRemovedFromTree(*roots[i]);
    }
}

void ChildFrameDisconnector::collectFrameOwners(ElementShadow& shadow)
{
    for (ShadowRoot* root = shadow.youngestShadowRoot(); root; root = root->olderShadowRoot())
        collectFrameOwners(*root);
}

#ifndef NDEBUG
unsigned assertConnectedSubrameCountIsConsistent(Node& node)
{
    unsigned count = 0;

    if (node.isElementNode()) {
        if (node.isFrameOwnerElement() && toHTMLFrameOwnerElement(node).contentFrame())
            count++;

        if (ElementShadow* shadow = toElement(node).shadow()) {
            for (ShadowRoot* root = shadow->youngestShadowRoot(); root; root = root->olderShadowRoot())
                count += assertConnectedSubrameCountIsConsistent(*root);
        }
    }

    for (Node* child = node.firstChild(); child; child = child->nextSibling())
        count += assertConnectedSubrameCountIsConsistent(*child);

    // If we undercount there's possibly a security bug since we'd leave frames
    // in subtrees outside the document.
    ASSERT(node.connectedSubframeCount() >= count);

    // If we overcount it's safe, but not optimal because it means we'll traverse
    // through the document in ChildFrameDisconnector looking for frames that have
    // already been disconnected.
    ASSERT(node.connectedSubframeCount() == count);

    return count;
}
#endif

}
