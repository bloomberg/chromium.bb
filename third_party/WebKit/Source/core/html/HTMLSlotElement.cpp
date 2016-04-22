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

#include "bindings/core/v8/Microtask.h"
#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/events/Event.h"
#include "core/html/AssignedNodesOptions.h"

namespace blink {

using namespace HTMLNames;

inline HTMLSlotElement::HTMLSlotElement(Document& document)
    : HTMLElement(slotTag, document)
    , m_distributionState(DistributionDone)
    , m_assignmentState(AssignmentDone)
    , m_slotchangeEventAdded(false)
{
    setHasCustomStyleCallbacks();
}

DEFINE_NODE_FACTORY(HTMLSlotElement);

const HeapVector<Member<Node>> HTMLSlotElement::assignedNodesForBinding(const AssignedNodesOptions& options)
{
    updateDistribution();
    if (options.hasFlatten() && options.flatten())
        return getDistributedNodes();
    return m_assignedNodes;
}

const HeapVector<Member<Node>>& HTMLSlotElement::getDistributedNodes()
{
    ASSERT(!needsDistributionRecalc());
    if (isInShadowTree())
        return m_distributedNodes;

    // A slot is unlikely to be used outside of a shadow tree.
    // We do not need to optimize this case in most cases.
    // TODO(hayato): If this path causes a performance issue, we should move
    // ShadowRootRaraDate::m_descendantSlots into TreeScopreRareData-ish and
    // update the distribution code so it considers a document tree too.
    willUpdateDistribution();
    for (Node& child : NodeTraversal::childrenOf(*this)) {
        if (!child.isSlotAssignable())
            continue;
        if (isHTMLSlotElement(child))
            m_distributedNodes.appendVector(toHTMLSlotElement(child).getDistributedNodes());
        else
            m_distributedNodes.append(&child);
    }
    didUpdateDistribution();
    return m_distributedNodes;
}

void HTMLSlotElement::appendAssignedNode(Node& node)
{
    ASSERT(m_assignmentState == AssignmentOnGoing);
    m_assignedNodes.append(&node);
}

void HTMLSlotElement::appendDistributedNode(Node& node)
{
    ASSERT(m_distributionState == DistributionOnGoing);
    size_t size = m_distributedNodes.size();
    m_distributedNodes.append(&node);
    m_distributedIndices.set(&node, size);
}

void HTMLSlotElement::appendFallbackNode(Node& node)
{
    ASSERT(m_assignmentState == AssignmentOnGoing);
    m_fallbackNodes.append(&node);
}

void HTMLSlotElement::appendDistributedNodesFrom(const HTMLSlotElement& other)
{
    ASSERT(m_distributionState == DistributionOnGoing);
    size_t index = m_distributedNodes.size();
    m_distributedNodes.appendVector(other.m_distributedNodes);
    for (const auto& node : other.m_distributedNodes)
        m_distributedIndices.set(node.get(), index++);
}

void HTMLSlotElement::willUpdateAssignment()
{
    ASSERT(m_assignmentState != AssignmentOnGoing);
    m_assignmentState = AssignmentOnGoing;
    m_oldAssignedNodes.swap(m_assignedNodes);
    m_assignedNodes.clear();
}

void HTMLSlotElement::willUpdateDistribution()
{
    ASSERT(m_distributionState != DistributionOnGoing);
    m_distributionState = DistributionOnGoing;
    m_oldDistributedNodes.swap(m_distributedNodes);
    m_distributedNodes.clear();
    m_distributedIndices.clear();
}

void HTMLSlotElement::willUpdateFallback()
{
    m_oldFallbackNodes.swap(m_fallbackNodes);
    m_fallbackNodes.clear();
}

void HTMLSlotElement::dispatchSlotChangeEvent()
{
    Event* event = Event::create(EventTypeNames::slotchange);
    event->setTarget(this);
    dispatchScopedEvent(event);
    m_slotchangeEventAdded = false;
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

AtomicString HTMLSlotElement::name() const
{
    return normalizeSlotName(fastGetAttribute(HTMLNames::nameAttr));
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
        if (ShadowRoot* root = containingShadowRoot()) {
            root->owner()->willAffectSelector();
            if (root->isV1() && oldValue != newValue)
                root->assignV1();
        }
    }
    HTMLElement::attributeChanged(name, oldValue, newValue, reason);
}

void HTMLSlotElement::childrenChanged(const ChildrenChange& change)
{
    HTMLElement::childrenChanged(change);
    if (ShadowRoot* root = containingShadowRoot()) {
        if (ElementShadow* rootOwner = root->owner()) {
            rootOwner->setNeedsDistributionRecalc();
        }
        if (m_assignedNodes.isEmpty() && root->isV1())
            root->assignV1();
    }
}

Node::InsertionNotificationRequest HTMLSlotElement::insertedInto(ContainerNode* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    ShadowRoot* root = containingShadowRoot();
    if (root) {
        if (ElementShadow* rootOwner = root->owner())
            rootOwner->setNeedsDistributionRecalc();
        if (root == insertionPoint->treeScope().rootNode())
            root->didAddSlot();
    }

    // We could have been distributed into in a detached subtree, make sure to
    // clear the distribution when inserted again to avoid cycles.
    clearDistribution();

    if (root && root->isV1())
        root->assignV1();

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

    ContainerNode& rootNode = insertionPoint->treeScope().rootNode();
    if (root == &rootNode)
        root->didRemoveSlot();
    else if (rootNode.isShadowRoot() && toShadowRoot(rootNode).isV1())
        toShadowRoot(rootNode).assignV1();

    if (root && root->isV1())
        root->assignV1();
    HTMLElement::removedFrom(insertionPoint);
}

void HTMLSlotElement::willRecalcStyle(StyleRecalcChange change)
{
    if (change < Inherit && getStyleChangeType() < SubtreeStyleChange)
        return;

    for (auto& node : m_distributedNodes)
        node->setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::PropagateInheritChangeToDistributedNodes));
}

void HTMLSlotElement::updateFallbackNodes()
{
    if (!m_fallbackNodes.isEmpty())
        return;
    for (auto& child : NodeTraversal::childrenOf(*this)) {
        if (!child.isSlotAssignable())
            continue;
        // Insertion points are not supported as slots fallback
        if (isActiveInsertionPoint(child))
            continue;
        appendFallbackNode(child);
    }
}

void HTMLSlotElement::updateDistributedNodesWithFallback()
{
    if (!m_distributedNodes.isEmpty())
        return;
    for (auto node : m_fallbackNodes) {
        if (isHTMLSlotElement(node))
            appendDistributedNodesFrom(*toHTMLSlotElement(node));
        else
            appendDistributedNode(*node);
    }
}

bool HTMLSlotElement::assignmentChanged()
{
    ASSERT(m_assignmentState != AssignmentOnGoing);
    if (m_assignmentState == AssignmentDone)
        m_assignmentState = m_oldAssignedNodes == m_assignedNodes ? AssignmentUnchanged : AssignmentChanged;
    return m_assignmentState == AssignmentChanged;
}

bool HTMLSlotElement::distributionChanged()
{
    ASSERT(m_distributionState != DistributionOnGoing);
    if (m_distributionState == DistributionDone)
        m_distributionState = m_oldDistributedNodes == m_distributedNodes ? DistributionUnchanged : DistributionChanged;
    return m_distributionState == DistributionChanged;
}

bool HTMLSlotElement::fallbackChanged()
{
    return m_oldFallbackNodes == m_fallbackNodes;
}

void HTMLSlotElement::didUpdateAssignment()
{
    ASSERT(m_assignmentState == AssignmentOnGoing);
    m_assignmentState = AssignmentDone;
    if ((assignmentChanged() || fallbackChanged()) && !m_slotchangeEventAdded)
        fireSlotChangeEvent();
}

void HTMLSlotElement::didUpdateDistribution()
{
    ASSERT(m_distributionState == DistributionOnGoing);
    m_distributionState = DistributionDone;
    if (isChildOfV1ShadowHost()) {
        ElementShadow* shadow = parentElementShadow();
        ASSERT(shadow);
        if (!shadow->needsDistributionRecalc() && distributionChanged())
            shadow->setNeedsDistributionRecalc();
    }
}

void HTMLSlotElement::fireSlotChangeEvent()
{
    ASSERT(!m_slotchangeEventAdded);
    Microtask::enqueueMicrotask(WTF::bind(&HTMLSlotElement::dispatchSlotChangeEvent, this));
    m_slotchangeEventAdded = true;

    Element* shadowHost = isShadowHost(parentElement()) ? parentElement() : nullptr;
    // If this slot is assigned to another slot, fire slot change event of that slot too.
    if (shadowHost && shadowHost->shadowRootIfV1()) {
        if (HTMLSlotElement* assigned = assignedSlot()) {
            if (!assigned->m_slotchangeEventAdded)
                assigned->fireSlotChangeEvent();
        }
    }
}

void HTMLSlotElement::clearDistribution()
{
    willUpdateDistribution();
    didUpdateDistribution();
}

short HTMLSlotElement::tabIndex() const
{
    return Element::tabIndex();
}

DEFINE_TRACE(HTMLSlotElement)
{
    visitor->trace(m_assignedNodes);
    visitor->trace(m_distributedNodes);
    visitor->trace(m_fallbackNodes);
    visitor->trace(m_distributedIndices);
    visitor->trace(m_oldAssignedNodes);
    visitor->trace(m_oldDistributedNodes);
    visitor->trace(m_oldFallbackNodes);
    HTMLElement::trace(visitor);
}

} // namespace blink
