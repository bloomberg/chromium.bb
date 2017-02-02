// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutAlgorithm_h
#define NGLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

struct MinAndMaxContentSizes;
class NGPhysicalFragment;

// Base class for all LayoutNG algorithms.
class CORE_EXPORT NGLayoutAlgorithm {
  STACK_ALLOCATED();

 public:
  virtual ~NGLayoutAlgorithm() {}

  // Actual layout function. Lays out the children and descendents within the
  // constraints given by the NGConstraintSpace. Returns a fragment with the
  // resulting layout information.
  // TODO(layout-dev): attempt to make this function const.
  virtual NGPhysicalFragment* Layout() = 0;

  // Computes the min-content and max-content intrinsic sizes for the given box.
  // The result will not take any min-width, max-width or width properties into
  // account. Implementations can return false, in which case the caller is
  // expected to synthesize this value from the overflow rect returned from
  // Layout called with a available width of 0 and LayoutUnit::max(),
  // respectively.
  virtual bool ComputeMinAndMaxContentSizes(MinAndMaxContentSizes*) const {
    return false;
  }
};

}  // namespace blink

#endif  // NGLayoutAlgorithm_h
