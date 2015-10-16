// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
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

bool PaintInfo::intersectsCullRect(const IntRect& rectArg) const
{
    return rectArg.intersects(rect);
}

bool PaintInfo::intersectsCullRect(const LayoutRect& rectArg) const
{
    return rect.intersects(enclosingIntRect(rectArg));
}

bool PaintInfo::intersectsCullRect(const AffineTransform& transform, const FloatRect& boundingBox) const
{
    return transform.mapRect(boundingBox).intersects(rect);
}

void PaintInfo::updateCullRectForSVGTransform(const AffineTransform& localToParentTransform)
{
    if (rect != LayoutRect::infiniteIntRect())
        rect = localToParentTransform.inverse().mapRect(rect);
}

} // namespace blink
