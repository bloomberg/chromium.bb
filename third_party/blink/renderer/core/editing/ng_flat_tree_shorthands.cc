// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ng_flat_tree_shorthands.h"

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

namespace blink {

const LayoutBlockFlow* NGInlineFormattingContextOf(
    const PositionInFlatTree& position) {
  return NGInlineFormattingContextOf(ToPositionInDOMTree(position));
}

LocalCaretRect ComputeNGLocalCaretRect(
    const LayoutBlockFlow& context,
    const PositionInFlatTreeWithAffinity& position) {
  const PositionWithAffinity dom_position(
      ToPositionInDOMTree(position.GetPosition()), position.Affinity());
  return ComputeNGLocalCaretRect(context, dom_position);
}

}  // namespace blink
