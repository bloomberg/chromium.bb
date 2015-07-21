/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/svg/graphics/filters/SVGFilter.h"

namespace blink {

SVGFilter::SVGFilter(const IntRect& absoluteSourceDrawingRegion, const FloatRect& targetBoundingBox, const FloatRect& filterRegion, bool effectBBoxMode)
    : Filter(targetBoundingBox, filterRegion, 1.0f)
    , m_absoluteSourceDrawingRegion(absoluteSourceDrawingRegion)
    , m_effectBBoxMode(effectBBoxMode)
{
}

float SVGFilter::applyHorizontalScale(float value) const
{
    if (m_effectBBoxMode)
        value *= targetBoundingBox().width();
    return Filter::applyHorizontalScale(value);
}

float SVGFilter::applyVerticalScale(float value) const
{
    if (m_effectBBoxMode)
        value *= targetBoundingBox().height();
    return Filter::applyVerticalScale(value);
}

FloatPoint3D SVGFilter::resolve3dPoint(const FloatPoint3D& point) const
{
    if (!m_effectBBoxMode)
        return point;
    return FloatPoint3D(point.x() * targetBoundingBox().width() + targetBoundingBox().x(),
        point.y() * targetBoundingBox().height() + targetBoundingBox().y(),
        point.z() * sqrtf(targetBoundingBox().size().diagonalLengthSquared() / 2));
}

PassRefPtrWillBeRawPtr<SVGFilter> SVGFilter::create(const IntRect& absoluteSourceDrawingRegion, const FloatRect& targetBoundingBox, const FloatRect& filterRegion, bool effectBBoxMode)
{
    return adoptRefWillBeNoop(new SVGFilter(absoluteSourceDrawingRegion, targetBoundingBox, filterRegion, effectBBoxMode));
}

} // namespace blink
