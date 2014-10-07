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

#ifndef SVGPathSegListSource_h
#define SVGPathSegListSource_h

#include "core/svg/SVGPathSeg.h"
#include "core/svg/SVGPathSegList.h"
#include "core/svg/SVGPathSource.h"
#include "platform/geometry/FloatPoint.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class SVGPathSegListSource final : public SVGPathSource {
public:
    SVGPathSegListSource(SVGPathSegList::ConstIterator, SVGPathSegList::ConstIterator);

private:
    virtual bool hasMoreData() const override;
    virtual bool moveToNextToken() override { return true; }
    virtual bool parseSVGSegmentType(SVGPathSegType&) override;
    virtual SVGPathSegType nextCommand(SVGPathSegType) override;

    virtual bool parseMoveToSegment(FloatPoint&) override;
    virtual bool parseLineToSegment(FloatPoint&) override;
    virtual bool parseLineToHorizontalSegment(float&) override;
    virtual bool parseLineToVerticalSegment(float&) override;
    virtual bool parseCurveToCubicSegment(FloatPoint&, FloatPoint&, FloatPoint&) override;
    virtual bool parseCurveToCubicSmoothSegment(FloatPoint&, FloatPoint&) override;
    virtual bool parseCurveToQuadraticSegment(FloatPoint&, FloatPoint&) override;
    virtual bool parseCurveToQuadraticSmoothSegment(FloatPoint&) override;
    virtual bool parseArcToSegment(float&, float&, float&, bool&, bool&, FloatPoint&) override;

    RefPtr<SVGPathSeg> m_segment;
    SVGPathSegList::ConstIterator m_itCurrent;
    SVGPathSegList::ConstIterator m_itEnd;
};

} // namespace blink

#endif // SVGPathSegListSource_h
