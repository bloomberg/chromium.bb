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
#include "core/rendering/svg/RenderSVGResourcePaintServer.h"

#include "core/rendering/style/RenderStyle.h"
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

static SVGPaintDescription requestPaint(const RenderObject& object, const RenderStyle* style, RenderSVGResourceMode mode)
{
    ASSERT(style);

    // If we have no style at all, ignore it.
    const SVGRenderStyle& svgStyle = style->svgStyle();

    // If we have no fill/stroke, return 0.
    if (mode == ApplyToFillMode) {
        if (!svgStyle.hasFill())
            return SVGPaintDescription();
    } else {
        if (!svgStyle.hasStroke())
            return SVGPaintDescription();
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
    if (paintType < SVG_PAINTTYPE_URI_NONE) {
        // |paintType| will be either <current-color> or <rgb-color> here - both of which will have a color.
        ASSERT(hasColor);
        return SVGPaintDescription(color);
    }

    RenderSVGResourcePaintServer* uriResource = 0;
    if (SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(&object))
        uriResource = applyToFill ? resources->fill() : resources->stroke();

    // If the requested resource is not available, return the color resource or 'none'.
    if (!uriResource) {
        // The fallback is 'none'. (SVG2 say 'none' is implied when no fallback is specified.)
        if (paintType == SVG_PAINTTYPE_URI_NONE || !hasColor)
            return SVGPaintDescription();

        return SVGPaintDescription(color);
    }

    // The paint server resource exists, though it may be invalid (pattern with width/height=0).
    // Return the fallback color to our caller so it can use it, if
    // preparePaintServer() on the resource container failed.
    if (hasColor)
        return SVGPaintDescription(uriResource, color);

    return SVGPaintDescription(uriResource);
}

SVGPaintServer SVGPaintServer::requestForRenderer(const RenderObject& renderer, const RenderStyle* style, RenderSVGResourceMode resourceMode)
{
    ASSERT(style);
    ASSERT(resourceMode == ApplyToFillMode || resourceMode == ApplyToStrokeMode);

    SVGPaintDescription paintDescription = requestPaint(renderer, style, resourceMode);
    if (!paintDescription.isValid)
        return invalid();
    if (!paintDescription.resource)
        return SVGPaintServer(paintDescription.color);
    SVGPaintServer paintServer = paintDescription.resource->preparePaintServer(renderer);
    if (paintServer.isValid())
        return paintServer;
    if (paintDescription.hasFallback)
        return SVGPaintServer(paintDescription.color);
    return invalid();
}

bool SVGPaintServer::existsForRenderer(const RenderObject& renderer, const RenderStyle* style, RenderSVGResourceMode resourceMode)
{
    return requestPaint(renderer, style, resourceMode).isValid;
}

RenderSVGResourcePaintServer::RenderSVGResourcePaintServer(SVGElement* element)
    : RenderSVGResourceContainer(element)
{
}

RenderSVGResourcePaintServer::~RenderSVGResourcePaintServer()
{
}

SVGPaintDescription RenderSVGResourcePaintServer::requestPaintDescription(const RenderObject& renderer, const RenderStyle* style, RenderSVGResourceMode resourceMode)
{
    return requestPaint(renderer, style, resourceMode);
}

}
