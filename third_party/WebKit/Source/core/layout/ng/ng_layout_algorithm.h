// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutAlgorithm_h
#define NGLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_units.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/Optional.h"

namespace blink {

struct MinAndMaxContentSizes;
class NGLayoutResult;

// Base class for all LayoutNG algorithms.
class CORE_EXPORT NGLayoutAlgorithm {
  STACK_ALLOCATED();

 public:
  virtual ~NGLayoutAlgorithm() {}

  // Actual layout function. Lays out the children and descendents within the
  // constraints given by the NGConstraintSpace. Returns a layout result with
  // the resulting layout information.
  // TODO(layout-dev): attempt to make this function const.
  virtual RefPtr<NGLayoutResult> Layout() = 0;

  // Computes the min-content and max-content intrinsic sizes for the given box.
  // The result will not take any min-width, max-width or width properties into
  // account. If the return value is empty, the caller is expected to synthesize
  // this value from the overflow rect returned from Layout called with an
  // available width of 0 and LayoutUnit::max(), respectively.
  virtual Optional<MinAndMaxContentSizes> ComputeMinAndMaxContentSizes() const {
    return WTF::nullopt;
  }
};

}  // namespace blink

#endif  // NGLayoutAlgorithm_h
