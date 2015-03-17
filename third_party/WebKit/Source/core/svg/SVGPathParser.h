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

#ifndef SVGPathParser_h
#define SVGPathParser_h

#include "core/svg/SVGPathConsumer.h"
#include "core/svg/SVGPathSeg.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGPathSource;

class SVGPathParser final : public NoBaseWillBeGarbageCollected<SVGPathParser> {
    WTF_MAKE_NONCOPYABLE(SVGPathParser); WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    SVGPathParser(SVGPathSource* source, SVGPathConsumer* consumer)
        : m_source(source)
        , m_consumer(consumer)
    {
        ASSERT(m_source);
        ASSERT(m_consumer);
    }

    bool parsePathDataFromSource(PathParsingMode, bool checkForInitialMoveTo = true);

    DECLARE_TRACE();

private:
    bool decomposeArcToCubic(float, float, float, const FloatPoint&, const FloatPoint&, bool largeArcFlag, bool sweepFlag);
    void parseClosePathSegment();
    bool parseMoveToSegment();
    bool parseLineToSegment();
    bool parseLineToHorizontalSegment();
    bool parseLineToVerticalSegment();
    bool parseCurveToCubicSegment();
    bool parseCurveToCubicSmoothSegment();
    bool parseCurveToQuadraticSegment();
    bool parseCurveToQuadraticSmoothSegment();
    bool parseArcToSegment();

    RawPtrWillBeMember<SVGPathSource> m_source;
    RawPtrWillBeMember<SVGPathConsumer> m_consumer;
    PathCoordinateMode m_mode;
    PathParsingMode m_pathParsingMode;
    SVGPathSegType m_lastCommand;
    bool m_closePath;
    FloatPoint m_controlPoint;
    FloatPoint m_currentPoint;
    FloatPoint m_subPathPoint;
};

} // namespace blink

#endif // SVGPathParser_h
