/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
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
 *
 */

#ifndef NodeRareData_h
#define NodeRareData_h

#include "core/dom/MutationObserverRegistration.h"
#include "core/dom/NodeListsNodeData.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"

namespace blink {

class NodeMutationObserverData final
    : public GarbageCollected<NodeMutationObserverData> {
  WTF_MAKE_NONCOPYABLE(NodeMutationObserverData);

 public:
  static NodeMutationObserverData* Create() {
    return new NodeMutationObserverData;
  }

  const HeapVector<TraceWrapperMember<MutationObserverRegistration>>&
  Registry() {
    return registry_;
  }

  const HeapHashSet<TraceWrapperMember<MutationObserverRegistration>>&
  TransientRegistry() {
    return transient_registry_;
  }

  void AddTransientRegistration(MutationObserverRegistration* registration) {
    transient_registry_.insert(registration);
  }

  void RemoveTransientRegistration(MutationObserverRegistration* registration) {
    DCHECK(transient_registry_.Contains(registration));
    transient_registry_.erase(registration);
  }

  void AddRegistration(MutationObserverRegistration* registration) {
    registry_.push_back(registration);
  }

  void RemoveRegistration(MutationObserverRegistration* registration) {
    DCHECK(registry_.Contains(registration));
    registry_.erase(registry_.Find(registration));
  }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(registry_);
    visitor->Trace(transient_registry_);
  }

  DEFINE_INLINE_TRACE_WRAPPERS() {
    for (auto registration : registry_) {
      visitor->TraceWrappers(registration);
    }
    for (auto registration : transient_registry_) {
      visitor->TraceWrappers(registration);
    }
  }

 private:
  NodeMutationObserverData() {}

  HeapVector<TraceWrapperMember<MutationObserverRegistration>> registry_;
  HeapHashSet<TraceWrapperMember<MutationObserverRegistration>>
      transient_registry_;
};

DEFINE_TRAIT_FOR_TRACE_WRAPPERS(NodeMutationObserverData);

class NodeRareData : public GarbageCollectedFinalized<NodeRareData>,
                     public NodeRareDataBase {
  WTF_MAKE_NONCOPYABLE(NodeRareData);

 public:
  static NodeRareData* Create(NodeRenderingData* node_layout_data) {
    return new NodeRareData(node_layout_data);
  }

  void ClearNodeLists() { node_lists_.Clear(); }
  NodeListsNodeData* NodeLists() const { return node_lists_.Get(); }
  // ensureNodeLists() and a following NodeListsNodeData functions must be
  // wrapped with a ThreadState::GCForbiddenScope in order to avoid an
  // initialized m_nodeLists is cleared by NodeRareData::traceAfterDispatch().
  NodeListsNodeData& EnsureNodeLists() {
    DCHECK(ThreadState::Current()->IsGCForbidden());
    if (!node_lists_) {
      node_lists_ = NodeListsNodeData::Create();
      ScriptWrappableVisitor::WriteBarrier(node_lists_);
    }
    return *node_lists_;
  }

  NodeMutationObserverData* MutationObserverData() {
    return mutation_observer_data_.Get();
  }
  NodeMutationObserverData& EnsureMutationObserverData() {
    if (!mutation_observer_data_) {
      mutation_observer_data_ = NodeMutationObserverData::Create();
      ScriptWrappableVisitor::WriteBarrier(mutation_observer_data_);
    }
    return *mutation_observer_data_;
  }

  unsigned ConnectedSubframeCount() const { return connected_frame_count_; }
  void IncrementConnectedSubframeCount();
  void DecrementConnectedSubframeCount() {
    DCHECK(connected_frame_count_);
    --connected_frame_count_;
  }

  bool HasElementFlag(ElementFlags mask) const { return element_flags_ & mask; }
  void SetElementFlag(ElementFlags mask, bool value) {
    element_flags_ = (element_flags_ & ~mask) | (-(int32_t)value & mask);
  }
  void ClearElementFlag(ElementFlags mask) { element_flags_ &= ~mask; }

  bool HasRestyleFlag(DynamicRestyleFlags mask) const {
    return restyle_flags_ & mask;
  }
  void SetRestyleFlag(DynamicRestyleFlags mask) {
    restyle_flags_ |= mask;
    CHECK(restyle_flags_);
  }
  bool HasRestyleFlags() const { return restyle_flags_; }
  void ClearRestyleFlags() { restyle_flags_ = 0; }

  enum {
    kConnectedFrameCountBits = 10,  // Must fit Page::maxNumberOfFrames.
  };

  DECLARE_TRACE();

  DECLARE_TRACE_AFTER_DISPATCH();
  void FinalizeGarbageCollectedObject();

  DECLARE_TRACE_WRAPPERS();
  DECLARE_TRACE_WRAPPERS_AFTER_DISPATCH();

 protected:
  explicit NodeRareData(NodeRenderingData* node_layout_data)
      : NodeRareDataBase(node_layout_data),
        connected_frame_count_(0),
        element_flags_(0),
        restyle_flags_(0),
        is_element_rare_data_(false) {
    CHECK_NE(node_layout_data, nullptr);
  }

 private:
  Member<NodeListsNodeData> node_lists_;
  Member<NodeMutationObserverData> mutation_observer_data_;

  unsigned connected_frame_count_ : kConnectedFrameCountBits;
  unsigned element_flags_ : kNumberOfElementFlags;
  unsigned restyle_flags_ : kNumberOfDynamicRestyleFlags;

 protected:
  unsigned is_element_rare_data_ : 1;
};

DEFINE_TRAIT_FOR_TRACE_WRAPPERS(NodeRareData);

}  // namespace blink

#endif  // NodeRareData_h
