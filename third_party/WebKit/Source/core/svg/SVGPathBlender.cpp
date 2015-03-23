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
        ASSERT(m_fromMode == m_toMode);
        return from + to * m_addTypesCount;
    }

    if (m_fromMode == m_toMode)
        return blend(from, to, m_progress);

    float fromValue = blendMode == BlendHorizontal ? m_fromCurrentPoint.x() : m_fromCurrentPoint.y();
    float toValue = blendMode == BlendHorizontal ? m_toCurrentPoint.x() : m_toCurrentPoint.y();

    // Transform toY to the coordinate mode of fromY
    float animValue = blend(from, m_fromMode == AbsoluteCoordinates ? to + toValue : to - toValue, m_progress);

    if (m_isInFirstHalfOfAnimation)
        return animValue;

    // Transform the animated point to the coordinate mode, needed for the current progress.
    float currentValue = blend(fromValue, toValue, m_progress);
    return m_toMode == AbsoluteCoordinates ? animValue + currentValue : animValue - currentValue;
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
    if (m_fromMode == m_toMode)
        return blendAnimatedFloatPointSameCoordinates(fromPoint, toPoint);

    // Transform toPoint to the coordinate mode of fromPoint
    FloatPoint animatedPoint = toPoint;
    if (m_fromMode == AbsoluteCoordinates)
        animatedPoint += m_toCurrentPoint;
    else
        animatedPoint.move(-m_toCurrentPoint.x(), -m_toCurrentPoint.y());

    animatedPoint = blendFloatPoint(fromPoint, animatedPoint, m_progress);

    if (m_isInFirstHalfOfAnimation)
        return animatedPoint;

    // Transform the animated point to the coordinate mode, needed for the current progress.
    FloatPoint currentPoint = blendFloatPoint(m_fromCurrentPoint, m_toCurrentPoint, m_progress);
    if (m_toMode == AbsoluteCoordinates)
        return animatedPoint + currentPoint;

    animatedPoint.move(-currentPoint.x(), -currentPoint.y());
    return animatedPoint;
}

void SVGPathBlender::blendMoveToSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    m_consumer->moveTo(blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint), m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
}

void SVGPathBlender::blendLineToSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    m_consumer->lineTo(blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint), m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
}

void SVGPathBlender::blendLineToHorizontalSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    float fromX = fromSeg.targetPoint.x();
    float toX = toSeg.targetPoint.x();
    m_consumer->lineToHorizontal(blendAnimatedDimensonalFloat(fromX, toX, BlendHorizontal), m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint.setX(m_fromMode == AbsoluteCoordinates ? fromX : m_fromCurrentPoint.x() + fromX);
    m_toCurrentPoint.setX(m_toMode == AbsoluteCoordinates ? toX : m_toCurrentPoint.x() + toX);
}

void SVGPathBlender::blendLineToVerticalSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    float fromY = fromSeg.targetPoint.y();
    float toY = toSeg.targetPoint.y();
    m_consumer->lineToVertical(blendAnimatedDimensonalFloat(fromY, toY, BlendVertical), m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint.setY(m_fromMode == AbsoluteCoordinates ? fromY : m_fromCurrentPoint.y() + fromY);
    m_toCurrentPoint.setY(m_toMode == AbsoluteCoordinates ? toY : m_toCurrentPoint.y() + toY);
}

void SVGPathBlender::blendCurveToCubicSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    m_consumer->curveToCubic(
        blendAnimatedFloatPoint(fromSeg.point1, toSeg.point1),
        blendAnimatedFloatPoint(fromSeg.point2, toSeg.point2),
        blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint),
        m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
}

void SVGPathBlender::blendCurveToCubicSmoothSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    m_consumer->curveToCubicSmooth(
        blendAnimatedFloatPoint(fromSeg.point2, toSeg.point2),
        blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint),
        m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
}

void SVGPathBlender::blendCurveToQuadraticSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    m_consumer->curveToQuadratic(
        blendAnimatedFloatPoint(fromSeg.point1, toSeg.point1),
        blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint),
        m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
}

void SVGPathBlender::blendCurveToQuadraticSmoothSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    m_consumer->curveToQuadraticSmooth(blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint), m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
}

void SVGPathBlender::blendArcToSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg)
{
    ASSERT(!m_addTypesCount || fromSeg.command == toSeg.command);

    bool blendedLargeArc;
    bool blendedSweep;

    if (m_addTypesCount) {
        blendedLargeArc = fromSeg.arcLarge || toSeg.arcLarge;
        blendedSweep = fromSeg.arcSweep || toSeg.arcSweep;
    } else {
        blendedLargeArc = m_isInFirstHalfOfAnimation ? fromSeg.arcLarge : toSeg.arcLarge;
        blendedSweep = m_isInFirstHalfOfAnimation ? fromSeg.arcSweep : toSeg.arcSweep;
    }

    FloatPoint blendedRadii = blendAnimatedFloatPointSameCoordinates(fromSeg.arcRadii(), toSeg.arcRadii());
    float blendedAngle = blendAnimatedFloatPointSameCoordinates(fromSeg.point2, toSeg.point2).x();

    m_consumer->arcTo(
        blendedRadii.x(), blendedRadii.y(), blendedAngle, blendedLargeArc, blendedSweep,
        blendAnimatedFloatPoint(fromSeg.targetPoint, toSeg.targetPoint),
        m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);

    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? fromSeg.targetPoint : m_fromCurrentPoint + fromSeg.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? toSeg.targetPoint : m_toCurrentPoint + toSeg.targetPoint;
}

static inline PathCoordinateMode coordinateModeOfCommand(const SVGPathSegType& type)
{
    if (type < PathSegMoveToAbs)
        return AbsoluteCoordinates;

    // Odd number = relative command
    if (type % 2)
        return RelativeCoordinates;

    return AbsoluteCoordinates;
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

        if (toAbsolutePathSegType(fromSeg.command) != toAbsolutePathSegType(toSeg.command))
            return false;

        m_fromMode = coordinateModeOfCommand(fromSeg.command);
        m_toMode = coordinateModeOfCommand(toSeg.command);

        if (m_addTypesCount && m_fromMode != m_toMode)
            return false;

        switch (toSeg.command) {
        case PathSegMoveToRel:
        case PathSegMoveToAbs:
            blendMoveToSegment(fromSeg, toSeg);
            break;
        case PathSegLineToRel:
        case PathSegLineToAbs:
            blendLineToSegment(fromSeg, toSeg);
            break;
        case PathSegLineToHorizontalRel:
        case PathSegLineToHorizontalAbs:
            blendLineToHorizontalSegment(fromSeg, toSeg);
            break;
        case PathSegLineToVerticalRel:
        case PathSegLineToVerticalAbs:
            blendLineToVerticalSegment(fromSeg, toSeg);
            break;
        case PathSegClosePath:
            m_consumer->closePath();
            break;
        case PathSegCurveToCubicRel:
        case PathSegCurveToCubicAbs:
            blendCurveToCubicSegment(fromSeg, toSeg);
            break;
        case PathSegCurveToCubicSmoothRel:
        case PathSegCurveToCubicSmoothAbs:
            blendCurveToCubicSmoothSegment(fromSeg, toSeg);
            break;
        case PathSegCurveToQuadraticRel:
        case PathSegCurveToQuadraticAbs:
            blendCurveToQuadraticSegment(fromSeg, toSeg);
            break;
        case PathSegCurveToQuadraticSmoothRel:
        case PathSegCurveToQuadraticSmoothAbs:
            blendCurveToQuadraticSmoothSegment(fromSeg, toSeg);
            break;
        case PathSegArcRel:
        case PathSegArcAbs:
            blendArcToSegment(fromSeg, toSeg);
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        if (!fromSourceHadData)
            continue;
        if (m_fromSource->hasMoreData() != m_toSource->hasMoreData())
            return false;
    }
    return true;
}

}
