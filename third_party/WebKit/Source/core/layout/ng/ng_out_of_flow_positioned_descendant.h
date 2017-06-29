// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGOutOfFlowPositionedDescendant_h
#define NGOutOfFlowPositionedDescendant_h

#include "core/CoreExport.h"

#include "core/layout/ng/geometry/ng_static_position.h"
#include "core/layout/ng/ng_block_node.h"

namespace blink {

// An out-of-flow positioned-descendant is an element with the style "postion:
// absolute" or "position: fixed" which hasn't been bubbled up to its
// containing block yet, e.g. an element with "position: relative". As soon as
// a descendant reaches its containing block, it gets placed, and doesn't bubble
// up the tree.
//
// This needs its static position [1] to be placed correcting in its containing
// block.
//
// [1] https://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-width
struct CORE_EXPORT NGOutOfFlowPositionedDescendant {
  NGBlockNode node;
  NGStaticPosition static_position;
};

}  // namespace blink

#endif  // NGOutOfFlowPositionedDescendant_h
