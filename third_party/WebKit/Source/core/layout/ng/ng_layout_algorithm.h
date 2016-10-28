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
class NGPhysicalFragment;

// Base class for all LayoutNG algorithms.
class CORE_EXPORT NGLayoutAlgorithm
    : public GarbageCollectedFinalized<NGLayoutAlgorithm> {
  WTF_MAKE_NONCOPYABLE(NGLayoutAlgorithm);

 public:
  NGLayoutAlgorithm() {}
  virtual ~NGLayoutAlgorithm() {}

  // Actual layout function. Lays out the children and descendents within the
  // constraints given by the NGConstraintSpace. Returns a fragment with the
  // resulting layout information.
  // This function can not be const because for interruptible layout, we have
  // to be able to store state information.
  // Returns true when done; when this function returns false, it has to be
  // called again. The out parameter will only be set when this function
  // returns true.
  virtual bool Layout(NGPhysicalFragment**) = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif  // NGLayoutAlgorithm_h
