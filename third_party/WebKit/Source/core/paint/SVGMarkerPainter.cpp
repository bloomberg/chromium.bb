// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGMarkerPainter.h"

#include "core/paint/SVGContainerPainter.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/svg/RenderSVGResourceMarker.h"
#include "core/rendering/svg/SVGMarkerData.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

void SVGMarkerPainter::paint(PaintInfo& paintInfo, const MarkerPosition& position, float strokeWidth)
{
    // An empty viewBox disables rendering.
    SVGMarkerElement* marker = toSVGMarkerElement(m_renderSVGMarker.element());
    ASSERT(marker);
    if (marker->hasAttribute(SVGNames::viewBoxAttr) && marker->viewBox()->currentValue()->isValid() && marker->viewBox()->currentValue()->value().isEmpty())
        return;

    PaintInfo info(paintInfo);
    GraphicsContextStateSaver stateSaver(*info.context, false);
    info.applyTransform(m_renderSVGMarker.markerTransformation(position.origin, position.angle, strokeWidth), &stateSaver);

    if (SVGRenderSupport::isOverflowHidden(&m_renderSVGMarker)) {
        stateSaver.saveIfNeeded();
        info.context->clip(m_renderSVGMarker.viewport());
    }

    SVGContainerPainter(m_renderSVGMarker).paint(info);
}

} // namespace blink
