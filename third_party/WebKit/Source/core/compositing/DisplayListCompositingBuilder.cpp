// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/compositing/DisplayListCompositingBuilder.h"

#include "platform/graphics/paint/DisplayItemTransformTreeBuilder.h"

namespace blink {

void DisplayListCompositingBuilder::build(CompositedDisplayList& compositedDisplayList)
{
    // TODO(pdr): Properly implement simple layer compositing here.
    // See: https://docs.google.com/document/d/1qF7wpO_lhuxUO6YXKZ3CJuXi0grcb5gKZJBBgnoTd0k/view

    DisplayItemTransformTreeBuilder transformTreeBuilder;
    for (const auto& displayItem : m_displayItemList.displayItems())
        transformTreeBuilder.processDisplayItem(displayItem);
    compositedDisplayList.transformTree = transformTreeBuilder.releaseTransformTree();
}

} // namespace blink
