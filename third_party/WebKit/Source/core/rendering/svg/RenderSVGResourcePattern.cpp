/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "core/rendering/svg/RenderSVGResourcePattern.h"

#include "core/dom/ElementTraversal.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/svg/SVGFitToViewBox.h"
#include "core/svg/SVGPatternElement.h"
#include "platform/graphics/DisplayList.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

struct PatternData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RefPtr<Pattern> pattern;
    AffineTransform transform;
};

RenderSVGResourcePattern::RenderSVGResourcePattern(SVGPatternElement* node)
    : RenderSVGResourcePaintServer(node)
    , m_shouldCollectPatternAttributes(true)
{
}

void RenderSVGResourcePattern::removeAllClientsFromCache(bool markForInvalidation)
{
    m_patternMap.clear();
    m_shouldCollectPatternAttributes = true;
    markAllClientsForInvalidation(markForInvalidation ? PaintInvalidation : ParentOnlyInvalidation);
}

void RenderSVGResourcePattern::removeClientFromCache(RenderObject* client, bool markForInvalidation)
{
    ASSERT(client);
    m_patternMap.remove(client);
    markClientForInvalidation(client, markForInvalidation ? PaintInvalidation : ParentOnlyInvalidation);
}

PatternData* RenderSVGResourcePattern::patternForRenderer(const RenderObject& object)
{
    ASSERT(!m_shouldCollectPatternAttributes);

    // FIXME: the double hash lookup is needed to guard against paint-time invalidation
    // (painting animated images may trigger layout invals which delete our map entry).
    // Hopefully that will be addressed at some point, and then we can optimize the lookup.
    if (PatternData* currentData = m_patternMap.get(&object))
        return currentData;

    return m_patternMap.set(&object, buildPatternData(object)).storedValue->value.get();
}

PassOwnPtr<PatternData> RenderSVGResourcePattern::buildPatternData(const RenderObject& object)
{
    // If we couldn't determine the pattern content element root, stop here.
    if (!m_attributes.patternContentElement())
        return nullptr;

    // An empty viewBox disables rendering.
    if (m_attributes.hasViewBox() && m_attributes.viewBox().isEmpty())
        return nullptr;

    ASSERT(element());
    // Compute tile metrics.
    FloatRect clientBoundingBox = object.objectBoundingBox();
    FloatRect tileBounds = SVGLengthContext::resolveRectangle(element(),
        m_attributes.patternUnits(), clientBoundingBox,
        m_attributes.x(), m_attributes.y(), m_attributes.width(), m_attributes.height());
    if (tileBounds.isEmpty())
        return nullptr;

    AffineTransform tileTransform;
    if (m_attributes.hasViewBox()) {
        if (m_attributes.viewBox().isEmpty())
            return nullptr;
        tileTransform = SVGFitToViewBox::viewBoxToViewTransform(m_attributes.viewBox(),
            m_attributes.preserveAspectRatio(), tileBounds.width(), tileBounds.height());
    } else {
        // A viewbox overrides patternContentUnits, per spec.
        if (m_attributes.patternContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
            tileTransform.scale(clientBoundingBox.width(), clientBoundingBox.height());
    }

    OwnPtr<PatternData> patternData = adoptPtr(new PatternData);
    patternData->pattern = Pattern::createDisplayListPattern(asDisplayList(tileBounds, tileTransform));

    // Compute pattern space transformation.
    patternData->transform.translate(tileBounds.x(), tileBounds.y());
    AffineTransform patternTransform = m_attributes.patternTransform();
    if (!patternTransform.isIdentity())
        patternData->transform = patternTransform * patternData->transform;

    return patternData.release();
}

SVGPaintServer RenderSVGResourcePattern::preparePaintServer(const RenderObject& object)
{
    clearInvalidationMask();

    SVGPatternElement* patternElement = toSVGPatternElement(element());
    if (!patternElement)
        return SVGPaintServer::invalid();

    if (m_shouldCollectPatternAttributes) {
        patternElement->synchronizeAnimatedSVGAttribute(anyQName());

        m_attributes = PatternAttributes();
        patternElement->collectPatternAttributes(m_attributes);
        m_shouldCollectPatternAttributes = false;
    }

    // Spec: When the geometry of the applicable element has no width or height and objectBoundingBox is specified,
    // then the given effect (e.g. a gradient or a filter) will be ignored.
    FloatRect objectBoundingBox = object.objectBoundingBox();
    if (m_attributes.patternUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX && objectBoundingBox.isEmpty())
        return SVGPaintServer::invalid();

    PatternData* patternData = patternForRenderer(object);
    if (!patternData)
        return SVGPaintServer::invalid();

    patternData->pattern->setPatternSpaceTransform(patternData->transform);

    return SVGPaintServer(patternData->pattern);
}

PassRefPtr<DisplayList> RenderSVGResourcePattern::asDisplayList(const FloatRect& tileBounds,
    const AffineTransform& tileTransform) const
{
    ASSERT(!m_shouldCollectPatternAttributes);

    AffineTransform contentTransform;
    if (m_attributes.patternContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        contentTransform = tileTransform;

    // Draw the content into a DisplayList.
    GraphicsContext recordingContext(nullptr);
    recordingContext.beginRecording(FloatRect(FloatPoint(), tileBounds.size()));
    recordingContext.concatCTM(tileTransform);

    ASSERT(m_attributes.patternContentElement());
    RenderSVGResourceContainer* patternRenderer =
        toRenderSVGResourceContainer(m_attributes.patternContentElement()->renderer());
    ASSERT(patternRenderer);
    ASSERT(!patternRenderer->needsLayout());

    SubtreeContentTransformScope contentTransformScope(contentTransform);
    for (RenderObject* child = patternRenderer->firstChild(); child; child = child->nextSibling())
        SVGRenderingContext::renderSubtree(&recordingContext, child);

    return recordingContext.endRecording();
}

}
