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

#include "core/CoreExport.h"
#include "core/svg/SVGPathData.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGPathConsumer;
class SVGPathSource;

class CORE_EXPORT SVGPathParser final {
    WTF_MAKE_NONCOPYABLE(SVGPathParser);
    STACK_ALLOCATED();
public:
    SVGPathParser(SVGPathSource* source, SVGPathConsumer* consumer)
        : m_source(source)
        , m_consumer(consumer)
    {
        ASSERT(m_source);
        ASSERT(m_consumer);
    }

    bool parsePathDataFromSource(bool checkForInitialMoveTo = true)
    {
        ASSERT(m_source);
        ASSERT(m_consumer);
        if (checkForInitialMoveTo && !initialCommandIsMoveTo())
            return false;
        return parsePath();
    }

private:
    bool initialCommandIsMoveTo();
    bool parsePath();

    SVGPathSource* m_source;
    SVGPathConsumer* m_consumer;
};

class SVGPathNormalizer {
    STACK_ALLOCATED();
public:
    SVGPathNormalizer(SVGPathConsumer* consumer)
        : m_consumer(consumer)
        , m_lastCommand(PathSegUnknown)
    {
        ASSERT(m_consumer);
    }

    void emitSegment(const PathSegmentData&);

private:
    bool decomposeArcToCubic(const FloatPoint& currentPoint, const PathSegmentData&);

    SVGPathConsumer* m_consumer;
    FloatPoint m_controlPoint;
    FloatPoint m_currentPoint;
    FloatPoint m_subPathPoint;
    SVGPathSegType m_lastCommand;
};

} // namespace blink

#endif // SVGPathParser_h
