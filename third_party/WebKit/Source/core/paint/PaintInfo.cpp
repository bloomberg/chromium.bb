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

bool CullRect::intersectsCullRect(const IntRect& boundingBox) const
{
    return boundingBox.intersects(m_rect);
}

bool CullRect::intersectsCullRect(const AffineTransform& transform, const FloatRect& boundingBox) const
{
    return transform.mapRect(boundingBox).intersects(m_rect);
}

bool CullRect::intersectsCullRect(const LayoutRect& rectArg) const
{
    return m_rect.intersects(enclosingIntRect(rectArg));
}

bool CullRect::intersectsHorizontalRange(LayoutUnit lo, LayoutUnit hi) const
{
    return !(lo >= m_rect.maxX() || hi <= m_rect.x());
}

bool CullRect::intersectsVerticalRange(LayoutUnit lo, LayoutUnit hi) const
{
    return !(lo >= m_rect.maxY() || hi <= m_rect.y());
}

void PaintInfo::updateCullRect(const AffineTransform& localToParentTransform)
{
    m_cullRect.updateCullRect(localToParentTransform);
}

void CullRect::updateCullRect(const AffineTransform& localToParentTransform)
{
    if (m_rect != LayoutRect::infiniteIntRect())
        m_rect = localToParentTransform.inverse().mapRect(m_rect);
}

} // namespace blink
