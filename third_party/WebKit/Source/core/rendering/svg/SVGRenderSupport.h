/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#ifndef SVGRenderSupport_h
#define SVGRenderSupport_h

#include "core/rendering/PaintInfo.h"

namespace WebCore {

class FloatPoint;
class FloatRect;
class ImageBuffer;
class LayoutRect;
class RenderBoxModelObject;
class RenderGeometryMap;
class RenderLayerModelObject;
class RenderObject;
class RenderStyle;
class RenderSVGRoot;
class TransformState;

namespace SVGRenderSupport {
// Shares child layouting code between RenderSVGRoot/RenderSVG(Hidden)Container
void layoutChildren(RenderObject*, bool selfNeedsLayout);

// Layout resources used by this node.
void layoutResourcesIfNeeded(const RenderObject*);

// Helper function determining wheter overflow is hidden
bool isOverflowHidden(const RenderObject*);

// Calculates the repaintRect in combination with filter, clipper and masker in local coordinates.
void intersectRepaintRectWithResources(const RenderObject*, FloatRect&);

// Determines whether a container needs to be laid out because it's filtered and a child is being laid out.
bool filtersForceContainerLayout(RenderObject*);

// Determines whether the passed point lies in a clipping area
bool pointInClippingArea(RenderObject*, const FloatPoint&);

void computeContainerBoundingBoxes(const RenderObject* container, FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, FloatRect& strokeBoundingBox, FloatRect& repaintBoundingBox);
bool paintInfoIntersectsRepaintRect(const FloatRect& localRepaintRect, const AffineTransform& localTransform, const PaintInfo&);

// Important functions used by nearly all SVG renderers centralizing coordinate transformations / repaint rect calculations
LayoutRect clippedOverflowRectForRepaint(const RenderObject*, const RenderLayerModelObject* repaintContainer);
void computeFloatRectForRepaint(const RenderObject*, const RenderLayerModelObject* repaintContainer, FloatRect&, bool fixed);
void mapLocalToContainer(const RenderObject*, const RenderLayerModelObject* repaintContainer, TransformState&, bool* wasFixed = 0);
const RenderObject* pushMappingToContainer(const RenderObject*, const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&);
bool checkForSVGRepaintDuringLayout(RenderObject*);

// Shared between SVG renderers and resources.
void applyStrokeStyleToContext(GraphicsContext*, const RenderStyle*, const RenderObject*);
void applyStrokeStyleToStrokeData(StrokeData*, const RenderStyle*, const RenderObject*);

// Determines if any ancestor's transform has changed.
bool transformToRootChanged(RenderObject*);

// FIXME: These methods do not belong here.
const RenderSVGRoot* findTreeRootObject(const RenderObject*);

// Helper method for determining if a RenderObject marked as text (isText()== true)
// can/will be rendered as part of a <text>.
bool isRenderableTextNode(const RenderObject*);

} // namespace SVGRenderSupport

} // namespace WebCore

#endif // SVGRenderSupport_h
