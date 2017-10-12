/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/dom/ElementShadowV0.h"

#include "core/dom/DistributedNodes.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLShadowElement.h"
#include "core/probe/CoreProbes.h"

namespace blink {

class DistributionPool final {
  STACK_ALLOCATED();

 public:
  explicit DistributionPool(const ContainerNode&);
  void Clear();
  ~DistributionPool();
  void DistributeTo(V0InsertionPoint*, ElementShadowV0*);
  void PopulateChildren(const ContainerNode&);

 private:
  void DetachNonDistributedNodes();
  HeapVector<Member<Node>, 32> nodes_;
  Vector<bool, 32> distributed_;
};

inline DistributionPool::DistributionPool(const ContainerNode& parent) {
  PopulateChildren(parent);
}

inline void DistributionPool::Clear() {
  DetachNonDistributedNodes();
  nodes_.clear();
  distributed_.clear();
}

inline void DistributionPool::PopulateChildren(const ContainerNode& parent) {
  Clear();
  for (Node* child = parent.firstChild(); child; child = child->nextSibling()) {
    // Re-distribution across v0 and v1 shadow trees is not supported
    if (IsHTMLSlotElement(child))
      continue;

    if (IsActiveV0InsertionPoint(*child)) {
      V0InsertionPoint* insertion_point = ToV0InsertionPoint(child);
      for (size_t i = 0; i < insertion_point->DistributedNodesSize(); ++i)
        nodes_.push_back(insertion_point->DistributedNodeAt(i));
    } else {
      nodes_.push_back(child);
    }
  }
  distributed_.resize(nodes_.size());
  distributed_.Fill(false);
}

void DistributionPool::DistributeTo(V0InsertionPoint* insertion_point,
                                    ElementShadowV0* element_shadow) {
  DistributedNodes distributed_nodes;

  for (size_t i = 0; i < nodes_.size(); ++i) {
    if (distributed_[i])
      continue;

    if (IsHTMLContentElement(*insertion_point) &&
        !ToHTMLContentElement(insertion_point)->CanSelectNode(nodes_, i))
      continue;

    Node* node = nodes_[i];
    distributed_nodes.Append(node);
    element_shadow->DidDistributeNode(node, insertion_point);
    distributed_[i] = true;
  }

  // Distributes fallback elements
  if (insertion_point->IsContentInsertionPoint() &&
      distributed_nodes.IsEmpty()) {
    for (Node* fallback_node = insertion_point->firstChild(); fallback_node;
         fallback_node = fallback_node->nextSibling()) {
      distributed_nodes.Append(fallback_node);
      element_shadow->DidDistributeNode(fallback_node, insertion_point);
    }
  }
  insertion_point->SetDistributedNodes(distributed_nodes);
}

inline DistributionPool::~DistributionPool() {
  DetachNonDistributedNodes();
}

inline void DistributionPool::DetachNonDistributedNodes() {
  for (size_t i = 0; i < nodes_.size(); ++i) {
    if (distributed_[i])
      continue;
    if (nodes_[i]->GetLayoutObject())
      nodes_[i]->LazyReattachIfAttached();
  }
}

ElementShadowV0* ElementShadowV0::Create(ElementShadow& element_shadow) {
  return new ElementShadowV0(element_shadow);
}

ElementShadowV0::ElementShadowV0(ElementShadow& element_shadow)
    : element_shadow_(&element_shadow), needs_select_feature_set_(false) {}

ElementShadowV0::~ElementShadowV0() {}

ShadowRoot& ElementShadowV0::YoungestShadowRoot() const {
  return element_shadow_->YoungestShadowRoot();
}

ShadowRoot& ElementShadowV0::OldestShadowRoot() const {
  return element_shadow_->OldestShadowRoot();
}

const V0InsertionPoint* ElementShadowV0::FinalDestinationInsertionPointFor(
    const Node* key) const {
  DCHECK(key);
  DCHECK(!key->NeedsDistributionRecalc());
  NodeToDestinationInsertionPoints::const_iterator it =
      node_to_insertion_points_.find(key);
  return it == node_to_insertion_points_.end() ? nullptr : it->value->back();
}

const DestinationInsertionPoints*
ElementShadowV0::DestinationInsertionPointsFor(const Node* key) const {
  DCHECK(key);
  DCHECK(!key->NeedsDistributionRecalc());
  NodeToDestinationInsertionPoints::const_iterator it =
      node_to_insertion_points_.find(key);
  return it == node_to_insertion_points_.end() ? nullptr : it->value;
}

void ElementShadowV0::Distribute() {
  HeapVector<Member<HTMLShadowElement>, 32> shadow_insertion_points;
  DistributionPool pool(element_shadow_->Host());

  for (ShadowRoot* root = &YoungestShadowRoot(); root;
       root = root->OlderShadowRoot()) {
    HTMLShadowElement* shadow_insertion_point = 0;
    for (const auto& point : root->DescendantInsertionPoints()) {
      if (!point->IsActive())
        continue;
      if (auto* shadow = ToHTMLShadowElementOrNull(*point)) {
        DCHECK(!shadow_insertion_point);
        shadow_insertion_point = shadow;
        shadow_insertion_points.push_back(shadow_insertion_point);
      } else {
        pool.DistributeTo(point, this);
        if (ElementShadow* shadow =
                ShadowWhereNodeCanBeDistributedForV0(*point))
          shadow->SetNeedsDistributionRecalc();
      }
    }
  }

  for (size_t i = shadow_insertion_points.size(); i > 0; --i) {
    HTMLShadowElement* shadow_insertion_point = shadow_insertion_points[i - 1];
    ShadowRoot* root = shadow_insertion_point->ContainingShadowRoot();
    DCHECK(root);
    if (root->IsOldest()) {
      pool.DistributeTo(shadow_insertion_point, this);
    } else if (root->OlderShadowRoot()->GetType() == root->GetType()) {
      // Only allow reprojecting older shadow roots between the same type to
      // disallow reprojecting UA elements into author shadows.
      DistributionPool older_shadow_root_pool(*root->OlderShadowRoot());
      older_shadow_root_pool.DistributeTo(shadow_insertion_point, this);
      root->OlderShadowRoot()->SetShadowInsertionPointOfYoungerShadowRoot(
          shadow_insertion_point);
    }
    if (ElementShadow* shadow =
            ShadowWhereNodeCanBeDistributedForV0(*shadow_insertion_point))
      shadow->SetNeedsDistributionRecalc();
  }
  probe::didPerformElementShadowDistribution(&element_shadow_->Host());
}

void ElementShadowV0::DidDistributeNode(const Node* node,
                                        V0InsertionPoint* insertion_point) {
  NodeToDestinationInsertionPoints::AddResult result =
      node_to_insertion_points_.insert(node, nullptr);
  if (result.is_new_entry)
    result.stored_value->value = new DestinationInsertionPoints;
  result.stored_value->value->push_back(insertion_point);
}

const SelectRuleFeatureSet& ElementShadowV0::EnsureSelectFeatureSet() {
  if (!needs_select_feature_set_)
    return select_features_;

  select_features_.Clear();
  for (const ShadowRoot* root = &OldestShadowRoot(); root;
       root = root->YoungerShadowRoot())
    CollectSelectFeatureSetFrom(*root);
  needs_select_feature_set_ = false;
  return select_features_;
}

void ElementShadowV0::CollectSelectFeatureSetFrom(const ShadowRoot& root) {
  if (!root.ContainsShadowRoots() && !root.ContainsContentElements())
    return;

  for (Element& element : ElementTraversal::DescendantsOf(root)) {
    if (ElementShadow* shadow = element.Shadow()) {
      if (!shadow->IsV1())
        select_features_.Add(shadow->V0().EnsureSelectFeatureSet());
    }
    if (auto* content = ToHTMLContentElementOrNull(element))
      select_features_.CollectFeaturesFromSelectorList(content->SelectorList());
  }
}

void ElementShadowV0::WillAffectSelector() {
  for (ElementShadow* shadow = element_shadow_; shadow;
       shadow = shadow->ContainingShadow()) {
    if (shadow->IsV1() || shadow->V0().NeedsSelectFeatureSet())
      break;
    shadow->V0().SetNeedsSelectFeatureSet();
  }
  element_shadow_->SetNeedsDistributionRecalc();
}

void ElementShadowV0::ClearDistribution() {
  node_to_insertion_points_.clear();

  for (ShadowRoot* root = &element_shadow_->YoungestShadowRoot(); root;
       root = root->OlderShadowRoot())
    root->SetShadowInsertionPointOfYoungerShadowRoot(nullptr);
}

DEFINE_TRACE(ElementShadowV0) {
  visitor->Trace(element_shadow_);
  visitor->Trace(node_to_insertion_points_);
}

DEFINE_TRACE_WRAPPERS(ElementShadowV0) {}

}  // namespace blink
