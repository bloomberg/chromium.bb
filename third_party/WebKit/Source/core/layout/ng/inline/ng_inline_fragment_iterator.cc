// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_fragment_iterator.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "platform/wtf/HashMap.h"

namespace blink {

NGInlineFragmentIterator::NGInlineFragmentIterator(
    const NGPhysicalBoxFragment& box,
    const LayoutObject* filter) {
  DCHECK(filter);

  CollectInlineFragments(box, {}, filter, &results_);
}

// Create a map from a LayoutObject to a vector of PhysicalFragment and its
// offset to the container box. This is done by collecting inline child
// fragments of the container fragment, while accumulating the offset to the
// container box.
void NGInlineFragmentIterator::CollectInlineFragments(
    const NGPhysicalContainerFragment& container,
    NGPhysicalOffset offset_to_container_box,
    const LayoutObject* filter,
    Results* results) {
  for (const auto& child : container.Children()) {
    NGPhysicalOffset child_offset = child->Offset() + offset_to_container_box;

    if (filter == child->GetLayoutObject()) {
      results->push_back(Result{child.get(), child_offset});
    }

    // Traverse descendants unless the fragment is laid out separately from the
    // inline layout algorithm.
    if (child->IsContainer() && !child->IsBlockLayoutRoot()) {
      CollectInlineFragments(ToNGPhysicalContainerFragment(*child),
                             child_offset, filter, results);
    }
  }
}

}  // namespace blink
