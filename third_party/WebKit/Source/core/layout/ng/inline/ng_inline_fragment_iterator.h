// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineFragmentIterator_h
#define NGInlineFragmentIterator_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_physical_offset.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"

namespace blink {

class LayoutObject;
class NGPhysicalBoxFragment;
class NGPhysicalContainerFragment;
class NGPhysicalFragment;
struct NGPhysicalOffset;

// Iterate through inline descendant fragments.
class CORE_EXPORT NGInlineFragmentIterator {
  STACK_ALLOCATED();

 public:
  // Create an iterator that returns inline fragments produced from the
  // specified LayoutObject.
  NGInlineFragmentIterator(const NGPhysicalBoxFragment&,
                           const LayoutObject* filter);

  // The data struct the iterator returns.
  struct Result {
    const NGPhysicalFragment* fragment;
    NGPhysicalOffset offset_to_container_box;
  };
  using Results = Vector<Result, 1>;
  using LayoutObjectMap = HashMap<const LayoutObject*, Results>;

  Results::const_iterator begin() const { return results_.begin(); }
  Results::const_iterator end() const { return results_.end(); }

 private:
  static void CollectInlineFragments(const NGPhysicalContainerFragment&,
                                     NGPhysicalOffset offset_to_container_box,
                                     const LayoutObject* filter,
                                     Results*);

  Results results_;
};

}  // namespace blink

#endif  // NGInlineFragmentIterator_h
