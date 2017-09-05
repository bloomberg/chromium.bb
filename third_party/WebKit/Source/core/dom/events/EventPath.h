/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#ifndef EventPath_h
#define EventPath_h

#include "core/CoreExport.h"
#include "core/dom/events/NodeEventContext.h"
#include "core/dom/events/TreeScopeEventContext.h"
#include "core/dom/events/WindowEventContext.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"

namespace blink {

class Event;
class EventTarget;
class Node;
class TouchEvent;
class TouchList;
class TreeScope;

class CORE_EXPORT EventPath final : public GarbageCollected<EventPath> {
  WTF_MAKE_NONCOPYABLE(EventPath);

 public:
  explicit EventPath(Node&, Event* = nullptr);

  void InitializeWith(Node&, Event*);

  const HeapVector<NodeEventContext>& NodeEventContexts() const {
    return node_event_contexts_;
  }
  HeapVector<NodeEventContext>& NodeEventContexts() {
    return node_event_contexts_;
  }
  NodeEventContext& operator[](size_t index) {
    return node_event_contexts_[index];
  }
  const NodeEventContext& operator[](size_t index) const {
    return node_event_contexts_[index];
  }
  NodeEventContext& at(size_t index) { return node_event_contexts_[index]; }
  NodeEventContext& Last() { return node_event_contexts_[size() - 1]; }

  WindowEventContext& GetWindowEventContext() {
    DCHECK(window_event_context_);
    return *window_event_context_;
  }
  void EnsureWindowEventContext();

  bool IsEmpty() const { return node_event_contexts_.IsEmpty(); }
  size_t size() const { return node_event_contexts_.size(); }

  void AdjustForRelatedTarget(Node&, EventTarget* related_target);
  void AdjustForTouchEvent(TouchEvent&);

  bool DisabledFormControlExistsInPath() const;

  NodeEventContext& TopNodeEventContext();

  static EventTarget* EventTargetRespectingTargetRules(Node&);

  DECLARE_TRACE();
  void Clear() {
    node_event_contexts_.clear();
    tree_scope_event_contexts_.clear();
  }

 private:
  EventPath();

  void Initialize();
  void CalculatePath();
  void CalculateAdjustedTargets();
  void CalculateTreeOrderAndSetNearestAncestorClosedTree();

  bool ShouldStopEventPath(EventTarget& current_target,
                           EventTarget& current_related_target,
                           const Node& target);

  void Shrink(size_t new_size) {
    DCHECK(!window_event_context_);
    node_event_contexts_.Shrink(new_size);
  }

  void RetargetRelatedTarget(const Node& related_target_node);

  void ShrinkForRelatedTarget(const Node& target);

  void AdjustTouchList(const TouchList*,
                       HeapVector<Member<TouchList>> adjusted_touch_list,
                       const HeapVector<Member<TreeScope>>& tree_scopes);

  using TreeScopeEventContextMap =
      HeapHashMap<Member<TreeScope>, Member<TreeScopeEventContext>>;
  TreeScopeEventContext* EnsureTreeScopeEventContext(Node* current_target,
                                                     TreeScope*,
                                                     TreeScopeEventContextMap&);

  using RelatedTargetMap = HeapHashMap<Member<TreeScope>, Member<EventTarget>>;

  static void BuildRelatedNodeMap(const Node&, RelatedTargetMap&);
  static EventTarget* FindRelatedNode(TreeScope&, RelatedTargetMap&);

#if DCHECK_IS_ON()
  static void CheckReachability(TreeScope&, TouchList&);
#endif

  HeapVector<NodeEventContext> node_event_contexts_;
  Member<Node> node_;
  Member<Event> event_;
  HeapVector<Member<TreeScopeEventContext>> tree_scope_event_contexts_;
  Member<WindowEventContext> window_event_context_;
};

}  // namespace blink

#endif
