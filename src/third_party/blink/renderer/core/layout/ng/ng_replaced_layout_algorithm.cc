// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_replaced_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"

namespace blink {

NGReplacedLayoutAlgorithm::NGReplacedLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {}

scoped_refptr<const NGLayoutResult> NGReplacedLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken() || BreakToken()->IsBreakBefore());
  // Set this as a legacy root so that legacy painters are used.
  container_builder_.SetIsLegacyLayoutRoot();

  const LayoutUnit intrinsic_block_size =
      ComputeReplacedSize(Node(), ConstraintSpace(), BorderPadding(),
                          ReplacedSizeMode::kIgnoreBlockLengths)
          .block_size;
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGReplacedLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  // Most layouts are interested in the min/max content contribution which will
  // call |ComputeReplacedSize| directly. (Which doesn't invoke the code below).
  // This is only used by flex, which expects inline-lengths to be ignored for
  // the min/max content size.
  MinMaxSizes sizes;
  sizes = ComputeReplacedSize(Node(), ConstraintSpace(), BorderPadding(),
                              ReplacedSizeMode::kIgnoreInlineLengths)
              .inline_size;
  return {sizes, /* depends_on_block_constraints */ false};
}

}  // namespace blink
