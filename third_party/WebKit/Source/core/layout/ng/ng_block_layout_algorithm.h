// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockLayoutAlgorithm_h
#define NGBlockLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_box.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "wtf/RefPtr.h"

namespace blink {

class ComputedStyle;
class NGConstraintSpace;
class NGPhysicalFragment;

// A class for general block layout (e.g. a <div> with no special style).
// Lays out the children in sequence.
class CORE_EXPORT NGBlockLayoutAlgorithm : public NGLayoutAlgorithm {
 public:
  // Default constructor.
  // @param style Style reference of the block that is being laid out.
  // @param first_child Our first child; the algorithm will use its NextSibling
  //                    method to access all the children.
  NGBlockLayoutAlgorithm(PassRefPtr<const ComputedStyle>, NGBox* first_child);

  // Actual layout implementation. Lays out the children in sequence within the
  // constraints given by the NGConstraintSpace. Returns a fragment with the
  // resulting layout information.
  // This function can not be const because for interruptible layout, we have
  // to be able to store state information.
  // Returns true when done; when this function returns false, it has to be
  // called again. The out parameter will only be set when this function
  // returns true. The same constraint space has to be passed each time.
  bool Layout(const NGConstraintSpace*, NGPhysicalFragment**) override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    NGLayoutAlgorithm::trace(visitor);
    visitor->trace(first_child_);
    visitor->trace(builder_);
    visitor->trace(constraint_space_for_children_);
    visitor->trace(current_child_);
  }

 private:
  RefPtr<const ComputedStyle> style_;
  Member<NGBox> first_child_;

  enum State { kStateInit, kStateChildLayout, kStateFinalize };
  State state_;
  Member<NGFragmentBuilder> builder_;
  Member<NGConstraintSpace> constraint_space_for_children_;
  Member<NGBox> current_child_;
  LayoutUnit content_size_;
  LayoutUnit max_inline_size_;
};

}  // namespace blink

#endif  // NGBlockLayoutAlgorithm_h
