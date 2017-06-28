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

#include "core/events/EventPath.h"

#include "core/EventNames.h"
#include "core/dom/Document.h"
#include "core/dom/InsertionPoint.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/Touch.h"
#include "core/dom/TouchList.h"
#include "core/events/TouchEvent.h"
#include "core/events/TouchEventContext.h"
#include "core/html/HTMLSlotElement.h"

namespace blink {

EventTarget* EventPath::EventTargetRespectingTargetRules(Node& reference_node) {
  if (reference_node.IsPseudoElement()) {
    DCHECK(reference_node.parentNode());
    return reference_node.parentNode();
  }

  return &reference_node;
}

static inline bool ShouldStopAtShadowRoot(Event& event,
                                          ShadowRoot& shadow_root,
                                          EventTarget& target) {
  if (shadow_root.IsV1()) {
    // In v1, an event is scoped by default unless event.composed flag is set.
    return !event.composed() && target.ToNode() &&
           target.ToNode()->OwnerShadowHost() == shadow_root.host();
  }
  // Ignores event.composed() for v0.
  // Instead, use event.isScopedInV0() for backward compatibility.
  return event.IsScopedInV0() && target.ToNode() &&
         target.ToNode()->OwnerShadowHost() == shadow_root.host();
}

EventPath::EventPath(Node& node, Event* event) : node_(node), event_(event) {
  Initialize();
}

void EventPath::InitializeWith(Node& node, Event* event) {
  node_ = &node;
  event_ = event;
  window_event_context_ = nullptr;
  node_event_contexts_.clear();
  tree_scope_event_contexts_.clear();
  Initialize();
}

static inline bool EventPathShouldBeEmptyFor(Node& node) {
  return node.IsPseudoElement() && !node.parentElement();
}

void EventPath::Initialize() {
  if (EventPathShouldBeEmptyFor(*node_))
    return;

  CalculatePath();
  CalculateAdjustedTargets();
  CalculateTreeOrderAndSetNearestAncestorClosedTree();
}

void EventPath::CalculatePath() {
  DCHECK(node_);
  DCHECK(node_event_contexts_.IsEmpty());
  node_->UpdateDistribution();

  // For performance and memory usage reasons we want to store the
  // path using as few bytes as possible and with as few allocations
  // as possible which is why we gather the data on the stack before
  // storing it in a perfectly sized m_nodeEventContexts Vector.
  HeapVector<Member<Node>, 64> nodes_in_path;
  Node* current = node_;

  nodes_in_path.push_back(current);
  while (current) {
    if (event_ && current->KeepEventInNode(event_))
      break;
    HeapVector<Member<InsertionPoint>, 8> insertion_points;
    CollectDestinationInsertionPoints(*current, insertion_points);
    if (!insertion_points.IsEmpty()) {
      for (const auto& insertion_point : insertion_points) {
        if (insertion_point->IsShadowInsertionPoint()) {
          ShadowRoot* containing_shadow_root =
              insertion_point->ContainingShadowRoot();
          DCHECK(containing_shadow_root);
          if (!containing_shadow_root->IsOldest())
            nodes_in_path.push_back(containing_shadow_root->OlderShadowRoot());
        }
        nodes_in_path.push_back(insertion_point);
      }
      current = insertion_points.back();
      continue;
    }
    if (current->IsChildOfV1ShadowHost()) {
      if (HTMLSlotElement* slot = current->AssignedSlot()) {
        current = slot;
        nodes_in_path.push_back(current);
        continue;
      }
    }
    if (current->IsShadowRoot()) {
      if (event_ &&
          ShouldStopAtShadowRoot(*event_, *ToShadowRoot(current), *node_))
        break;
      current = current->OwnerShadowHost();
      nodes_in_path.push_back(current);
    } else {
      current = current->parentNode();
      if (current)
        nodes_in_path.push_back(current);
    }
  }

  node_event_contexts_.ReserveCapacity(nodes_in_path.size());
  for (Node* node_in_path : nodes_in_path) {
    node_event_contexts_.push_back(NodeEventContext(
        node_in_path, EventTargetRespectingTargetRules(*node_in_path)));
  }
}

void EventPath::CalculateTreeOrderAndSetNearestAncestorClosedTree() {
  // Precondition:
  //   - TreeScopes in m_treeScopeEventContexts must be *connected* in the same
  //     composed tree.
  //   - The root tree must be included.
  HeapHashMap<Member<const TreeScope>, Member<TreeScopeEventContext>>
      tree_scope_event_context_map;
  for (const auto& tree_scope_event_context : tree_scope_event_contexts_)
    tree_scope_event_context_map.insert(
        &tree_scope_event_context->GetTreeScope(),
        tree_scope_event_context.Get());
  TreeScopeEventContext* root_tree = nullptr;
  for (const auto& tree_scope_event_context : tree_scope_event_contexts_) {
    // Use olderShadowRootOrParentTreeScope here for parent-child relationships.
    // See the definition of trees of trees in the Shadow DOM spec:
    // http://w3c.github.io/webcomponents/spec/shadow/
    TreeScope* parent = tree_scope_event_context.Get()
                            ->GetTreeScope()
                            .OlderShadowRootOrParentTreeScope();
    if (!parent) {
      DCHECK(!root_tree);
      root_tree = tree_scope_event_context.Get();
      continue;
    }
    DCHECK_NE(tree_scope_event_context_map.find(parent),
              tree_scope_event_context_map.end());
    tree_scope_event_context_map.find(parent)->value->AddChild(
        *tree_scope_event_context.Get());
  }
  DCHECK(root_tree);
  root_tree->CalculateTreeOrderAndSetNearestAncestorClosedTree(0, nullptr);
}

TreeScopeEventContext* EventPath::EnsureTreeScopeEventContext(
    Node* current_target,
    TreeScope* tree_scope,
    TreeScopeEventContextMap& tree_scope_event_context_map) {
  if (!tree_scope)
    return nullptr;
  TreeScopeEventContext* tree_scope_event_context;
  bool is_new_entry;
  {
    TreeScopeEventContextMap::AddResult add_result =
        tree_scope_event_context_map.insert(tree_scope, nullptr);
    is_new_entry = add_result.is_new_entry;
    if (is_new_entry)
      add_result.stored_value->value =
          TreeScopeEventContext::Create(*tree_scope);
    tree_scope_event_context = add_result.stored_value->value.Get();
  }
  if (is_new_entry) {
    TreeScopeEventContext* parent_tree_scope_event_context =
        EnsureTreeScopeEventContext(
            0, tree_scope->OlderShadowRootOrParentTreeScope(),
            tree_scope_event_context_map);
    if (parent_tree_scope_event_context &&
        parent_tree_scope_event_context->Target()) {
      tree_scope_event_context->SetTarget(
          parent_tree_scope_event_context->Target());
    } else if (current_target) {
      tree_scope_event_context->SetTarget(
          EventTargetRespectingTargetRules(*current_target));
    }
  } else if (!tree_scope_event_context->Target() && current_target) {
    tree_scope_event_context->SetTarget(
        EventTargetRespectingTargetRules(*current_target));
  }
  return tree_scope_event_context;
}

void EventPath::CalculateAdjustedTargets() {
  const TreeScope* last_tree_scope = nullptr;

  TreeScopeEventContextMap tree_scope_event_context_map;
  TreeScopeEventContext* last_tree_scope_event_context = nullptr;

  for (auto& context : node_event_contexts_) {
    Node* current_node = context.GetNode();
    TreeScope& current_tree_scope = current_node->GetTreeScope();
    if (last_tree_scope != &current_tree_scope) {
      last_tree_scope_event_context = EnsureTreeScopeEventContext(
          current_node, &current_tree_scope, tree_scope_event_context_map);
    }
    DCHECK(last_tree_scope_event_context);
    context.SetTreeScopeEventContext(last_tree_scope_event_context);
    last_tree_scope = &current_tree_scope;
  }
  tree_scope_event_contexts_.AppendRange(
      tree_scope_event_context_map.Values().begin(),
      tree_scope_event_context_map.Values().end());
}

void EventPath::BuildRelatedNodeMap(const Node& related_node,
                                    RelatedTargetMap& related_target_map) {
  EventPath* related_target_event_path =
      new EventPath(const_cast<Node&>(related_node));
  for (const auto& tree_scope_event_context :
       related_target_event_path->tree_scope_event_contexts_) {
    related_target_map.insert(&tree_scope_event_context->GetTreeScope(),
                              tree_scope_event_context->Target());
  }
  // Oilpan: It is important to explicitly clear the vectors to reuse
  // the memory in subsequent event dispatchings.
  related_target_event_path->Clear();
}

EventTarget* EventPath::FindRelatedNode(TreeScope& scope,
                                        RelatedTargetMap& related_target_map) {
  HeapVector<Member<TreeScope>, 32> parent_tree_scopes;
  EventTarget* related_node = nullptr;
  for (TreeScope* current = &scope; current;
       current = current->OlderShadowRootOrParentTreeScope()) {
    parent_tree_scopes.push_back(current);
    RelatedTargetMap::const_iterator iter = related_target_map.find(current);
    if (iter != related_target_map.end() && iter->value) {
      related_node = iter->value;
      break;
    }
  }
  DCHECK(related_node);
  for (const auto& entry : parent_tree_scopes)
    related_target_map.insert(entry, related_node);

  return related_node;
}

void EventPath::AdjustForRelatedTarget(Node& target,
                                       EventTarget* related_target) {
  if (!related_target)
    return;
  Node* related_node = related_target->ToNode();
  if (!related_node)
    return;
  if (target.GetDocument() != related_node->GetDocument())
    return;
  RetargetRelatedTarget(*related_node);
  ShrinkForRelatedTarget(target);
}

void EventPath::RetargetRelatedTarget(const Node& related_target_node) {
  RelatedTargetMap related_node_map;
  BuildRelatedNodeMap(related_target_node, related_node_map);

  for (const auto& tree_scope_event_context : tree_scope_event_contexts_) {
    EventTarget* adjusted_related_target = FindRelatedNode(
        tree_scope_event_context->GetTreeScope(), related_node_map);
    DCHECK(adjusted_related_target);
    tree_scope_event_context.Get()->SetRelatedTarget(adjusted_related_target);
  }
}

bool EventPath::ShouldStopEventPath(EventTarget& current_target,
                                    EventTarget& current_related_target,
                                    const Node& target) {
  if (&current_target != &current_related_target)
    return false;
  if (event_->isTrusted())
    return true;
  Node* current_target_node = current_target.ToNode();
  if (!current_target_node)
    return false;
  return current_target_node->GetTreeScope() != target.GetTreeScope();
}

void EventPath::ShrinkForRelatedTarget(const Node& target) {
  for (size_t i = 0; i < size(); ++i) {
    if (ShouldStopEventPath(*at(i).Target(), *at(i).RelatedTarget(), target)) {
      Shrink(i);
      break;
    }
  }
}

void EventPath::AdjustForTouchEvent(TouchEvent& touch_event) {
  HeapVector<Member<TouchList>> adjusted_touches;
  HeapVector<Member<TouchList>> adjusted_target_touches;
  HeapVector<Member<TouchList>> adjusted_changed_touches;
  HeapVector<Member<TreeScope>> tree_scopes;

  for (const auto& tree_scope_event_context : tree_scope_event_contexts_) {
    TouchEventContext* touch_event_context =
        tree_scope_event_context->EnsureTouchEventContext();
    adjusted_touches.push_back(&touch_event_context->Touches());
    adjusted_target_touches.push_back(&touch_event_context->TargetTouches());
    adjusted_changed_touches.push_back(&touch_event_context->ChangedTouches());
    tree_scopes.push_back(&tree_scope_event_context->GetTreeScope());
  }

  AdjustTouchList(touch_event.touches(), adjusted_touches, tree_scopes);
  AdjustTouchList(touch_event.targetTouches(), adjusted_target_touches,
                  tree_scopes);
  AdjustTouchList(touch_event.changedTouches(), adjusted_changed_touches,
                  tree_scopes);

#if DCHECK_IS_ON()
  for (const auto& tree_scope_event_context : tree_scope_event_contexts_) {
    TreeScope& tree_scope = tree_scope_event_context->GetTreeScope();
    TouchEventContext* touch_event_context =
        tree_scope_event_context->GetTouchEventContext();
    CheckReachability(tree_scope, touch_event_context->Touches());
    CheckReachability(tree_scope, touch_event_context->TargetTouches());
    CheckReachability(tree_scope, touch_event_context->ChangedTouches());
  }
#endif
}

void EventPath::AdjustTouchList(
    const TouchList* touch_list,
    HeapVector<Member<TouchList>> adjusted_touch_list,
    const HeapVector<Member<TreeScope>>& tree_scopes) {
  if (!touch_list)
    return;
  for (size_t i = 0; i < touch_list->length(); ++i) {
    const Touch& touch = *touch_list->item(i);
    if (!touch.target())
      continue;

    Node* target_node = touch.target()->ToNode();
    if (!target_node)
      continue;

    RelatedTargetMap related_node_map;
    BuildRelatedNodeMap(*target_node, related_node_map);
    for (size_t j = 0; j < tree_scopes.size(); ++j) {
      adjusted_touch_list[j]->Append(touch.CloneWithNewTarget(
          FindRelatedNode(*tree_scopes[j], related_node_map)));
    }
  }
}

bool EventPath::DisabledFormControlExistsInPath() const {
  for (const auto& context : node_event_contexts_) {
    const Node* target_node = context.GetNode();
    if (target_node && IsDisabledFormControl(target_node))
      return true;
  }
  return false;
}

NodeEventContext& EventPath::TopNodeEventContext() {
  DCHECK(!IsEmpty());
  return Last();
}

void EventPath::EnsureWindowEventContext() {
  DCHECK(event_);
  if (!window_event_context_)
    window_event_context_ =
        new WindowEventContext(*event_, TopNodeEventContext());
}

#if DCHECK_IS_ON()
void EventPath::CheckReachability(TreeScope& tree_scope,
                                  TouchList& touch_list) {
  for (size_t i = 0; i < touch_list.length(); ++i)
    DCHECK(touch_list.item(i)
               ->target()
               ->ToNode()
               ->GetTreeScope()
               .IsInclusiveOlderSiblingShadowRootOrAncestorTreeScopeOf(
                   tree_scope));
}
#endif

DEFINE_TRACE(EventPath) {
  visitor->Trace(node_event_contexts_);
  visitor->Trace(node_);
  visitor->Trace(event_);
  visitor->Trace(tree_scope_event_contexts_);
  visitor->Trace(window_event_context_);
}

}  // namespace blink
