// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockLayoutAlgorithm_h
#define NGBlockLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_box.h"
#include "core/layout/ng/ng_box_iterator.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "wtf/RefPtr.h"

namespace blink {

class ComputedStyle;
class NGConstraintSpace;
class NGFragment;

// A class for general block layout (e.g. a <div> with no special style).
// Lays out the children in sequence.
class CORE_EXPORT NGBlockLayoutAlgorithm : public NGLayoutAlgorithm {
 public:
  // Default constructor.
  // @param style Style reference of the block that is being laid out.
  // @param boxIterator Iterator for the block's children.
  NGBlockLayoutAlgorithm(PassRefPtr<const ComputedStyle>, NGBoxIterator);

  // Actual layout implementation. Lays out the children in sequence within the
  // constraints given by the NGConstraintSpace. Returns a fragment with the
  // resulting layout information.
  // This function can not be const because for interruptible layout, we have
  // to be able to store state information.
  // Returns true when done; when this function returns false, it has to be
  // called again. The out parameter will only be set when this function
  // returns true. The same constraint space has to be passed each time.
  bool Layout(const NGConstraintSpace*, NGFragment**) override;

 private:
  RefPtr<const ComputedStyle> style_;
  NGBoxIterator box_iterator_;
};

}  // namespace blink

#endif  // NGBlockLayoutAlgorithm_h
