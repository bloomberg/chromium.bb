// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_column_layout_algorithm.h"

namespace blink {

NGColumnLayoutAlgorithm::NGColumnLayoutAlgorithm(NGBlockNode node,
                                                 NGConstraintSpace* space,
                                                 NGBreakToken* break_token)
    : NGBlockLayoutAlgorithm(node, space, ToNGBlockBreakToken(break_token)){};

RefPtr<NGLayoutResult> NGColumnLayoutAlgorithm::Layout() {
  LOG(FATAL) << "Not implemented";
  return nullptr;
}

}  // namespace Blink
