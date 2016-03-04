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

#ifndef HTMLSlotElement_h
#define HTMLSlotElement_h

#include "core/CoreExport.h"
#include "core/html/HTMLElement.h"

namespace blink {

class AssignedNodesOptions;

class CORE_EXPORT HTMLSlotElement final : public HTMLElement {
    DEFINE_WRAPPERTYPEINFO();
public:
    DECLARE_NODE_FACTORY(HTMLSlotElement);

    const WillBeHeapVector<RefPtrWillBeMember<Node>>& getAssignedNodes() const { ASSERT(!needsDistributionRecalc()); return m_assignedNodes; }
    const WillBeHeapVector<RefPtrWillBeMember<Node>>& getDistributedNodes();
    const WillBeHeapVector<RefPtrWillBeMember<Node>> getAssignedNodesForBinding(const AssignedNodesOptions&);

    Node* firstDistributedNode() const { return m_distributedNodes.isEmpty() ? nullptr : m_distributedNodes.first().get(); }
    Node* lastDistributedNode() const { return m_distributedNodes.isEmpty() ? nullptr : m_distributedNodes.last().get(); }

    Node* distributedNodeNextTo(const Node&) const;
    Node* distributedNodePreviousTo(const Node&) const;

    void appendAssignedNode(Node&);
    void appendDistributedNode(Node&);
    void appendDistributedNodesFrom(const HTMLSlotElement& other);
    void willUpdateDistribution();

    bool hasSlotChangeEventListener();

    void updateDistributedNodesWithFallback();
    void didUpdateDistribution();

    void attach(const AttachContext& = AttachContext()) final;
    void detach(const AttachContext& = AttachContext()) final;

    void attributeChanged(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason = ModifiedDirectly) final;

    DECLARE_VIRTUAL_TRACE();

private:
    HTMLSlotElement(Document&);

    enum DistributionState {
        DistributionOnGoing,
        DistributionDone,
        DistributionChanged,
        DistributionUnchanged
    };

    void clearDistribution();
    void childrenChanged(const ChildrenChange&) final;
    InsertionNotificationRequest insertedInto(ContainerNode*) final;
    void removedFrom(ContainerNode*) final;
    void willRecalcStyle(StyleRecalcChange) final;

    void dispatchSlotChangeEvent();
    bool distributionChanged();

    WillBeHeapVector<RefPtrWillBeMember<Node>> m_assignedNodes;
    WillBeHeapVector<RefPtrWillBeMember<Node>> m_distributedNodes;
    WillBeHeapHashMap<RawPtrWillBeMember<const Node>, size_t> m_distributedIndices;
    // TODO(hayato): Remove m_oldDistibutedNodes and make SlotAssignment check the diffirence between old and new distributed nodes for each slot to save the memories.
    WillBeHeapVector<RefPtrWillBeMember<Node>> m_oldDistributedNodes;
    DistributionState m_distributionState;
};

} // namespace blink

#endif // HTMLSlotElement_h
