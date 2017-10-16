// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGColumnLayoutAlgorithm_h
#define NGColumnLayoutAlgorithm_h

#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_algorithm.h"

namespace blink {

class NGBlockNode;
class NGBlockBreakToken;
class NGBreakToken;
class NGConstraintSpace;

class CORE_EXPORT NGColumnLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  NGColumnLayoutAlgorithm(NGBlockNode node,
                          const NGConstraintSpace& space,
                          NGBreakToken* break_token = nullptr);

  RefPtr<NGLayoutResult> Layout() override;

  Optional<MinMaxSize> ComputeMinMaxSize() const override;

 private:
  NGLogicalSize CalculateColumnSize(const NGLogicalSize& content_box_size);
  LayoutUnit CalculateBalancedColumnBlockSize(const NGLogicalSize& column_size,
                                              int column_count);
  RefPtr<NGConstraintSpace> CreateConstraintSpaceForColumns(
      const NGLogicalSize& column_size) const;
  RefPtr<NGConstraintSpace> CreateConstaintSpaceForBalancing(
      const NGLogicalSize& column_size) const;
};

}  // namespace Blink

#endif  // NGColumnLayoutAlgorithm_h
