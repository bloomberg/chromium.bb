// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPageLayoutAlgorithm_h
#define NGPageLayoutAlgorithm_h

#include "core/layout/ng/ng_layout_algorithm.h"

namespace blink {

class NGBlockNode;
class NGBlockBreakToken;
class NGBreakToken;
class NGConstraintSpace;
struct NGLogicalSize;

class CORE_EXPORT NGPageLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode, NGBlockBreakToken> {
 public:
  NGPageLayoutAlgorithm(NGBlockNode node,
                        const NGConstraintSpace& space,
                        NGBreakToken* break_token = nullptr);

  RefPtr<NGLayoutResult> Layout() override;

  Optional<MinMaxSize> ComputeMinMaxSize() const override;

 private:
  RefPtr<NGConstraintSpace> CreateConstraintSpaceForPages(
      const NGLogicalSize& size) const;
};

}  // namespace blink

#endif  // NGPageLayoutAlgorithm_h
