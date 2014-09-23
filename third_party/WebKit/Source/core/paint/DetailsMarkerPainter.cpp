// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/DetailsMarkerPainter.h"

#include "core/paint/BlockPainter.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderDetailsMarker.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/Path.h"

namespace blink {

void DetailsMarkerPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhaseForeground || m_renderDetailsMarker.style()->visibility() != VISIBLE) {
        BlockPainter(m_renderDetailsMarker).paint(paintInfo, paintOffset);
        return;
    }

    LayoutPoint boxOrigin(paintOffset + m_renderDetailsMarker.location());
    LayoutRect overflowRect(m_renderDetailsMarker.visualOverflowRect());
    overflowRect.moveBy(boxOrigin);

    if (!paintInfo.rect.intersects(pixelSnappedIntRect(overflowRect)))
        return;

    const Color color(m_renderDetailsMarker.resolveColor(CSSPropertyColor));
    paintInfo.context->setStrokeColor(color);
    paintInfo.context->setStrokeStyle(SolidStroke);
    paintInfo.context->setStrokeThickness(1.0f);
    paintInfo.context->setFillColor(color);

    boxOrigin.move(m_renderDetailsMarker.borderLeft() + m_renderDetailsMarker.paddingLeft(), m_renderDetailsMarker.borderTop() + m_renderDetailsMarker.paddingTop());
    paintInfo.context->fillPath(getPath(boxOrigin));
}

static Path createPath(const FloatPoint* path)
{
    Path result;
    result.moveTo(FloatPoint(path[0].x(), path[0].y()));
    for (int i = 1; i < 4; ++i)
        result.addLineTo(FloatPoint(path[i].x(), path[i].y()));
    return result;
}

static Path createDownArrowPath()
{
    FloatPoint points[4] = { FloatPoint(0.0f, 0.07f), FloatPoint(0.5f, 0.93f), FloatPoint(1.0f, 0.07f), FloatPoint(0.0f, 0.07f) };
    return createPath(points);
}

static Path createUpArrowPath()
{
    FloatPoint points[4] = { FloatPoint(0.0f, 0.93f), FloatPoint(0.5f, 0.07f), FloatPoint(1.0f, 0.93f), FloatPoint(0.0f, 0.93f) };
    return createPath(points);
}

static Path createLeftArrowPath()
{
    FloatPoint points[4] = { FloatPoint(1.0f, 0.0f), FloatPoint(0.14f, 0.5f), FloatPoint(1.0f, 1.0f), FloatPoint(1.0f, 0.0f) };
    return createPath(points);
}

static Path createRightArrowPath()
{
    FloatPoint points[4] = { FloatPoint(0.0f, 0.0f), FloatPoint(0.86f, 0.5f), FloatPoint(0.0f, 1.0f), FloatPoint(0.0f, 0.0f) };
    return createPath(points);
}

Path DetailsMarkerPainter::getCanonicalPath() const
{
    switch (m_renderDetailsMarker.orientation()) {
    case RenderDetailsMarker::Left: return createLeftArrowPath();
    case RenderDetailsMarker::Right: return createRightArrowPath();
    case RenderDetailsMarker::Up: return createUpArrowPath();
    case RenderDetailsMarker::Down: return createDownArrowPath();
    }

    return Path();
}

Path DetailsMarkerPainter::getPath(const LayoutPoint& origin) const
{
    Path result = getCanonicalPath();
    result.transform(AffineTransform().scale(m_renderDetailsMarker.contentWidth().toFloat(), m_renderDetailsMarker.contentHeight().toFloat()));
    result.translate(FloatSize(origin.x().toFloat(), origin.y().toFloat()));
    return result;
}

} // namespace paint
