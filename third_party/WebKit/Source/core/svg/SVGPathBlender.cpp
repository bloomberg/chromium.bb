/*
 * Copyright (C) Research In Motion Limited 2010, 2011. All rights reserved.
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
#include "core/svg/SVGPathBlender.h"

#include "core/svg/SVGPathConsumer.h"
#include "core/svg/SVGPathSeg.h"
#include "core/svg/SVGPathSource.h"
#include "platform/animation/AnimationUtilities.h"
#include "wtf/TemporaryChange.h"

namespace blink {

SVGPathBlender::SVGPathBlender(SVGPathSource* fromSource, SVGPathSource* toSource, SVGPathConsumer* consumer)
    : m_fromSource(fromSource)
    , m_toSource(toSource)
    , m_consumer(consumer)
    , m_progress(0)
    , m_addTypesCount(0)
    , m_isInFirstHalfOfAnimation(false)
    , m_typesAreEqual(false)
    , m_fromIsAbsolute(false)
    , m_toIsAbsolute(false)
{
    ASSERT(m_fromSource);
    ASSERT(m_toSource);
    ASSERT(m_consumer);
}

DEFINE_TRACE(SVGPathBlender)
{
    visitor->trace(m_fromSource);
    visitor->trace(m_toSource);
    visitor->trace(m_consumer);
}

// Helper functions
static inline FloatPoint blendFloatPoint(const FloatPoint& a, const FloatPoint& b, float progress)
{
    return FloatPoint(blend(a.x(), b.x(), progress), blend(a.y(), b.y(), progress));
}

float SVGPathBlender::blendAnimatedDimensonalFloat(float from, float to, FloatBlendMode blendMode)
{
    if (m_addTypesCount) {
        ASSERT(m_typesAreEqual);
        return from + to * m_addTypesCount;
    }

    if (m_typesAreEqual)
        return blend(from, to, m_progress);

    float fromValue = blendMode == BlendHorizontal ? m_fromCurrentPoint.x() : m_fromCurrentPoint.y();
    float toValue = blendMode == BlendHorizontal ? m_toCurrentPoint.x() : m_toCurrentPoint.y();

    // Transform toY to the coordinate mode of fromY
    float animValue = blend(from, m_fromIsAbsolute ? to + toValue : to - toValue, m_progress);

    // If we're in the first half of the animation, we should use the type of the from segment.
    if (m_isInFirstHalfOfAnimation)
        return animValue;

    // Transform the animated point to the coordinate mode, needed for the current progress.
    float currentValue = blend(fromValue, toValue, m_progress);
    return !m_fromIsAbsolute ? animValue + currentValue : animValue - currentValue;
}

FloatPoint SVGPathBlender::blendAnimatedFloatPointSameCoordinates(const FloatPoint& fromPoint, const FloatPoint& toPoint)
{
    if (m_addTypesCount) {
        FloatPoint repeatedToPoint = toPoint;
        repeatedToPoint.scale(m_addTypesCount, m_addTypesCount);
        return fromPoint + repeatedToPoint;
    }
    return blendFloatPoint(fromPoint, toPoint, m_progress);
}

FloatPoint SVGPathBlender::blendAnimatedFloatPoint(const FloatPoint& fromPoint, const FloatPoint& toPoint)
{
    if (m_typesAreEqual)
        return blendAnimatedFloatPointSameCoordinates(fromPoint, toPoint);

    // Transform toPoint to the coordinate mode of fromPoint
    FloatPoint animatedPoint = toPoint;
    if (m_fromIsAbsolute)
        animatedPoint += m_toCurrentPoint;
    else
        animatedPoint.move(-m_toCurrentPoint.x(), -m_toCurrentPoint.y());

    animatedPoint = blendFloatPoint(fromPoint, animatedPoint, m_progress);

    // If we're in the first half of the animation, we should use the type of the from segment.
    if (m_isInFirstHalfOfAnimation)
        return animatedPoint;

    // Transform the animated point to the coordinate mode, needed for the current progress.
    FloatPoint currentPoint = blendFloatPoint(m_fromCurrentPoint, m_toCurrentPoint, m_progress);
    if (!m_fromIsAbsolute)
        return animatedPoint + currentPoint;

    animatedPoint.move(-currentPoint.x(), -currentPoint.y());
    return animatedPoint;
}

PathSegmentData SVGPathBlender::blendMoveToSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    PathSegmentData blendedSegment;
    blendedSegment.command = m_isInFirstHalfOfAnimation ? fromSeg.command : toSeg.command;
    blendedSegment.targetPoint = blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint);

    m_fromCurrentPoint = m_fromIsAbsolute ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toIsAbsolute ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
    return blendedSegment;
}

PathSegmentData SVGPathBlender::blendLineToSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    PathSegmentData blendedSegment;
    blendedSegment.command = m_isInFirstHalfOfAnimation ? fromSeg.command : toSeg.command;
    blendedSegment.targetPoint = blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint);

    m_fromCurrentPoint = m_fromIsAbsolute ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toIsAbsolute ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
    return blendedSegment;
}

PathSegmentData SVGPathBlender::blendLineToHorizontalSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    PathSegmentData blendedSegment;
    blendedSegment.command = m_isInFirstHalfOfAnimation ? fromSeg.command : toSeg.command;
    blendedSegment.targetPoint.setX(blendAnimatedDimensonalFloat(fromSeg.targetPoint.x(), toSeg.targetPoint.x(), BlendHorizontal));

    m_fromCurrentPoint.setX(m_fromIsAbsolute ? fromSeg.targetPoint.x() : m_fromCurrentPoint.x() + fromSeg.targetPoint.x());
    m_toCurrentPoint.setX(m_toIsAbsolute ? toSeg.targetPoint.x() : m_toCurrentPoint.x() + toSeg.targetPoint.x());
    return blendedSegment;
}

PathSegmentData SVGPathBlender::blendLineToVerticalSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    PathSegmentData blendedSegment;
    blendedSegment.command = m_isInFirstHalfOfAnimation ? fromSeg.command : toSeg.command;
    blendedSegment.targetPoint.setY(blendAnimatedDimensonalFloat(fromSeg.targetPoint.y(), toSeg.targetPoint.y(), BlendVertical));

    m_fromCurrentPoint.setY(m_fromIsAbsolute ? fromSeg.targetPoint.y() : m_fromCurrentPoint.y() + fromSeg.targetPoint.y());
    m_toCurrentPoint.setY(m_toIsAbsolute ? toSeg.targetPoint.y() : m_toCurrentPoint.y() + toSeg.targetPoint.y());
    return blendedSegment;
}

PathSegmentData SVGPathBlender::blendCurveToCubicSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    PathSegmentData blendedSegment;
    blendedSegment.command = m_isInFirstHalfOfAnimation ? fromSeg.command : toSeg.command;
    blendedSegment.targetPoint = blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint);
    blendedSegment.point1 = blendAnimatedFloatPoint(fromSeg.point1, toSeg.point1);
    blendedSegment.point2 = blendAnimatedFloatPoint(fromSeg.point2, toSeg.point2);

    m_fromCurrentPoint = m_fromIsAbsolute ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toIsAbsolute ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
    return blendedSegment;
}

PathSegmentData SVGPathBlender::blendCurveToCubicSmoothSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    PathSegmentData blendedSegment;
    blendedSegment.command = m_isInFirstHalfOfAnimation ? fromSeg.command : toSeg.command;
    blendedSegment.targetPoint = blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint);
    blendedSegment.point2 = blendAnimatedFloatPoint(fromSeg.point2, toSeg.point2);

    m_fromCurrentPoint = m_fromIsAbsolute ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toIsAbsolute ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
    return blendedSegment;
}

PathSegmentData SVGPathBlender::blendCurveToQuadraticSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    PathSegmentData blendedSegment;
    blendedSegment.command = m_isInFirstHalfOfAnimation ? fromSeg.command : toSeg.command;
    blendedSegment.targetPoint = blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint);
    blendedSegment.point1 = blendAnimatedFloatPoint(fromSeg.point1, toSeg.point1);

    m_fromCurrentPoint = m_fromIsAbsolute ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toIsAbsolute ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
    return blendedSegment;
}

PathSegmentData SVGPathBlender::blendCurveToQuadraticSmoothSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    PathSegmentData blendedSegment;
    blendedSegment.command = m_isInFirstHalfOfAnimation ? fromSeg.command : toSeg.command;
    blendedSegment.targetPoint = blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint);

    m_fromCurrentPoint = m_fromIsAbsolute ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toIsAbsolute ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
    return blendedSegment;
}

PathSegmentData SVGPathBlender::blendArcToSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    ASSERT(!m_addTypesCount || fromSeg.command == toSeg.command);

    PathSegmentData blendedSegment;
    blendedSegment.command = m_isInFirstHalfOfAnimation ? fromSeg.command : toSeg.command;
    blendedSegment.targetPoint = blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint);
    blendedSegment.point1 = blendAnimatedFloatPointSameCoordinates(fromSeg.arcRadii(), toSeg.arcRadii());
    blendedSegment.point2 = blendAnimatedFloatPointSameCoordinates(fromSeg.point2, toSeg.point2);
    if (m_addTypesCount) {
        blendedSegment.arcLarge = fromSeg.arcLarge || toSeg.arcLarge;
        blendedSegment.arcSweep = fromSeg.arcSweep || toSeg.arcSweep;
    } else {
        blendedSegment.arcLarge = m_isInFirstHalfOfAnimation ? fromSeg.arcLarge : toSeg.arcLarge;
        blendedSegment.arcSweep = m_isInFirstHalfOfAnimation ? fromSeg.arcSweep : toSeg.arcSweep;
    }

    m_fromCurrentPoint = m_fromIsAbsolute ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toIsAbsolute ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
    return blendedSegment;
}

void SVGPathBlender::blendSegments(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    PathSegmentData blendedSegment;
    switch (toSeg.command) {
    case PathSegMoveToRel:
    case PathSegMoveToAbs:
        blendedSegment = blendMoveToSegment(fromSeg, toSeg);
        break;
    case PathSegLineToRel:
    case PathSegLineToAbs:
        blendedSegment = blendLineToSegment(fromSeg, toSeg);
        break;
    case PathSegLineToHorizontalRel:
    case PathSegLineToHorizontalAbs:
        blendedSegment = blendLineToHorizontalSegment(fromSeg, toSeg);
        break;
    case PathSegLineToVerticalRel:
    case PathSegLineToVerticalAbs:
        blendedSegment = blendLineToVerticalSegment(fromSeg, toSeg);
        break;
    case PathSegClosePath:
        blendedSegment = toSeg;
        break;
    case PathSegCurveToCubicRel:
    case PathSegCurveToCubicAbs:
        blendedSegment = blendCurveToCubicSegment(fromSeg, toSeg);
        break;
    case PathSegCurveToCubicSmoothRel:
    case PathSegCurveToCubicSmoothAbs:
        blendedSegment = blendCurveToCubicSmoothSegment(fromSeg, toSeg);
        break;
    case PathSegCurveToQuadraticRel:
    case PathSegCurveToQuadraticAbs:
        blendedSegment = blendCurveToQuadraticSegment(fromSeg, toSeg);
        break;
    case PathSegCurveToQuadraticSmoothRel:
    case PathSegCurveToQuadraticSmoothAbs:
        blendedSegment = blendCurveToQuadraticSmoothSegment(fromSeg, toSeg);
        break;
    case PathSegArcRel:
    case PathSegArcAbs:
        blendedSegment = blendArcToSegment(fromSeg, toSeg);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    m_consumer->emitSegment(blendedSegment);
}

bool SVGPathBlender::addAnimatedPath(unsigned repeatCount)
{
    TemporaryChange<unsigned> change(m_addTypesCount, repeatCount);
    return blendAnimatedPath(0);
}

bool SVGPathBlender::blendAnimatedPath(float progress)
{
    m_isInFirstHalfOfAnimation = progress < 0.5f;
    m_progress = progress;

    bool fromSourceHadData = m_fromSource->hasMoreData();
    while (m_toSource->hasMoreData()) {
        PathSegmentData toSeg = m_toSource->parseSegment();
        if (toSeg.command == PathSegUnknown)
            return false;

        PathSegmentData fromSeg;
        fromSeg.command = toSeg.command;

        if (m_fromSource->hasMoreData()) {
            fromSeg = m_fromSource->parseSegment();
            if (fromSeg.command == PathSegUnknown)
                return false;
        }

        m_typesAreEqual = fromSeg.command == toSeg.command;

        // If the types are equal, they'll blend regardless of parameters.
        if (!m_typesAreEqual) {
            // Addition require segments with the same type.
            if (m_addTypesCount)
                return false;
            // Allow the segments to differ in "relativeness".
            if (toAbsolutePathSegType(fromSeg.command) != toAbsolutePathSegType(toSeg.command))
                return false;
        }

        m_fromIsAbsolute = isAbsolutePathSegType(fromSeg.command);
        m_toIsAbsolute = isAbsolutePathSegType(toSeg.command);

        blendSegments(fromSeg, toSeg);

        if (!fromSourceHadData)
            continue;
        if (m_fromSource->hasMoreData() != m_toSource->hasMoreData())
            return false;
    }
    return true;
}

}
