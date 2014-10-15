/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
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

#include "config.h"
#include "core/rendering/svg/RenderSVGResource.h"

#include "core/rendering/svg/RenderSVGResourceClipper.h"
#include "core/rendering/svg/RenderSVGResourceFilter.h"
#include "core/rendering/svg/RenderSVGResourceMasker.h"
#include "core/rendering/svg/RenderSVGResourceSolidColor.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/rendering/svg/SVGResources.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

SVGPaintServer::SVGPaintServer(Color color)
    : m_color(color)
{
}

SVGPaintServer::SVGPaintServer(PassRefPtr<Gradient> gradient)
    : m_gradient(gradient)
    , m_color(Color::black)
{
}

SVGPaintServer::SVGPaintServer(PassRefPtr<Pattern> pattern)
    : m_pattern(pattern)
    , m_color(Color::black)
{
}

void SVGPaintServer::apply(GraphicsContext& context, RenderSVGResourceMode resourceMode, GraphicsContextStateSaver* stateSaver)
{
    ASSERT(resourceMode == ApplyToFillMode || resourceMode == ApplyToStrokeMode);
    if (stateSaver && (m_gradient || m_pattern))
        stateSaver->saveIfNeeded();

    if (resourceMode == ApplyToFillMode) {
        if (m_pattern)
            context.setFillPattern(m_pattern);
        else if (m_gradient)
            context.setFillGradient(m_gradient);
        else
            context.setFillColor(m_color);
    } else {
        if (m_pattern)
            context.setStrokePattern(m_pattern);
        else if (m_gradient)
            context.setStrokeGradient(m_gradient);
        else
            context.setStrokeColor(m_color);
    }
}

void SVGPaintServer::prependTransform(const AffineTransform& transform)
{
    ASSERT(m_gradient || m_pattern);
    if (m_pattern)
        m_pattern->setPatternSpaceTransform(transform * m_pattern->patternSpaceTransform());
    else
        m_gradient->setGradientSpaceTransform(transform * m_gradient->gradientSpaceTransform());
}

SVGPaintServer SVGPaintServer::requestForRenderer(RenderObject& renderer, RenderStyle* style, RenderSVGResourceMode resourceMode)
{
    ASSERT(style);
    ASSERT(resourceMode == ApplyToFillMode || resourceMode == ApplyToStrokeMode);

    bool hasFallback = false;
    RenderSVGResource* paintingResource = RenderSVGResource::requestPaintingResource(resourceMode, &renderer, style, hasFallback);
    if (!paintingResource)
        return invalid();

    SVGPaintServer paintServer = paintingResource->preparePaintServer(&renderer);
    if (paintServer.isValid())
        return paintServer;
    if (hasFallback)
        return SVGPaintServer(RenderSVGResource::sharedSolidPaintingResource()->color());
    return invalid();
}

SVGPaintServer RenderSVGResource::preparePaintServer(RenderObject*)
{
    ASSERT_NOT_REACHED();
    return SVGPaintServer::invalid();
}

RenderSVGResource* RenderSVGResource::requestPaintingResource(RenderSVGResourceMode mode, RenderObject* object, const RenderStyle* style, bool& hasFallback)
{
    ASSERT(object);
    ASSERT(style);

    hasFallback = false;

    // If we have no style at all, ignore it.
    const SVGRenderStyle& svgStyle = style->svgStyle();

    // If we have no fill/stroke, return 0.
    if (mode == ApplyToFillMode) {
        if (!svgStyle.hasFill())
            return 0;
    } else {
        if (!svgStyle.hasStroke())
            return 0;
    }

    bool applyToFill = mode == ApplyToFillMode;
    SVGPaintType paintType = applyToFill ? svgStyle.fillPaintType() : svgStyle.strokePaintType();
    ASSERT(paintType != SVG_PAINTTYPE_NONE);

    Color color;
    bool hasColor = false;
    switch (paintType) {
    case SVG_PAINTTYPE_CURRENTCOLOR:
    case SVG_PAINTTYPE_RGBCOLOR:
    case SVG_PAINTTYPE_URI_CURRENTCOLOR:
    case SVG_PAINTTYPE_URI_RGBCOLOR:
        color = applyToFill ? svgStyle.fillPaintColor() : svgStyle.strokePaintColor();
        hasColor = true;
    default:
        break;
    }

    if (style->insideLink() == InsideVisitedLink) {
        // FIXME: This code doesn't support the uri component of the visited link paint, https://bugs.webkit.org/show_bug.cgi?id=70006
        SVGPaintType visitedPaintType = applyToFill ? svgStyle.visitedLinkFillPaintType() : svgStyle.visitedLinkStrokePaintType();

        // For SVG_PAINTTYPE_CURRENTCOLOR, 'color' already contains the 'visitedColor'.
        if (visitedPaintType < SVG_PAINTTYPE_URI_NONE && visitedPaintType != SVG_PAINTTYPE_CURRENTCOLOR) {
            const Color& visitedColor = applyToFill ? svgStyle.visitedLinkFillPaintColor() : svgStyle.visitedLinkStrokePaintColor();
            color = Color(visitedColor.red(), visitedColor.green(), visitedColor.blue(), color.alpha());
            hasColor = true;
        }
    }

    // If the primary resource is just a color, return immediately.
    RenderSVGResourceSolidColor* colorResource = RenderSVGResource::sharedSolidPaintingResource();
    if (paintType < SVG_PAINTTYPE_URI_NONE) {
        // |paintType| will be either <current-color> or <rgb-color> here - both of which will have a color.
        ASSERT(hasColor);
        colorResource->setColor(color);
        return colorResource;
    }

    RenderSVGResource* uriResource = 0;
    if (SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(object))
        uriResource = applyToFill ? resources->fill() : resources->stroke();

    // If the requested resource is not available, return the color resource or 'none'.
    if (!uriResource) {
        // The fallback is 'none'. (SVG2 say 'none' is implied when no fallback is specified.)
        if (paintType == SVG_PAINTTYPE_URI_NONE || !hasColor)
            return 0;

        colorResource->setColor(color);
        return colorResource;
    }

    // The paint server resource exists, though it may be invalid (pattern with width/height=0). Pass the fallback color to our caller
    // via sharedSolidPaintingResource so it can use the solid color painting resource, if applyResource() on the URI resource failed.
    if (hasColor) {
        colorResource->setColor(color);
        hasFallback = true;
    }
    return uriResource;
}

RenderSVGResourceSolidColor* RenderSVGResource::sharedSolidPaintingResource()
{
    static RenderSVGResourceSolidColor* s_sharedSolidPaintingResource = 0;
    if (!s_sharedSolidPaintingResource)
        s_sharedSolidPaintingResource = new RenderSVGResourceSolidColor;
    return s_sharedSolidPaintingResource;
}

static inline void removeFromCacheAndInvalidateDependencies(RenderObject* object, bool needsLayout)
{
    ASSERT(object);
    if (SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(object)) {
        if (RenderSVGResourceFilter* filter = resources->filter())
            filter->removeClientFromCache(object);

        if (RenderSVGResourceMasker* masker = resources->masker())
            masker->removeClientFromCache(object);

        if (RenderSVGResourceClipper* clipper = resources->clipper())
            clipper->removeClientFromCache(object);
    }

    if (!object->node() || !object->node()->isSVGElement())
        return;
    SVGElementSet* dependencies = toSVGElement(object->node())->setOfIncomingReferences();
    if (!dependencies)
        return;

    // We allow cycles in SVGDocumentExtensions reference sets in order to avoid expensive
    // reference graph adjustments on changes, so we need to break possible cycles here.
    // This strong reference is safe, as it is guaranteed that this set will be emptied
    // at the end of recursion.
    typedef WillBeHeapHashSet<RawPtrWillBeMember<SVGElement> > SVGElementSet;
    DEFINE_STATIC_LOCAL(OwnPtrWillBePersistent<SVGElementSet>, invalidatingDependencies, (adoptPtrWillBeNoop(new SVGElementSet)));

    SVGElementSet::iterator end = dependencies->end();
    for (SVGElementSet::iterator it = dependencies->begin(); it != end; ++it) {
        if (RenderObject* renderer = (*it)->renderer()) {
            if (UNLIKELY(!invalidatingDependencies->add(*it).isNewEntry)) {
                // Reference cycle: we are in process of invalidating this dependant.
                continue;
            }

            RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer, needsLayout);
            invalidatingDependencies->remove(*it);
        }
    }
}

void RenderSVGResource::markForLayoutAndParentResourceInvalidation(RenderObject* object, bool needsLayout)
{
    ASSERT(object);
    ASSERT(object->node());

    if (needsLayout && !object->documentBeingDestroyed())
        object->setNeedsLayoutAndFullPaintInvalidation();

    removeFromCacheAndInvalidateDependencies(object, needsLayout);

    // Invalidate resources in ancestor chain, if needed.
    RenderObject* current = object->parent();
    while (current) {
        removeFromCacheAndInvalidateDependencies(current, needsLayout);

        if (current->isSVGResourceContainer()) {
            // This will process the rest of the ancestors.
            toRenderSVGResourceContainer(current)->removeAllClientsFromCache();
            break;
        }

        current = current->parent();
    }
}

}
