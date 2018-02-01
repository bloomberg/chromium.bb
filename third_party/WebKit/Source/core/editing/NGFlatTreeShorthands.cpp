// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/NGFlatTreeShorthands.h"

#include "core/editing/LocalCaretRect.h"
#include "core/editing/Position.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/layout/ng/inline/ng_caret_rect.h"
#include "core/layout/ng/inline/ng_offset_mapping.h"

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
