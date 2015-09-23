// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebDisplayItemTransformTree.h"

#include "platform/graphics/paint/DisplayItemTransformTree.h"

namespace blink {

WebDisplayItemTransformTree::WebDisplayItemTransformTree()
{
}

WebDisplayItemTransformTree::WebDisplayItemTransformTree(const PassOwnPtr<DisplayItemTransformTree>& passImpl)
    : m_private(passImpl)
{
}

WebDisplayItemTransformTree::~WebDisplayItemTransformTree()
{
    // WebPrivateOwnPtr requires explicit clearing here.
    m_private.reset(nullptr);
}

size_t WebDisplayItemTransformTree::nodeCount() const
{
    return m_private->nodeCount();
}

const WebDisplayItemTransformTree::TransformNode& WebDisplayItemTransformTree::nodeAt(size_t index) const
{
    return m_private->nodeAt(index);
}

} // namespace blink
