/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef PaintInfo_h
#define PaintInfo_h

// TODO(jchaffraix): Once we unify PaintBehavior and PaintLayerFlags, we should move
// PaintLayerFlags to PaintPhase and rename it. Thus removing the need for this #include.
#include "core/paint/PaintLayerPaintingInfo.h"
#include "core/paint/PaintPhase.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/ListHashSet.h"

#include <limits>

namespace blink {

class LayoutInline;
class LayoutBoxModelObject;
class LayoutObject;

struct PaintInfo {
    ALLOW_ONLY_INLINE_ALLOCATION();
    PaintInfo(GraphicsContext* newContext, const IntRect& newRect, PaintPhase newPhase, GlobalPaintFlags globalPaintFlags, PaintLayerFlags paintFlags,
        LayoutObject* newPaintingRoot = 0, const LayoutBoxModelObject* newPaintContainer = 0)
        : context(newContext)
        , rect(newRect)
        , phase(newPhase)
        , paintingRoot(newPaintingRoot)
        , m_paintContainer(newPaintContainer)
        , m_paintFlags(paintFlags)
        , m_globalPaintFlags(globalPaintFlags)
    {
    }

    void updatePaintingRootForChildren(const LayoutObject* layoutObject)
    {
        if (!paintingRoot)
            return;

        // If we're the painting root, kids draw normally, and see root of 0.
        if (paintingRoot == layoutObject) {
            paintingRoot = 0;
            return;
        }
    }

    bool shouldPaintWithinRoot(const LayoutObject* layoutObject) const
    {
        return !paintingRoot || paintingRoot == layoutObject;
    }

    bool isRenderingClipPathAsMaskImage() const { return m_paintFlags & PaintLayerPaintingRenderingClipPathAsMask; }

    bool skipRootBackground() const { return m_paintFlags & PaintLayerPaintingSkipRootBackground; }
    bool paintRootBackgroundOnly() const { return m_paintFlags & PaintLayerPaintingRootBackgroundOnly; }

    bool isPrinting() const { return m_globalPaintFlags & GlobalPaintPrinting; }

    DisplayItem::Type displayItemTypeForClipping() const { return DisplayItem::paintPhaseToClipBoxType(phase); }

    const LayoutBoxModelObject* paintContainer() const { return m_paintContainer; }

    GlobalPaintFlags globalPaintFlags() const { return m_globalPaintFlags; }

    PaintLayerFlags paintFlags() const { return m_paintFlags; }

    bool intersectsCullRect(const AffineTransform& transform, const FloatRect& boundingBox) const
    {
        return transform.mapRect(boundingBox).intersects(rect);
    }

    void updateCullRectForSVGTransform(const AffineTransform& localToParentTransform)
    {
        if (rect != LayoutRect::infiniteIntRect())
            rect = localToParentTransform.inverse().mapRect(rect);
    }

    // FIXME: Introduce setters/getters at some point. Requires a lot of changes throughout layout/.
    GraphicsContext* context;
    IntRect rect; // dirty rect used for culling non-intersecting layoutObjects
    PaintPhase phase;
    LayoutObject* paintingRoot; // used to draw just one element and its visual kids

private:
    const LayoutBoxModelObject* m_paintContainer; // the box model object that originates the current painting

    const PaintLayerFlags m_paintFlags;
    const GlobalPaintFlags m_globalPaintFlags;
};

} // namespace blink

#endif // PaintInfo_h
