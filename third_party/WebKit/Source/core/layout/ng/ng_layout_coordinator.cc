// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_coordinator.h"

#include "core/layout/ng/ng_layout_input_node.h"
#include "core/layout/ng/ng_physical_fragment_base.h"

namespace blink {

NGLayoutCoordinator::NGLayoutCoordinator(
    NGLayoutInputNode* input_node,
    const NGConstraintSpace* constraint_space) {
  layout_algorithms_.append(
      NGLayoutInputNode::AlgorithmForInputNode(input_node, constraint_space));
}

bool NGLayoutCoordinator::Tick(NGPhysicalFragmentBase** out_fragment) {
  NGLayoutAlgorithm* child_algorithm;

  // Tick should never be called without a layout algorithm on the stack.
  DCHECK(layout_algorithms_.size());

  NGPhysicalFragmentBase* fragment;
  NGPhysicalFragmentBase* in_fragment = fragment_;
  fragment_ = nullptr;

  switch (layout_algorithms_.back()->Layout(in_fragment, &fragment,
                                            &child_algorithm)) {
    case kNotFinished:
      return false;
    case kNewFragment:
      layout_algorithms_.pop_back();
      if (layout_algorithms_.size() == 0) {
        *out_fragment = fragment;
        return true;
      }
      fragment_ = fragment;
      return false;
    case kChildAlgorithmRequired:
      layout_algorithms_.append(child_algorithm);
      return false;
  }

  NOTREACHED();
  return false;
}

DEFINE_TRACE(NGLayoutCoordinator) {
  visitor->trace(layout_algorithms_);
  visitor->trace(fragment_);
}
}
