// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintInfo.h"

namespace blink {

void PaintInfo::updatePaintingRootForChildren(const LayoutObject* layoutObject)
{
    if (!paintingRoot)
        return;

    // If we're the painting root, kids draw normally, and see root of nullptr.
    if (paintingRoot == layoutObject) {
        paintingRoot = nullptr;
        return;
    }
}

bool PaintInfo::shouldPaintWithinRoot(const LayoutObject* layoutObject) const
{
    return !paintingRoot || paintingRoot == layoutObject;
}

void PaintInfo::updateCullRect(const AffineTransform& localToParentTransform)
{
    m_cullRect.updateCullRect(localToParentTransform);
}

} // namespace blink
