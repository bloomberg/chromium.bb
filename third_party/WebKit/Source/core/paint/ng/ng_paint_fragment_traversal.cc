// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_paint_fragment_traversal.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/paint/ng/ng_paint_fragment.h"

namespace blink {

namespace {

// Preorder traverse |container|, and collect the fragments satisfying
// |filter| into |results|.
// |filter|.IsTraverse(NGPaintFragment) returns true to traverse children.
// |filter|.IsCollectible(NGPaintFragment) returns true to collect fragment.

template <typename Filter>
void CollectPaintFragments(const NGPaintFragment& container,
                           NGPhysicalOffset offset_to_container_box,
                           Filter& filter,
                           Vector<NGPaintFragmentWithContainerOffset>* result) {
  for (const auto& child : container.Children()) {
    NGPaintFragmentWithContainerOffset fragment_with_offset{
        child.get(), child->Offset() + offset_to_container_box};
    if (filter.IsCollectible(child.get())) {
      result->push_back(fragment_with_offset);
    }
    if (filter.IsTraverse(child.get())) {
      CollectPaintFragments(*child.get(), fragment_with_offset.container_offset,
                            filter, result);
    }
  }
}

// Does not collect fragments with SelfPaintingLayer or their descendants.
class NotSelfPaintingFilter {
 public:
  bool IsCollectible(const NGPaintFragment* fragment) const {
    return !fragment->HasSelfPaintingLayer();
  }
  bool IsTraverse(const NGPaintFragment* fragment) const {
    return !fragment->HasSelfPaintingLayer();
  }
};

// Collect only fragments that belong to this LayoutObject.
class LayoutObjectFilter {
 public:
  explicit LayoutObjectFilter(const LayoutObject* layout_object)
      : layout_object_(layout_object) {
    DCHECK(layout_object);
  };
  bool IsCollectible(const NGPaintFragment* fragment) const {
    return fragment->GetLayoutObject() == layout_object_;
  }
  bool IsTraverse(const NGPaintFragment*) const { return true; }

 private:
  const LayoutObject* layout_object_;
};

}  // namespace

Vector<NGPaintFragmentWithContainerOffset>
NGPaintFragmentTraversal::DescendantsOf(const NGPaintFragment& container) {
  Vector<NGPaintFragmentWithContainerOffset> result;
  NotSelfPaintingFilter filter;
  CollectPaintFragments(container, NGPhysicalOffset(), filter, &result);
  return result;
}

Vector<NGPaintFragmentWithContainerOffset>
NGPaintFragmentTraversal::SelfFragmentsOf(const NGPaintFragment& container,
                                          const LayoutObject* target) {
  Vector<NGPaintFragmentWithContainerOffset> result;
  LayoutObjectFilter filter(target);
  CollectPaintFragments(container, NGPhysicalOffset(), filter, &result);
  return result;
}

}  // namespace blink
