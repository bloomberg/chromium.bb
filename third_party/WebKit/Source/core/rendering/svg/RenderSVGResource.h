/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#ifndef RenderSVGResource_h
#define RenderSVGResource_h

namespace blink {

enum RenderSVGResourceType {
    MaskerResourceType,
    MarkerResourceType,
    PatternResourceType,
    LinearGradientResourceType,
    RadialGradientResourceType,
    SolidColorResourceType,
    FilterResourceType,
    ClipperResourceType
};

// If this enum changes change the unsigned bitfields using it.
enum RenderSVGResourceMode {
    ApplyToFillMode    = 1 << 0,
    ApplyToStrokeMode  = 1 << 1,
    ApplyToTextMode    = 1 << 2 // used in combination with ApplyTo{Fill|Stroke}Mode
};
typedef unsigned RenderSVGResourceModeFlags;

class GraphicsContext;
class RenderObject;
class RenderStyle;
class RenderSVGResourceSolidColor;

class RenderSVGResource {
public:
    RenderSVGResource() { }
    virtual ~RenderSVGResource() { }

    virtual bool applyResource(RenderObject*, RenderStyle*, GraphicsContext*, RenderSVGResourceModeFlags);
    virtual void postApplyResource(GraphicsContext*) { }

    virtual RenderSVGResourceType resourceType() const = 0;

    // Helper utilities used in the render tree to access resources used for painting shapes/text (gradients & patterns & solid colors only)
    // If hasFallback gets set to true, the sharedSolidPaintingResource is set to a fallback color.
    static RenderSVGResource* requestPaintingResource(RenderSVGResourceMode, RenderObject*, const RenderStyle*, bool& hasFallback);
    static RenderSVGResourceSolidColor* sharedSolidPaintingResource();

    static void markForLayoutAndParentResourceInvalidation(RenderObject*, bool needsLayout = true);

protected:
    // Transfer fill/stroke style (except paint server) to the GC.
    static void updateGraphicsContext(GraphicsContext*, const RenderStyle*, const RenderObject&, unsigned resourceModeFlags);
};

#define DEFINE_RENDER_SVG_RESOURCE_TYPE_CASTS(thisType, typeName) \
    DEFINE_TYPE_CASTS(thisType, RenderSVGResource, resource, resource->resourceType() == typeName, resource.resourceType() == typeName)

}

#endif
