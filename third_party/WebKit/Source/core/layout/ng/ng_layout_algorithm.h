// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutAlgorithm_h
#define NGLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class NGConstraintSpace;
class NGFragment;

// Base class for all LayoutNG algorithms.
class CORE_EXPORT NGLayoutAlgorithm {
  WTF_MAKE_NONCOPYABLE(NGLayoutAlgorithm);
  USING_FAST_MALLOC(NGLayoutAlgorithm);

 public:
  NGLayoutAlgorithm() {}

  // Actual layout function. Lays out the children and descendents within the
  // constraints given by the NGConstraintSpace. Returns a fragment with the
  // resulting layout information.
  // This function can not be const because for interruptible layout, we have
  // to be able to store state information.
  virtual NGFragment* layout(const NGConstraintSpace*) = 0;
};

}  // namespace blink

#endif  // NGLayoutAlgorithm_h
