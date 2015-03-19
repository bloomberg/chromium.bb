/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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
#include "core/svg/SVGPathBuilder.h"

#include "platform/graphics/Path.h"

namespace blink {

void SVGPathBuilder::moveTo(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    ASSERT(mode == AbsoluteCoordinates);
    if (m_closed && !m_path.isEmpty())
        closePath();
    m_path.moveTo(targetPoint);
    m_closed = false;
}

void SVGPathBuilder::lineTo(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    ASSERT(mode == AbsoluteCoordinates);
    m_path.addLineTo(targetPoint);
}

void SVGPathBuilder::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    ASSERT(mode == AbsoluteCoordinates);
    m_path.addBezierCurveTo(point1, point2, targetPoint);
}

void SVGPathBuilder::closePath()
{
    m_path.closeSubpath();
    m_closed = true;
}

}
