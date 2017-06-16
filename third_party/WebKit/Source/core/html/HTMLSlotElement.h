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

  const HeapVector<Member<Node>>& AssignedNodes();
  const HeapVector<Member<Node>>& GetDistributedNodes();
  const HeapVector<Member<Node>> assignedNodesForBinding(
      const AssignedNodesOptions&);

  Node* FirstDistributedNode() const {
    DCHECK(SupportsDistribution());
    return distributed_nodes_.IsEmpty() ? nullptr
                                        : distributed_nodes_.front().Get();
  }
  Node* LastDistributedNode() const {
    DCHECK(SupportsDistribution());
    return distributed_nodes_.IsEmpty() ? nullptr
                                        : distributed_nodes_.back().Get();
  }

  Node* DistributedNodeNextTo(const Node&) const;
  Node* DistributedNodePreviousTo(const Node&) const;

  void AppendAssignedNode(Node&);

  void ResolveDistributedNodes();
  void AppendDistributedNode(Node&);
  void AppendDistributedNodesFrom(const HTMLSlotElement& other);

  void UpdateDistributedNodesWithFallback();

  void LazyReattachDistributedNodesIfNeeded();

  void AttachLayoutTree(const AttachContext& = AttachContext()) final;
  void DetachLayoutTree(const AttachContext& = AttachContext()) final;
  void RebuildDistributedChildrenLayoutTrees();

  void AttributeChanged(const AttributeModificationParams&) final;

  int tabIndex() const override;
  AtomicString GetName() const;

  // This method can be slow because this has to traverse the children of a
  // shadow host.  This method should be used only when m_assignedNodes is
  // dirty.  e.g. To detect a slotchange event in DOM mutations.
  bool HasAssignedNodesSlow() const;
  bool FindHostChildWithSameSlotName() const;

  void ClearDistribution();
  void SaveAndClearDistribution();

  bool SupportsDistribution() const { return IsInV1ShadowTree(); }

  void CheckFallbackAfterInsertedIntoShadowTree();
  void CheckFallbackAfterRemovedFromShadowTree();

  void DidSlotChange(SlotChangeType);
  void DidSlotChangeAfterRemovedFromShadowTree();
  void DidSlotChangeAfterRenaming();
  void DispatchSlotChangeEvent();
  void ClearSlotChangeEventEnqueued() { slotchange_event_enqueued_ = false; }

  static AtomicString NormalizeSlotName(const AtomicString&);

  DECLARE_VIRTUAL_TRACE();

 private:
  HTMLSlotElement(Document&);

  InsertionNotificationRequest InsertedInto(ContainerNode*) final;
  void RemovedFrom(ContainerNode*) final;
  void WillRecalcStyle(StyleRecalcChange) final;

  void EnqueueSlotChangeEvent();

  bool HasSlotableChild() const;

  HeapVector<Member<Node>> assigned_nodes_;
  HeapVector<Member<Node>> distributed_nodes_;
  HeapVector<Member<Node>> old_distributed_nodes_;
  HeapHashMap<Member<const Node>, size_t> distributed_indices_;
  bool slotchange_event_enqueued_ = false;
};

}  // namespace blink

#endif  // HTMLSlotElement_h
