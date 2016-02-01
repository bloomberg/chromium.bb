/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/HTMLSlotElement.h"

#include "core/HTMLNames.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/html/AssignedNodesOptions.h"

namespace blink {

using namespace HTMLNames;

inline HTMLSlotElement::HTMLSlotElement(Document& document)
    : HTMLElement(slotTag, document)
{
}

DEFINE_NODE_FACTORY(HTMLSlotElement);

const WillBeHeapVector<RefPtrWillBeMember<Node>> HTMLSlotElement::getAssignedNodesForBinding(const AssignedNodesOptions& options)
{
    updateDistribution();
    if (options.hasFlatten() && options.flatten())
        return getDistributedNodes();
    return m_assignedNodes;
}

const WillBeHeapVector<RefPtrWillBeMember<Node>>& HTMLSlotElement::getDistributedNodes()
{
    ASSERT(!needsDistributionRecalc());
    if (isInShadowTree())
        return m_distributedNodes;

    // A slot is unlikely to be used outside of a shadow tree.
    // We do not need to optimize this case in most cases.
    // TODO(hayato): If this path causes a performance issue, we should move
    // ShadowRootRaraDate::m_descendantSlots into TreeScopreRareData-ish and
    // update the distribution code so it considers a document tree too.
    clearDistribution();
    for (Node& child : NodeTraversal::childrenOf(*this)) {
        if (isHTMLSlotElement(child))
            m_distributedNodes.appendVector(toHTMLSlotElement(child).getDistributedNodes());
        else
            m_distributedNodes.append(&child);
    }
    return m_distributedNodes;
}

void HTMLSlotElement::appendAssignedNode(Node& node)
{
    m_assignedNodes.append(&node);
}

void HTMLSlotElement::appendDistributedNode(Node& node)
{
    size_t size = m_distributedNodes.size();
    m_distributedNodes.append(&node);
    m_distributedIndices.set(&node, size);
}

void HTMLSlotElement::appendDistributedNodesFrom(const HTMLSlotElement& other)
{
    size_t index = m_distributedNodes.size();
    m_distributedNodes.appendVector(other.m_distributedNodes);
    for (const auto& it : other.m_distributedIndices) {
        const Node* node = it.key;
        m_distributedIndices.set(node, index++);
    }
}

void HTMLSlotElement::clearDistribution()
{
    m_assignedNodes.clear();
    m_distributedNodes.clear();
    m_distributedIndices.clear();
}

Node* HTMLSlotElement::distributedNodeNextTo(const Node& node) const
{
    const auto& it = m_distributedIndices.find(&node);
    if (it == m_distributedIndices.end())
        return nullptr;
    size_t index = it->value;
    if (index + 1 == m_distributedNodes.size())
        return nullptr;
    return m_distributedNodes[index + 1].get();
}

Node* HTMLSlotElement::distributedNodePreviousTo(const Node& node) const
{
    const auto& it = m_distributedIndices.find(&node);
    if (it == m_distributedIndices.end())
        return nullptr;
    size_t index = it->value;
    if (index == 0)
        return nullptr;
    return m_distributedNodes[index - 1].get();
}

void HTMLSlotElement::attach(const AttachContext& context)
{
    for (auto& node : m_distributedNodes) {
        if (node->needsAttach())
            node->attach(context);
    }

    HTMLElement::attach(context);
}

void HTMLSlotElement::detach(const AttachContext& context)
{
    for (auto& node : m_distributedNodes)
        node->lazyReattachIfAttached();

    HTMLElement::detach(context);
}

void HTMLSlotElement::attributeChanged(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason reason)
{
    if (name == nameAttr) {
        if (ShadowRoot* root = containingShadowRoot())
            root->owner()->willAffectSelector();
    }
    HTMLElement::attributeChanged(name, oldValue, newValue, reason);
}

void HTMLSlotElement::childrenChanged(const ChildrenChange& change)
{
    HTMLElement::childrenChanged(change);
    if (ShadowRoot* root = containingShadowRoot()) {
        if (ElementShadow* rootOwner = root->owner())
            rootOwner->setNeedsDistributionRecalc();
    }
}

Node::InsertionNotificationRequest HTMLSlotElement::insertedInto(ContainerNode* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    if (ShadowRoot* root = containingShadowRoot()) {
        if (ElementShadow* rootOwner = root->owner())
            rootOwner->setNeedsDistributionRecalc();
        if (root == insertionPoint->treeScope().rootNode())
            root->didAddSlot();
    }

    // We could have been distributed into in a detached subtree, make sure to
    // clear the distribution when inserted again to avoid cycles.
    clearDistribution();

    return InsertionDone;
}

void HTMLSlotElement::removedFrom(ContainerNode* insertionPoint)
{
    ShadowRoot* root = containingShadowRoot();
    if (!root)
        root = insertionPoint->containingShadowRoot();
    if (root) {
        if (ElementShadow* rootOwner = root->owner())
            rootOwner->setNeedsDistributionRecalc();
    }

    // Since this insertion point is no longer visible from the shadow subtree, it need to clean itself up.
    clearDistribution();

    if (root == insertionPoint->treeScope().rootNode())
        root->didRemoveSlot();

    HTMLElement::removedFrom(insertionPoint);
}

void HTMLSlotElement::updateDistributedNodesWithFallback()
{
    if (!m_distributedNodes.isEmpty())
        return;
    for (auto& child : NodeTraversal::childrenOf(*this)) {
        // Insertion points are not supported as slots fallback
        if (isActiveInsertionPoint(child))
            continue;
        if (isHTMLSlotElement(child))
            appendDistributedNodesFrom(toHTMLSlotElement(child));
        else
            appendDistributedNode(child);
    }
}

DEFINE_TRACE(HTMLSlotElement)
{
#if ENABLE(OILPAN)
    visitor->trace(m_assignedNodes);
    visitor->trace(m_distributedNodes);
    visitor->trace(m_distributedIndices);
#endif
    HTMLElement::trace(visitor);
}

} // namespace blink
