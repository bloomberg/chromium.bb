// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_boundary.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"

namespace blink {

bool DisplayLockUtilities::ActivateFindInPageMatchRangeIfNeeded(
    const EphemeralRangeInFlatTree& range) {
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled())
    return false;
  DCHECK(!range.IsNull());
  DCHECK(!range.IsCollapsed());
  if (range.GetDocument().LockedDisplayLockCount() ==
      range.GetDocument().ActivationBlockingDisplayLockCount())
    return false;
  // Find-in-page matches can't span multiple block-level elements (because the
  // text will be broken by newlines between blocks), so first we find the
  // block-level element which contains the match.
  // This means we only need to traverse up from one node in the range, in this
  // case we are traversing from the start position of the range.
  Element* enclosing_block =
      EnclosingBlock(range.StartPosition(), kCannotCrossEditingBoundary);
  DCHECK(enclosing_block);
  DCHECK_EQ(enclosing_block,
            EnclosingBlock(range.EndPosition(), kCannotCrossEditingBoundary));
  const HeapVector<Member<Element>>& elements_to_activate =
      ActivatableLockedInclusiveAncestors(*enclosing_block);
  for (Element* element : elements_to_activate) {
    // We save the elements to a vector and go through & activate them one by
    // one like this because the DOM structure might change due to running event
    // handlers of the beforeactivate event.
    element->ActivateDisplayLockIfNeeded();
  }
  return !elements_to_activate.IsEmpty();
}

const HeapVector<Member<Element>>
DisplayLockUtilities::ActivatableLockedInclusiveAncestors(Element& element) {
  HeapVector<Member<Element>> elements_to_activate;
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(element)) {
    if (!ancestor.IsElementNode())
      continue;
    if (auto* context = ToElement(ancestor).GetDisplayLockContext()) {
      DCHECK(context->IsActivatable());
      if (!context->IsLocked())
        continue;
      elements_to_activate.push_back(&ToElement(ancestor));
    }
  }
  return elements_to_activate;
}

DisplayLockUtilities::ScopedChainForcedUpdate::ScopedChainForcedUpdate(
    const Node* node) {
  if (!RuntimeEnabledFeatures::DisplayLockingEnabled() ||
      node->GetDocument().LockedDisplayLockCount() == 0) {
    return;
  }
  const_cast<Node*>(node)->UpdateDistributionForFlatTreeTraversal();
  // TODO(vmpstr): This is somewhat inefficient, since we would pay the cost
  // of traversing the ancestor chain even for nodes that are not in the
  // locked subtree. We need to figure out if there is a supplementary
  // structure that we can use to quickly identify nodes that are in the
  // locked subtree.
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(*node)) {
    if (!ancestor.IsElementNode())
      continue;
    if (auto* context = ToElement(ancestor).GetDisplayLockContext())
      scoped_update_forced_list_.push_back(context->GetScopedForcedUpdate());
  }
}

}  // namespace blink
