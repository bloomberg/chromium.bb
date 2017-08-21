// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGColumnLayoutAlgorithm_h
#define NGColumnLayoutAlgorithm_h

#include "core/layout/ng/ng_block_layout_algorithm.h"

namespace blink {

class NGBlockNode;
class NGBreakToken;
class NGConstraintSpace;

class CORE_EXPORT NGColumnLayoutAlgorithm : public NGBlockLayoutAlgorithm {
 public:
  NGColumnLayoutAlgorithm(NGBlockNode node,
                          NGConstraintSpace* space,
                          NGBreakToken* break_token = nullptr);

  RefPtr<NGLayoutResult> Layout() override;
};

}  // namespace Blink

#endif  // NGColumnLayoutAlgorithm_h
