// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/PathSVGInterpolation.h"

#include "core/svg/SVGPathByteStreamBuilder.h"
#include "core/svg/SVGPathByteStreamSource.h"
#include "core/svg/SVGPathElement.h"
#include "core/svg/SVGPathParser.h"

namespace blink {

namespace {

struct SubPathCoordinates {
    double initialX = 0;
    double initialY = 0;
    double currentX = 0;
    double currentY = 0;
};

PassOwnPtr<InterpolableNumber> controlToInterpolableValue(double value, bool isAbsolute, double currentValue)
{
    if (isAbsolute)
        return InterpolableNumber::create(value);
    return InterpolableNumber::create(currentValue + value);
}

double controlFromInterpolableValue(const InterpolableValue* number, bool isAbsolute, double currentValue)
{
    double value = toInterpolableNumber(number)->value();

    if (isAbsolute)
        return value;
    return value - currentValue;
}

PassOwnPtr<InterpolableNumber> specifiedToInterpolableValue(double value, bool isAbsolute, double& currentValue)
{
    if (isAbsolute)
        currentValue = value;
    else
        currentValue += value;
    return InterpolableNumber::create(currentValue);
}

double specifiedFromInterpolableValue(const InterpolableValue* number, bool isAbsolute, double& currentValue)
{
    double previousValue = currentValue;
    currentValue = toInterpolableNumber(number)->value();

    if (isAbsolute)
        return currentValue;
    return currentValue - previousValue;
}

PassOwnPtr<InterpolableValue> pathSegClosePathToInterpolableValue(const PathSegmentData&, SubPathCoordinates& coordinates)
{
    coordinates.currentX = coordinates.initialX;
    coordinates.currentY = coordinates.initialY;

    // arbitrary
    return InterpolableBool::create(false);
}

PathSegmentData pathSegClosePathFromInterpolableValue(const InterpolableValue&, SVGPathSegType segType, SubPathCoordinates& coordinates)
{
    coordinates.currentX = coordinates.initialX;
    coordinates.currentY = coordinates.initialY;

    PathSegmentData segment;
    segment.command = segType;
    return segment;
}

PassOwnPtr<InterpolableValue> pathSegSingleCoordinateToInterpolableValue(const PathSegmentData& segment, SubPathCoordinates& coordinates)
{
    bool isAbsolute = isAbsolutePathSegType(segment.command);
    OwnPtr<InterpolableList> result = InterpolableList::create(2);
    result->set(0, specifiedToInterpolableValue(segment.x(), isAbsolute, coordinates.currentX));
    result->set(1, specifiedToInterpolableValue(segment.y(), isAbsolute, coordinates.currentY));

    if (toAbsolutePathSegType(segment.command) == PathSegMoveToAbs) {
        // Any upcoming 'closepath' commands bring us back to the location we have just moved to.
        coordinates.initialX = coordinates.currentX;
        coordinates.initialY = coordinates.currentY;
    }

    return result.release();
}

PathSegmentData pathSegSingleCoordinateFromInterpolableValue(const InterpolableValue& value, SVGPathSegType segType, SubPathCoordinates& coordinates)
{
    const InterpolableList& list = toInterpolableList(value);
    bool isAbsolute = isAbsolutePathSegType(segType);
    PathSegmentData segment;
    segment.command = segType;
    segment.targetPoint.setX(specifiedFromInterpolableValue(list.get(0), isAbsolute, coordinates.currentX));
    segment.targetPoint.setY(specifiedFromInterpolableValue(list.get(1), isAbsolute, coordinates.currentY));

    if (toAbsolutePathSegType(segType) == PathSegMoveToAbs) {
        // Any upcoming 'closepath' commands bring us back to the location we have just moved to.
        coordinates.initialX = coordinates.currentX;
        coordinates.initialY = coordinates.currentY;
    }

    return segment;
}

PassOwnPtr<InterpolableValue> pathSegCurvetoCubicToInterpolableValue(const PathSegmentData& segment, SubPathCoordinates& coordinates)
{
    bool isAbsolute = isAbsolutePathSegType(segment.command);
    OwnPtr<InterpolableList> result = InterpolableList::create(6);
    result->set(0, controlToInterpolableValue(segment.x1(), isAbsolute, coordinates.currentX));
    result->set(1, controlToInterpolableValue(segment.y1(), isAbsolute, coordinates.currentY));
    result->set(2, controlToInterpolableValue(segment.x2(), isAbsolute, coordinates.currentX));
    result->set(3, controlToInterpolableValue(segment.y2(), isAbsolute, coordinates.currentY));
    result->set(4, specifiedToInterpolableValue(segment.x(), isAbsolute, coordinates.currentX));
    result->set(5, specifiedToInterpolableValue(segment.y(), isAbsolute, coordinates.currentY));
    return result.release();
}

PathSegmentData pathSegCurvetoCubicFromInterpolableValue(const InterpolableValue& value, SVGPathSegType segType, SubPathCoordinates& coordinates)
{
    const InterpolableList& list = toInterpolableList(value);
    bool isAbsolute = isAbsolutePathSegType(segType);
    PathSegmentData segment;
    segment.command = segType;
    segment.point1.setX(controlFromInterpolableValue(list.get(0), isAbsolute, coordinates.currentX));
    segment.point1.setY(controlFromInterpolableValue(list.get(1), isAbsolute, coordinates.currentY));
    segment.point2.setX(controlFromInterpolableValue(list.get(2), isAbsolute, coordinates.currentX));
    segment.point2.setY(controlFromInterpolableValue(list.get(3), isAbsolute, coordinates.currentY));
    segment.targetPoint.setX(specifiedFromInterpolableValue(list.get(4), isAbsolute, coordinates.currentX));
    segment.targetPoint.setY(specifiedFromInterpolableValue(list.get(5), isAbsolute, coordinates.currentY));
    return segment;
}

PassOwnPtr<InterpolableValue> pathSegCurvetoQuadraticToInterpolableValue(const PathSegmentData& segment, SubPathCoordinates& coordinates)
{
    bool isAbsolute = isAbsolutePathSegType(segment.command);
    OwnPtr<InterpolableList> result = InterpolableList::create(4);
    result->set(0, controlToInterpolableValue(segment.x1(), isAbsolute, coordinates.currentX));
    result->set(1, controlToInterpolableValue(segment.y1(), isAbsolute, coordinates.currentY));
    result->set(2, specifiedToInterpolableValue(segment.x(), isAbsolute, coordinates.currentX));
    result->set(3, specifiedToInterpolableValue(segment.y(), isAbsolute, coordinates.currentY));
    return result.release();
}

PathSegmentData pathSegCurvetoQuadraticFromInterpolableValue(const InterpolableValue& value, SVGPathSegType segType, SubPathCoordinates& coordinates)
{
    const InterpolableList& list = toInterpolableList(value);
    bool isAbsolute = isAbsolutePathSegType(segType);
    PathSegmentData segment;
    segment.command = segType;
    segment.point1.setX(controlFromInterpolableValue(list.get(0), isAbsolute, coordinates.currentX));
    segment.point1.setY(controlFromInterpolableValue(list.get(1), isAbsolute, coordinates.currentY));
    segment.targetPoint.setX(specifiedFromInterpolableValue(list.get(2), isAbsolute, coordinates.currentX));
    segment.targetPoint.setY(specifiedFromInterpolableValue(list.get(3), isAbsolute, coordinates.currentY));
    return segment;
}

PassOwnPtr<InterpolableValue> pathSegArcToInterpolableValue(const PathSegmentData& segment, SubPathCoordinates& coordinates)
{
    bool isAbsolute = isAbsolutePathSegType(segment.command);
    OwnPtr<InterpolableList> result = InterpolableList::create(7);
    result->set(0, specifiedToInterpolableValue(segment.x(), isAbsolute, coordinates.currentX));
    result->set(1, specifiedToInterpolableValue(segment.y(), isAbsolute, coordinates.currentY));
    result->set(2, InterpolableNumber::create(segment.r1()));
    result->set(3, InterpolableNumber::create(segment.r2()));
    result->set(4, InterpolableNumber::create(segment.arcAngle()));
    result->set(5, InterpolableBool::create(segment.largeArcFlag()));
    result->set(6, InterpolableBool::create(segment.sweepFlag()));
    return result.release();
}

PathSegmentData pathSegArcFromInterpolableValue(const InterpolableValue& value, SVGPathSegType segType, SubPathCoordinates& coordinates)
{
    const InterpolableList& list = toInterpolableList(value);
    bool isAbsolute = isAbsolutePathSegType(segType);
    PathSegmentData segment;
    segment.command = segType;
    segment.targetPoint.setX(specifiedFromInterpolableValue(list.get(0), isAbsolute, coordinates.currentX));
    segment.targetPoint.setY(specifiedFromInterpolableValue(list.get(1), isAbsolute, coordinates.currentY));
    segment.point1.setX(toInterpolableNumber(list.get(2))->value());
    segment.point1.setY(toInterpolableNumber(list.get(3))->value());
    segment.point2.setX(toInterpolableNumber(list.get(4))->value());
    segment.arcLarge = toInterpolableBool(list.get(5))->value();
    segment.arcSweep = toInterpolableBool(list.get(6))->value();
    return segment;
}

PassOwnPtr<InterpolableValue> pathSegLinetoHorizontalToInterpolableValue(const PathSegmentData& segment, SubPathCoordinates& coordinates)
{
    bool isAbsolute = isAbsolutePathSegType(segment.command);
    return specifiedToInterpolableValue(segment.x(), isAbsolute, coordinates.currentX);
}

PathSegmentData pathSegLinetoHorizontalFromInterpolableValue(const InterpolableValue& value, SVGPathSegType segType, SubPathCoordinates& coordinates)
{
    bool isAbsolute = isAbsolutePathSegType(segType);
    PathSegmentData segment;
    segment.command = segType;
    segment.targetPoint.setX(specifiedFromInterpolableValue(&value, isAbsolute, coordinates.currentX));
    return segment;
}

PassOwnPtr<InterpolableValue> pathSegLinetoVerticalToInterpolableValue(const PathSegmentData& segment, SubPathCoordinates& coordinates)
{
    bool isAbsolute = isAbsolutePathSegType(segment.command);
    return specifiedToInterpolableValue(segment.y(), isAbsolute, coordinates.currentY);
}

PathSegmentData pathSegLinetoVerticalFromInterpolableValue(const InterpolableValue& value, SVGPathSegType segType, SubPathCoordinates& coordinates)
{
    bool isAbsolute = isAbsolutePathSegType(segType);
    PathSegmentData segment;
    segment.command = segType;
    segment.targetPoint.setY(specifiedFromInterpolableValue(&value, isAbsolute, coordinates.currentY));
    return segment;
}

PassOwnPtr<InterpolableValue> pathSegCurvetoCubicSmoothToInterpolableValue(const PathSegmentData& segment, SubPathCoordinates& coordinates)
{
    bool isAbsolute = isAbsolutePathSegType(segment.command);
    OwnPtr<InterpolableList> result = InterpolableList::create(4);
    result->set(0, controlToInterpolableValue(segment.x2(), isAbsolute, coordinates.currentX));
    result->set(1, controlToInterpolableValue(segment.y2(), isAbsolute, coordinates.currentY));
    result->set(2, specifiedToInterpolableValue(segment.x(), isAbsolute, coordinates.currentX));
    result->set(3, specifiedToInterpolableValue(segment.y(), isAbsolute, coordinates.currentY));
    return result.release();
}

PathSegmentData pathSegCurvetoCubicSmoothFromInterpolableValue(const InterpolableValue& value, SVGPathSegType segType, SubPathCoordinates& coordinates)
{
    const InterpolableList& list = toInterpolableList(value);
    bool isAbsolute = isAbsolutePathSegType(segType);
    PathSegmentData segment;
    segment.command = segType;
    segment.point2.setX(controlFromInterpolableValue(list.get(0), isAbsolute, coordinates.currentX));
    segment.point2.setY(controlFromInterpolableValue(list.get(1), isAbsolute, coordinates.currentY));
    segment.targetPoint.setX(specifiedFromInterpolableValue(list.get(2), isAbsolute, coordinates.currentX));
    segment.targetPoint.setY(specifiedFromInterpolableValue(list.get(3), isAbsolute, coordinates.currentY));
    return segment;
}

PassOwnPtr<InterpolableValue> pathSegToInterpolableValue(const PathSegmentData& segment, SubPathCoordinates& coordinates, SVGPathSegType* ptrSegType)
{
    if (ptrSegType)
        *ptrSegType = segment.command;

    switch (segment.command) {
    case PathSegClosePath:
        return pathSegClosePathToInterpolableValue(segment, coordinates);

    case PathSegMoveToAbs:
    case PathSegMoveToRel:
    case PathSegLineToAbs:
    case PathSegLineToRel:
    case PathSegCurveToQuadraticSmoothAbs:
    case PathSegCurveToQuadraticSmoothRel:
        return pathSegSingleCoordinateToInterpolableValue(segment, coordinates);

    case PathSegCurveToCubicAbs:
    case PathSegCurveToCubicRel:
        return pathSegCurvetoCubicToInterpolableValue(segment, coordinates);

    case PathSegCurveToQuadraticAbs:
    case PathSegCurveToQuadraticRel:
        return pathSegCurvetoQuadraticToInterpolableValue(segment, coordinates);

    case PathSegArcAbs:
    case PathSegArcRel:
        return pathSegArcToInterpolableValue(segment, coordinates);

    case PathSegLineToHorizontalAbs:
    case PathSegLineToHorizontalRel:
        return pathSegLinetoHorizontalToInterpolableValue(segment, coordinates);

    case PathSegLineToVerticalAbs:
    case PathSegLineToVerticalRel:
        return pathSegLinetoVerticalToInterpolableValue(segment, coordinates);

    case PathSegCurveToCubicSmoothAbs:
    case PathSegCurveToCubicSmoothRel:
        return pathSegCurvetoCubicSmoothToInterpolableValue(segment, coordinates);

    case PathSegUnknown:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

PathSegmentData pathSegFromInterpolableValue(const InterpolableValue& value, SVGPathSegType segType, SubPathCoordinates& coordinates)
{
    switch (segType) {
    case PathSegClosePath:
        return pathSegClosePathFromInterpolableValue(value, segType, coordinates);

    case PathSegMoveToAbs:
    case PathSegMoveToRel:
    case PathSegLineToAbs:
    case PathSegLineToRel:
    case PathSegCurveToQuadraticSmoothAbs:
    case PathSegCurveToQuadraticSmoothRel:
        return pathSegSingleCoordinateFromInterpolableValue(value, segType, coordinates);

    case PathSegCurveToCubicAbs:
    case PathSegCurveToCubicRel:
        return pathSegCurvetoCubicFromInterpolableValue(value, segType, coordinates);

    case PathSegCurveToQuadraticAbs:
    case PathSegCurveToQuadraticRel:
        return pathSegCurvetoQuadraticFromInterpolableValue(value, segType, coordinates);

    case PathSegArcAbs:
    case PathSegArcRel:
        return pathSegArcFromInterpolableValue(value, segType, coordinates);

    case PathSegLineToHorizontalAbs:
    case PathSegLineToHorizontalRel:
        return pathSegLinetoHorizontalFromInterpolableValue(value, segType, coordinates);

    case PathSegLineToVerticalAbs:
    case PathSegLineToVerticalRel:
        return pathSegLinetoVerticalFromInterpolableValue(value, segType, coordinates);

    case PathSegCurveToCubicSmoothAbs:
    case PathSegCurveToCubicSmoothRel:
        return pathSegCurvetoCubicSmoothFromInterpolableValue(value, segType, coordinates);

    case PathSegUnknown:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return PathSegmentData();
}

class InterpolatedPathSource : public SVGPathSource {
public:
    InterpolatedPathSource(const InterpolableList& listValue, const Vector<SVGPathSegType>& pathSegTypes)
        : m_currentIndex(0)
        , m_list(listValue)
        , m_segmentTypes(pathSegTypes)
    {
        ASSERT(m_list.length() == m_segmentTypes.size());
    }

private:
    bool hasMoreData() const override;
    SVGPathSegType peekSegmentType() override;
    PathSegmentData parseSegment() override;

    SubPathCoordinates m_normalizationState;
    size_t m_currentIndex;
    const InterpolableList& m_list;
    const Vector<SVGPathSegType>& m_segmentTypes;
};

bool InterpolatedPathSource::hasMoreData() const
{
    return m_currentIndex < m_list.length();
}

SVGPathSegType InterpolatedPathSource::peekSegmentType()
{
    ASSERT(hasMoreData());
    return m_segmentTypes.at(m_currentIndex);
}

PathSegmentData InterpolatedPathSource::parseSegment()
{
    PathSegmentData segment = pathSegFromInterpolableValue(*m_list.get(m_currentIndex), m_segmentTypes.at(m_currentIndex), m_normalizationState);
    ++m_currentIndex;
    return segment;
}

size_t countPathCommands(const SVGPathByteStream& path)
{
    size_t count = 0;
    SVGPathByteStreamSource pathSource(path);
    while (pathSource.hasMoreData()) {
        pathSource.parseSegment();
        ++count;
    }
    return count;
}

} // namespace

PassRefPtr<PathSVGInterpolation> PathSVGInterpolation::maybeCreate(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
{
    ASSERT(start->type() == SVGPathSegList::classType());
    ASSERT(end->type() == SVGPathSegList::classType());

    const SVGPathByteStream& startPath = static_cast<SVGPathSegList*>(start)->byteStream();
    const SVGPathByteStream& endPath = static_cast<SVGPathSegList*>(end)->byteStream();

    if (startPath.size() != endPath.size())
        return nullptr;

    size_t length = countPathCommands(startPath);

    SVGPathByteStreamSource startPathSource(startPath);
    SVGPathByteStreamSource endPathSource(endPath);

    Vector<SVGPathSegType> pathSegTypes(length);
    OwnPtr<InterpolableList> startValue = InterpolableList::create(length);
    OwnPtr<InterpolableList> endValue = InterpolableList::create(length);
    SubPathCoordinates startCoordinates;
    SubPathCoordinates endCoordinates;
    size_t i = 0;
    while (startPathSource.hasMoreData()) {
        if (toAbsolutePathSegType(startPathSource.peekSegmentType()) != toAbsolutePathSegType(endPathSource.peekSegmentType()))
            return nullptr;

        // Like Firefox SMIL, we use the final path seg type.
        const PathSegmentData startSeg = startPathSource.parseSegment();
        startValue->set(i, pathSegToInterpolableValue(startSeg, startCoordinates, nullptr));

        const PathSegmentData endSeg = endPathSource.parseSegment();
        endValue->set(i, pathSegToInterpolableValue(endSeg, endCoordinates, &pathSegTypes.at(i)));

        ++i;
    }
    ASSERT(!endPathSource.hasMoreData());
    ASSERT(i == length);

    return adoptRef(new PathSVGInterpolation(startValue.release(), endValue.release(), attribute, pathSegTypes));
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> PathSVGInterpolation::fromInterpolableValue(const InterpolableValue& value, const Vector<SVGPathSegType>& pathSegTypes, SVGPathElement* element)
{
    RefPtrWillBeRawPtr<SVGPathSegList> result = SVGPathSegList::create(element);
    result->invalidateList();
    InterpolatedPathSource source(toInterpolableList(value), pathSegTypes);
    SVGPathByteStreamBuilder builder(result->mutableByteStream());
    SVGPathParser parser(&source, &builder);
    parser.parsePathDataFromSource(UnalteredParsing, false);
    return result.release();
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> PathSVGInterpolation::interpolatedValue(SVGElement& element) const
{
    return fromInterpolableValue(*m_cachedValue, m_pathSegTypes, toSVGPathElement(&element));
}

}
