// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/invalidation/StyleInvalidator.h"

#include "core/css/invalidation/InvalidationSet.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/StyleChangeReason.h"
#include "core/html/HTMLSlotElement.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/layout/LayoutObject.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

// StyleInvalidator methods are super sensitive to performance benchmarks.
// We easily get 1% regression per additional if statement on recursive
// invalidate methods.
// To minimize performance impact, we wrap trace events with a lookup of
// cached flag. The cached flag is made "static const" and is not shared
// with InvalidationSet to avoid additional GOT lookup cost.
static const unsigned char* g_style_invalidator_tracing_enabled = nullptr;

#define TRACE_STYLE_INVALIDATOR_INVALIDATION_IF_ENABLED(element, reason) \
  if (UNLIKELY(*g_style_invalidator_tracing_enabled))                    \
    TRACE_STYLE_INVALIDATOR_INVALIDATION(element, reason);

void StyleInvalidator::Invalidate(Document& document) {
  RecursionData recursion_data;
  SiblingData sibling_data;
  if (UNLIKELY(document.NeedsStyleInvalidation()))
    PushInvalidationSetsForContainerNode(document, recursion_data,
                                         sibling_data);
  if (Element* document_element = document.documentElement())
    Invalidate(*document_element, recursion_data, sibling_data);
  document.ClearChildNeedsStyleInvalidation();
  document.ClearNeedsStyleInvalidation();
  pending_invalidation_map_.clear();
}

void StyleInvalidator::ScheduleInvalidationSetsForNode(
    const InvalidationLists& invalidation_lists,
    ContainerNode& node) {
  DCHECK(node.InActiveDocument());
  bool requires_descendant_invalidation = false;

  if (node.GetStyleChangeType() < kSubtreeStyleChange) {
    for (auto& invalidation_set : invalidation_lists.descendants) {
      if (invalidation_set->WholeSubtreeInvalid()) {
        node.SetNeedsStyleRecalc(kSubtreeStyleChange,
                                 StyleChangeReasonForTracing::Create(
                                     StyleChangeReason::kStyleInvalidator));
        requires_descendant_invalidation = false;
        break;
      }

      if (invalidation_set->InvalidatesSelf())
        node.SetNeedsStyleRecalc(kLocalStyleChange,
                                 StyleChangeReasonForTracing::Create(
                                     StyleChangeReason::kStyleInvalidator));

      if (!invalidation_set->IsEmpty())
        requires_descendant_invalidation = true;
    }
  }

  if (!requires_descendant_invalidation &&
      (invalidation_lists.siblings.IsEmpty() || !node.nextSibling()))
    return;

  node.SetNeedsStyleInvalidation();

  PendingInvalidations& pending_invalidations =
      EnsurePendingInvalidations(node);
  if (node.nextSibling()) {
    for (auto& invalidation_set : invalidation_lists.siblings) {
      if (pending_invalidations.Siblings().Contains(invalidation_set))
        continue;
      pending_invalidations.Siblings().push_back(invalidation_set);
    }
  }

  if (!requires_descendant_invalidation)
    return;

  for (auto& invalidation_set : invalidation_lists.descendants) {
    DCHECK(!invalidation_set->WholeSubtreeInvalid());
    if (invalidation_set->IsEmpty())
      continue;
    if (pending_invalidations.Descendants().Contains(invalidation_set))
      continue;
    pending_invalidations.Descendants().push_back(invalidation_set);
  }
}

void StyleInvalidator::ScheduleSiblingInvalidationsAsDescendants(
    const InvalidationLists& invalidation_lists,
    ContainerNode& scheduling_parent) {
  DCHECK(invalidation_lists.descendants.IsEmpty());

  if (invalidation_lists.siblings.IsEmpty())
    return;

  PendingInvalidations& pending_invalidations =
      EnsurePendingInvalidations(scheduling_parent);

  scheduling_parent.SetNeedsStyleInvalidation();

  for (auto& invalidation_set : invalidation_lists.siblings) {
    if (invalidation_set->WholeSubtreeInvalid()) {
      scheduling_parent.SetNeedsStyleRecalc(
          kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                                   StyleChangeReason::kStyleInvalidator));
      return;
    }
    if (invalidation_set->InvalidatesSelf() &&
        !pending_invalidations.Descendants().Contains(invalidation_set))
      pending_invalidations.Descendants().push_back(invalidation_set);

    if (DescendantInvalidationSet* descendants =
            ToSiblingInvalidationSet(*invalidation_set).SiblingDescendants()) {
      if (descendants->WholeSubtreeInvalid()) {
        scheduling_parent.SetNeedsStyleRecalc(
            kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                                     StyleChangeReason::kStyleInvalidator));
        return;
      }
      if (!pending_invalidations.Descendants().Contains(descendants))
        pending_invalidations.Descendants().push_back(descendants);
    }
  }
}

void StyleInvalidator::RescheduleSiblingInvalidationsAsDescendants(
    Element& element) {
  DCHECK(element.parentNode());
  PendingInvalidations* pending_invalidations =
      pending_invalidation_map_.at(&element);
  if (!pending_invalidations || pending_invalidations->Siblings().IsEmpty())
    return;

  InvalidationLists invalidation_lists;
  for (const auto& invalidation_set : pending_invalidations->Siblings()) {
    invalidation_lists.descendants.push_back(invalidation_set);
    if (DescendantInvalidationSet* descendants =
            ToSiblingInvalidationSet(*invalidation_set).SiblingDescendants()) {
      invalidation_lists.descendants.push_back(descendants);
    }
  }
  ScheduleInvalidationSetsForNode(invalidation_lists, *element.parentNode());
}

void StyleInvalidator::ClearInvalidation(ContainerNode& node) {
  if (!node.NeedsStyleInvalidation())
    return;
  pending_invalidation_map_.erase(&node);
  node.ClearNeedsStyleInvalidation();
}

PendingInvalidations& StyleInvalidator::EnsurePendingInvalidations(
    ContainerNode& node) {
  PendingInvalidationMap::AddResult add_result =
      pending_invalidation_map_.insert(&node, nullptr);
  if (add_result.is_new_entry)
    add_result.stored_value->value = WTF::MakeUnique<PendingInvalidations>();
  return *add_result.stored_value->value;
}

StyleInvalidator::StyleInvalidator() {
  g_style_invalidator_tracing_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"));
  InvalidationSet::CacheTracingFlag();
}

StyleInvalidator::~StyleInvalidator() {}

void StyleInvalidator::RecursionData::PushInvalidationSet(
    const InvalidationSet& invalidation_set) {
  DCHECK(!whole_subtree_invalid_);
  DCHECK(!invalidation_set.WholeSubtreeInvalid());
  DCHECK(!invalidation_set.IsEmpty());
  if (invalidation_set.CustomPseudoInvalid())
    invalidate_custom_pseudo_ = true;
  if (invalidation_set.TreeBoundaryCrossing())
    tree_boundary_crossing_ = true;
  if (invalidation_set.InsertionPointCrossing())
    insertion_point_crossing_ = true;
  if (invalidation_set.InvalidatesSlotted())
    invalidates_slotted_ = true;
  invalidation_sets_.push_back(&invalidation_set);
}

ALWAYS_INLINE bool
StyleInvalidator::RecursionData::MatchesCurrentInvalidationSets(
    Element& element) const {
  if (invalidate_custom_pseudo_ && element.ShadowPseudoId() != g_null_atom) {
    TRACE_STYLE_INVALIDATOR_INVALIDATION_IF_ENABLED(element,
                                                    kInvalidateCustomPseudo);
    return true;
  }

  if (insertion_point_crossing_ && element.IsV0InsertionPoint())
    return true;

  for (const auto& invalidation_set : invalidation_sets_) {
    if (invalidation_set->InvalidatesElement(element))
      return true;
  }

  return false;
}

bool StyleInvalidator::RecursionData::MatchesCurrentInvalidationSetsAsSlotted(
    Element& element) const {
  DCHECK(invalidates_slotted_);

  for (const auto& invalidation_set : invalidation_sets_) {
    if (!invalidation_set->InvalidatesSlotted())
      continue;
    if (invalidation_set->InvalidatesElement(element))
      return true;
  }
  return false;
}

void StyleInvalidator::SiblingData::PushInvalidationSet(
    const SiblingInvalidationSet& invalidation_set) {
  unsigned invalidation_limit;
  if (invalidation_set.MaxDirectAdjacentSelectors() == UINT_MAX)
    invalidation_limit = UINT_MAX;
  else
    invalidation_limit =
        element_index_ + invalidation_set.MaxDirectAdjacentSelectors();
  invalidation_entries_.push_back(Entry(&invalidation_set, invalidation_limit));
}

bool StyleInvalidator::SiblingData::MatchCurrentInvalidationSets(
    Element& element,
    RecursionData& recursion_data) {
  bool this_element_needs_style_recalc = false;
  DCHECK(!recursion_data.WholeSubtreeInvalid());

  unsigned index = 0;
  while (index < invalidation_entries_.size()) {
    if (element_index_ > invalidation_entries_[index].invalidation_limit_) {
      // invalidation_entries_[index] only applies to earlier siblings. Remove
      // it.
      invalidation_entries_[index] = invalidation_entries_.back();
      invalidation_entries_.pop_back();
      continue;
    }

    const SiblingInvalidationSet& invalidation_set =
        *invalidation_entries_[index].invalidation_set_;
    ++index;
    if (!invalidation_set.InvalidatesElement(element))
      continue;

    if (invalidation_set.InvalidatesSelf())
      this_element_needs_style_recalc = true;

    if (const DescendantInvalidationSet* descendants =
            invalidation_set.SiblingDescendants()) {
      if (descendants->WholeSubtreeInvalid()) {
        element.SetNeedsStyleRecalc(kSubtreeStyleChange,
                                    StyleChangeReasonForTracing::Create(
                                        StyleChangeReason::kStyleInvalidator));
        return true;
      }

      if (!descendants->IsEmpty())
        recursion_data.PushInvalidationSet(*descendants);
    }
  }
  return this_element_needs_style_recalc;
}

void StyleInvalidator::PushInvalidationSetsForContainerNode(
    ContainerNode& node,
    RecursionData& recursion_data,
    SiblingData& sibling_data) {
  PendingInvalidations* pending_invalidations =
      pending_invalidation_map_.at(&node);
  DCHECK(pending_invalidations);

  for (const auto& invalidation_set : pending_invalidations->Siblings()) {
    CHECK(invalidation_set->IsAlive());
    sibling_data.PushInvalidationSet(
        ToSiblingInvalidationSet(*invalidation_set));
  }

  if (node.GetStyleChangeType() >= kSubtreeStyleChange)
    return;

  if (!pending_invalidations->Descendants().IsEmpty()) {
    for (const auto& invalidation_set : pending_invalidations->Descendants()) {
      CHECK(invalidation_set->IsAlive());
      recursion_data.PushInvalidationSet(*invalidation_set);
    }
    if (UNLIKELY(*g_style_invalidator_tracing_enabled)) {
      TRACE_EVENT_INSTANT1(
          TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
          "StyleInvalidatorInvalidationTracking", TRACE_EVENT_SCOPE_THREAD,
          "data",
          InspectorStyleInvalidatorInvalidateEvent::InvalidationList(
              node, pending_invalidations->Descendants()));
    }
  }
}

ALWAYS_INLINE bool StyleInvalidator::CheckInvalidationSetsAgainstElement(
    Element& element,
    RecursionData& recursion_data,
    SiblingData& sibling_data) {
  if (recursion_data.WholeSubtreeInvalid())
    return false;

  bool this_element_needs_style_recalc = false;
  if (element.GetStyleChangeType() >= kSubtreeStyleChange) {
    recursion_data.SetWholeSubtreeInvalid();
  } else {
    this_element_needs_style_recalc =
        recursion_data.MatchesCurrentInvalidationSets(element);
    if (UNLIKELY(!sibling_data.IsEmpty()))
      this_element_needs_style_recalc |=
          sibling_data.MatchCurrentInvalidationSets(element, recursion_data);
  }

  if (UNLIKELY(element.NeedsStyleInvalidation()))
    PushInvalidationSetsForContainerNode(element, recursion_data, sibling_data);

  return this_element_needs_style_recalc;
}

bool StyleInvalidator::InvalidateShadowRootChildren(
    Element& element,
    RecursionData& recursion_data) {
  bool some_children_need_style_recalc = false;
  for (ShadowRoot* root = element.YoungestShadowRoot(); root;
       root = root->OlderShadowRoot()) {
    if (!recursion_data.TreeBoundaryCrossing() &&
        !root->ChildNeedsStyleInvalidation() && !root->NeedsStyleInvalidation())
      continue;
    RecursionCheckpoint checkpoint(&recursion_data);
    SiblingData sibling_data;
    if (UNLIKELY(root->NeedsStyleInvalidation()))
      PushInvalidationSetsForContainerNode(*root, recursion_data, sibling_data);
    for (Element* child = ElementTraversal::FirstChild(*root); child;
         child = ElementTraversal::NextSibling(*child)) {
      bool child_recalced = Invalidate(*child, recursion_data, sibling_data);
      some_children_need_style_recalc =
          some_children_need_style_recalc || child_recalced;
    }
    root->ClearChildNeedsStyleInvalidation();
    root->ClearNeedsStyleInvalidation();
  }
  return some_children_need_style_recalc;
}

bool StyleInvalidator::InvalidateChildren(Element& element,
                                          RecursionData& recursion_data) {
  SiblingData sibling_data;
  bool some_children_need_style_recalc = false;
  if (UNLIKELY(!!element.YoungestShadowRoot())) {
    some_children_need_style_recalc =
        InvalidateShadowRootChildren(element, recursion_data);
  }

  for (Element* child = ElementTraversal::FirstChild(element); child;
       child = ElementTraversal::NextSibling(*child)) {
    bool child_recalced = Invalidate(*child, recursion_data, sibling_data);
    some_children_need_style_recalc =
        some_children_need_style_recalc || child_recalced;
  }
  return some_children_need_style_recalc;
}

bool StyleInvalidator::Invalidate(Element& element,
                                  RecursionData& recursion_data,
                                  SiblingData& sibling_data) {
  sibling_data.Advance();
  RecursionCheckpoint checkpoint(&recursion_data);

  bool this_element_needs_style_recalc = CheckInvalidationSetsAgainstElement(
      element, recursion_data, sibling_data);

  bool some_children_need_style_recalc = false;
  if (recursion_data.HasInvalidationSets() ||
      element.ChildNeedsStyleInvalidation())
    some_children_need_style_recalc =
        InvalidateChildren(element, recursion_data);

  if (this_element_needs_style_recalc) {
    DCHECK(!recursion_data.WholeSubtreeInvalid());
    element.SetNeedsStyleRecalc(kLocalStyleChange,
                                StyleChangeReasonForTracing::Create(
                                    StyleChangeReason::kStyleInvalidator));
  } else if (recursion_data.HasInvalidationSets() &&
             some_children_need_style_recalc) {
    // Clone the ComputedStyle in order to preserve correct style sharing, if
    // possible. Otherwise recalc style.
    if (LayoutObject* layout_object = element.GetLayoutObject()) {
      layout_object->SetStyleInternal(
          ComputedStyle::Clone(layout_object->StyleRef()));
    } else {
      element.SetNeedsStyleRecalc(kLocalStyleChange,
                                  StyleChangeReasonForTracing::Create(
                                      StyleChangeReason::kStyleInvalidator));
    }
  }

  if (recursion_data.InsertionPointCrossing() && element.IsV0InsertionPoint())
    element.SetNeedsStyleRecalc(kSubtreeStyleChange,
                                StyleChangeReasonForTracing::Create(
                                    StyleChangeReason::kStyleInvalidator));
  if (recursion_data.InvalidatesSlotted() && isHTMLSlotElement(element))
    InvalidateSlotDistributedElements(toHTMLSlotElement(element),
                                      recursion_data);

  element.ClearChildNeedsStyleInvalidation();
  element.ClearNeedsStyleInvalidation();

  return this_element_needs_style_recalc;
}

void StyleInvalidator::InvalidateSlotDistributedElements(
    HTMLSlotElement& slot,
    const RecursionData& recursion_data) const {
  for (auto& distributed_node : slot.GetDistributedNodes()) {
    if (distributed_node->NeedsStyleRecalc())
      continue;
    if (!distributed_node->IsElementNode())
      continue;
    if (recursion_data.MatchesCurrentInvalidationSetsAsSlotted(
            ToElement(*distributed_node)))
      distributed_node->SetNeedsStyleRecalc(
          kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                 StyleChangeReason::kStyleInvalidator));
  }
}

}  // namespace blink
