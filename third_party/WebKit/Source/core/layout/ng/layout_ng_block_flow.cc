// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/layout_ng_block_flow.h"

namespace blink {

LayoutNGBlockFlow::LayoutNGBlockFlow(Element* element)
    : LayoutBlockFlow(element) {}

bool LayoutNGBlockFlow::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectNGBlockFlow || LayoutBlockFlow::isOfType(type);
}

}  // namespace blink
