// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebDisplayItemClipTree.h"

#include "platform/graphics/paint/DisplayItemClipTree.h"

namespace blink {

WebDisplayItemClipTree::WebDisplayItemClipTree()
{
}

WebDisplayItemClipTree::WebDisplayItemClipTree(const PassOwnPtr<DisplayItemClipTree>& passImpl)
    : m_private(passImpl)
{
}

WebDisplayItemClipTree::~WebDisplayItemClipTree()
{
    // WebPrivateOwnPtr requires explicit clearing here.
    m_private.reset(nullptr);
}

size_t WebDisplayItemClipTree::nodeCount() const
{
    return m_private->nodeCount();
}

const WebDisplayItemClipTree::ClipNode& WebDisplayItemClipTree::nodeAt(size_t index) const
{
    return m_private->nodeAt(index);
}

} // namespace blink
