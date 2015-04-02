/*
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

#ifndef SVGPathBlender_h
#define SVGPathBlender_h

#include "platform/geometry/FloatPoint.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

enum FloatBlendMode {
    BlendHorizontal,
    BlendVertical
};

struct PathSegmentData;
class SVGPathConsumer;
class SVGPathSource;

class SVGPathBlender : public NoBaseWillBeGarbageCollectedFinalized<SVGPathBlender> {
    WTF_MAKE_NONCOPYABLE(SVGPathBlender); WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(SVGPathBlender);
public:
    SVGPathBlender(SVGPathSource* fromSource, SVGPathSource* toSource, SVGPathConsumer*);

    bool addAnimatedPath(unsigned repeatCount);
    bool blendAnimatedPath(float);

    DECLARE_TRACE();

private:
    PathSegmentData blendMoveToSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg);
    PathSegmentData blendLineToSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg);
    PathSegmentData blendLineToHorizontalSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg);
    PathSegmentData blendLineToVerticalSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg);
    PathSegmentData blendCurveToCubicSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg);
    PathSegmentData blendCurveToCubicSmoothSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg);
    PathSegmentData blendCurveToQuadraticSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg);
    PathSegmentData blendCurveToQuadraticSmoothSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg);
    PathSegmentData blendArcToSegment(const PathSegmentData& fromSeg, const PathSegmentData& toSeg);
    void blendSegments(const PathSegmentData& fromSeg, const PathSegmentData& toSeg);

    float blendAnimatedDimensonalFloat(float, float, FloatBlendMode);
    FloatPoint blendAnimatedFloatPoint(const FloatPoint& from, const FloatPoint& to);
    FloatPoint blendAnimatedFloatPointSameCoordinates(const FloatPoint& from, const FloatPoint& to);

    RawPtrWillBeMember<SVGPathSource> m_fromSource;
    RawPtrWillBeMember<SVGPathSource> m_toSource;
    RawPtrWillBeMember<SVGPathConsumer> m_consumer;

    FloatPoint m_fromCurrentPoint;
    FloatPoint m_toCurrentPoint;

    float m_progress;
    unsigned m_addTypesCount;
    bool m_isInFirstHalfOfAnimation;
    // This is per-segment blend state corresponding to the 'from' and 'to'
    // segments currently being blended, and only used within blendSegments().
    bool m_typesAreEqual;
    bool m_fromIsAbsolute;
    bool m_toIsAbsolute;
};

} // namespace blink

#endif // SVGPathBlender_h
