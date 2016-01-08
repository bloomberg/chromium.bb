// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGPathInterpolationType.h"

#include "core/animation/InterpolatedSVGPathSource.h"
#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/SVGPathSegInterpolationFunctions.h"
#include "core/svg/SVGPath.h"
#include "core/svg/SVGPathByteStreamBuilder.h"
#include "core/svg/SVGPathByteStreamSource.h"
#include "core/svg/SVGPathParser.h"

namespace blink {

class SVGPathNonInterpolableValue : public NonInterpolableValue {
public:
    virtual ~SVGPathNonInterpolableValue() {}

    static PassRefPtr<SVGPathNonInterpolableValue> create(Vector<SVGPathSegType>& pathSegTypes)
    {
        return adoptRef(new SVGPathNonInterpolableValue(pathSegTypes));
    }

    const Vector<SVGPathSegType>& pathSegTypes() const { return m_pathSegTypes; }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    SVGPathNonInterpolableValue(Vector<SVGPathSegType>& pathSegTypes)
    {
        m_pathSegTypes.swap(pathSegTypes);
    }

    Vector<SVGPathSegType> m_pathSegTypes;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(SVGPathNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(SVGPathNonInterpolableValue);

enum PathComponentIndex {
    PathArgsIndex,
    PathNeutralIndex,
    PathComponentIndexCount,
};

PassOwnPtr<InterpolationValue> SVGPathInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedPath)
        return nullptr;

    SVGPathByteStreamSource pathSource(toSVGPath(svgValue).byteStream());
    size_t length = 0;
    PathCoordinates currentCoordinates;
    Vector<OwnPtr<InterpolableValue>> interpolablePathSegs;
    Vector<SVGPathSegType> pathSegTypes;

    while (pathSource.hasMoreData()) {
        const PathSegmentData segment = pathSource.parseSegment();
        interpolablePathSegs.append(SVGPathSegInterpolationFunctions::consumePathSeg(segment, currentCoordinates));
        pathSegTypes.append(segment.command);
        length++;
    }

    OwnPtr<InterpolableList> pathArgs = InterpolableList::create(length);
    for (size_t i = 0; i < interpolablePathSegs.size(); i++)
        pathArgs->set(i, interpolablePathSegs[i].release());

    OwnPtr<InterpolableList> result = InterpolableList::create(PathComponentIndexCount);
    result->set(PathArgsIndex, pathArgs.release());
    result->set(PathNeutralIndex, InterpolableNumber::create(0));

    return InterpolationValue::create(*this, result.release(), SVGPathNonInterpolableValue::create(pathSegTypes));
}

class UnderlyingPathSegTypesChecker : public InterpolationType::ConversionChecker {
public:
    ~UnderlyingPathSegTypesChecker() final {}

    static PassOwnPtr<UnderlyingPathSegTypesChecker> create(const InterpolationType& type, const UnderlyingValue& underlyingValue)
    {
        return adoptPtr(new UnderlyingPathSegTypesChecker(type, getPathSegTypes(underlyingValue)));
    }

private:
    UnderlyingPathSegTypesChecker(const InterpolationType& type, const Vector<SVGPathSegType>& pathSegTypes)
        : ConversionChecker(type)
        , m_pathSegTypes(pathSegTypes)
    { }

    static const Vector<SVGPathSegType>& getPathSegTypes(const UnderlyingValue& underlyingValue)
    {
        return toSVGPathNonInterpolableValue(underlyingValue->nonInterpolableValue())->pathSegTypes();
    }

    bool isValid(const InterpolationEnvironment&, const UnderlyingValue& underlyingValue) const final
    {
        return m_pathSegTypes == getPathSegTypes(underlyingValue);
    }

    Vector<SVGPathSegType> m_pathSegTypes;
};

PassOwnPtr<InterpolationValue> SVGPathInterpolationType::maybeConvertNeutral(const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
{
    conversionCheckers.append(UnderlyingPathSegTypesChecker::create(*this, underlyingValue));
    OwnPtr<InterpolableList> result = InterpolableList::create(PathComponentIndexCount);
    result->set(PathArgsIndex, toInterpolableList(underlyingValue->interpolableValue()).get(PathArgsIndex)->cloneAndZero());
    result->set(PathNeutralIndex, InterpolableNumber::create(1));
    return InterpolationValue::create(*this, result.release(),
        const_cast<NonInterpolableValue*>(underlyingValue->nonInterpolableValue())); // Take ref.
}

static bool pathSegTypesMatch(const Vector<SVGPathSegType>& a, const Vector<SVGPathSegType>& b)
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); i++) {
        if (toAbsolutePathSegType(a[i]) != toAbsolutePathSegType(b[i]))
            return false;
    }

    return true;
}

PassOwnPtr<PairwisePrimitiveInterpolation> SVGPathInterpolationType::mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
{
    const Vector<SVGPathSegType>& startTypes = toSVGPathNonInterpolableValue(startValue.nonInterpolableValue())->pathSegTypes();
    const Vector<SVGPathSegType>& endTypes = toSVGPathNonInterpolableValue(endValue.nonInterpolableValue())->pathSegTypes();
    if (!pathSegTypesMatch(startTypes, endTypes))
        return nullptr;

    return PairwisePrimitiveInterpolation::create(*this,
        startValue.mutableComponent().interpolableValue.release(),
        endValue.mutableComponent().interpolableValue.release(),
        const_cast<NonInterpolableValue*>(endValue.nonInterpolableValue())); // Take ref.
}

void SVGPathInterpolationType::composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
{
    const InterpolableList& list = toInterpolableList(value.interpolableValue());
    double neutralComponent = toInterpolableNumber(list.get(PathNeutralIndex))->value();

    if (neutralComponent == 0) {
        underlyingValue.set(&value);
        return;
    }

    ASSERT(pathSegTypesMatch(
        toSVGPathNonInterpolableValue(underlyingValue->nonInterpolableValue())->pathSegTypes(),
        toSVGPathNonInterpolableValue(value.nonInterpolableValue())->pathSegTypes()));
    underlyingValue.mutableComponent().interpolableValue->scaleAndAdd(neutralComponent, value.interpolableValue());
    underlyingValue.mutableComponent().nonInterpolableValue = const_cast<NonInterpolableValue*>(value.nonInterpolableValue()); // Take ref.
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGPathInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue) const
{
    RefPtr<SVGPathByteStream> pathByteStream = SVGPathByteStream::create();
    InterpolatedSVGPathSource source(
        toInterpolableList(*toInterpolableList(interpolableValue).get(PathArgsIndex)),
        toSVGPathNonInterpolableValue(nonInterpolableValue)->pathSegTypes());
    SVGPathByteStreamBuilder builder(*pathByteStream);
    SVGPathParser(&source, &builder).parsePathDataFromSource(UnalteredParsing, false);
    return SVGPath::create(CSSPathValue::create(pathByteStream.release()));
}

} // namespace blink
