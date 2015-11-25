// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/SVGTransformListInterpolationType.h"

#include "core/animation/InterpolableValue.h"
#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/NonInterpolableValue.h"
#include "core/svg/SVGTransform.h"
#include "core/svg/SVGTransformList.h"

namespace blink {

class SVGTransformNonInterpolableValue : public NonInterpolableValue {
public:
    virtual ~SVGTransformNonInterpolableValue() {}

    static PassRefPtr<SVGTransformNonInterpolableValue> create(Vector<SVGTransformType>& transformTypes)
    {
        return adoptRef(new SVGTransformNonInterpolableValue(transformTypes));
    }

    const Vector<SVGTransformType>& transformTypes() const { return m_transformTypes; }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    SVGTransformNonInterpolableValue(Vector<SVGTransformType>& transformTypes)
    {
        m_transformTypes.swap(transformTypes);
    }

    Vector<SVGTransformType> m_transformTypes;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(SVGTransformNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(SVGTransformNonInterpolableValue);

namespace {

PassOwnPtr<InterpolableValue> translateToInterpolableValue(SVGTransform* transform)
{
    FloatPoint translate = transform->translate();
    OwnPtr<InterpolableList> result = InterpolableList::create(2);
    result->set(0, InterpolableNumber::create(translate.x()));
    result->set(1, InterpolableNumber::create(translate.y()));
    return result.release();
}

PassRefPtrWillBeRawPtr<SVGTransform> translateFromInterpolableValue(const InterpolableValue& value)
{
    const InterpolableList& list = toInterpolableList(value);

    RefPtrWillBeRawPtr<SVGTransform> transform = SVGTransform::create(SVG_TRANSFORM_TRANSLATE);
    transform->setTranslate(
        toInterpolableNumber(list.get(0))->value(),
        toInterpolableNumber(list.get(1))->value());
    return transform.release();
}

PassOwnPtr<InterpolableValue> scaleToInterpolableValue(SVGTransform* transform)
{
    FloatSize scale = transform->scale();
    OwnPtr<InterpolableList> result = InterpolableList::create(2);
    result->set(0, InterpolableNumber::create(scale.width()));
    result->set(1, InterpolableNumber::create(scale.height()));
    return result.release();
}

PassRefPtrWillBeRawPtr<SVGTransform> scaleFromInterpolableValue(const InterpolableValue& value)
{
    const InterpolableList& list = toInterpolableList(value);

    RefPtrWillBeRawPtr<SVGTransform> transform = SVGTransform::create(SVG_TRANSFORM_SCALE);
    transform->setScale(
        toInterpolableNumber(list.get(0))->value(),
        toInterpolableNumber(list.get(1))->value());
    return transform.release();
}

PassOwnPtr<InterpolableValue> rotateToInterpolableValue(SVGTransform* transform)
{
    FloatPoint rotationCenter = transform->rotationCenter();
    OwnPtr<InterpolableList> result = InterpolableList::create(3);
    result->set(0, InterpolableNumber::create(transform->angle()));
    result->set(1, InterpolableNumber::create(rotationCenter.x()));
    result->set(2, InterpolableNumber::create(rotationCenter.y()));
    return result.release();
}

PassRefPtrWillBeRawPtr<SVGTransform> rotateFromInterpolableValue(const InterpolableValue& value)
{
    const InterpolableList& list = toInterpolableList(value);

    RefPtrWillBeRawPtr<SVGTransform> transform = SVGTransform::create(SVG_TRANSFORM_ROTATE);
    transform->setRotate(
        toInterpolableNumber(list.get(0))->value(),
        toInterpolableNumber(list.get(1))->value(),
        toInterpolableNumber(list.get(2))->value());
    return transform.release();
}

PassOwnPtr<InterpolableValue> skewXToInterpolableValue(SVGTransform* transform)
{
    return InterpolableNumber::create(transform->angle());
}

PassRefPtrWillBeRawPtr<SVGTransform> skewXFromInterpolableValue(const InterpolableValue& value)
{
    RefPtrWillBeRawPtr<SVGTransform> transform = SVGTransform::create(SVG_TRANSFORM_SKEWX);
    transform->setSkewX(toInterpolableNumber(value).value());
    return transform.release();
}

PassOwnPtr<InterpolableValue> skewYToInterpolableValue(SVGTransform* transform)
{
    return InterpolableNumber::create(transform->angle());
}

PassRefPtrWillBeRawPtr<SVGTransform> skewYFromInterpolableValue(const InterpolableValue& value)
{
    RefPtrWillBeRawPtr<SVGTransform> transform = SVGTransform::create(SVG_TRANSFORM_SKEWY);
    transform->setSkewY(toInterpolableNumber(value).value());
    return transform.release();
}

PassOwnPtr<InterpolableValue> toInterpolableValue(SVGTransform* transform, SVGTransformType transformType)
{
    switch (transformType) {
    case SVG_TRANSFORM_TRANSLATE:
        return translateToInterpolableValue(transform);
    case SVG_TRANSFORM_SCALE:
        return scaleToInterpolableValue(transform);
    case SVG_TRANSFORM_ROTATE:
        return rotateToInterpolableValue(transform);
    case SVG_TRANSFORM_SKEWX:
        return skewXToInterpolableValue(transform);
    case SVG_TRANSFORM_SKEWY:
        return skewYToInterpolableValue(transform);
    case SVG_TRANSFORM_MATRIX:
    case SVG_TRANSFORM_UNKNOWN:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

PassRefPtrWillBeRawPtr<SVGTransform> fromInterpolableValue(const InterpolableValue& value, SVGTransformType transformType)
{
    switch (transformType) {
    case SVG_TRANSFORM_TRANSLATE:
        return translateFromInterpolableValue(value);
    case SVG_TRANSFORM_SCALE:
        return scaleFromInterpolableValue(value);
    case SVG_TRANSFORM_ROTATE:
        return rotateFromInterpolableValue(value);
    case SVG_TRANSFORM_SKEWX:
        return skewXFromInterpolableValue(value);
    case SVG_TRANSFORM_SKEWY:
        return skewYFromInterpolableValue(value);
    case SVG_TRANSFORM_MATRIX:
    case SVG_TRANSFORM_UNKNOWN:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool transformTypesMatch(const InterpolationValue& first, const InterpolationValue& second)
{
    const Vector<SVGTransformType>& firstTransformTypes = toSVGTransformNonInterpolableValue(first.nonInterpolableValue())->transformTypes();
    const Vector<SVGTransformType>& secondTransformTypes = toSVGTransformNonInterpolableValue(second.nonInterpolableValue())->transformTypes();
    return firstTransformTypes == secondTransformTypes;
}

class UnderlyingTypesChecker : public InterpolationType::ConversionChecker {
public:
    static PassOwnPtr<UnderlyingTypesChecker> create(const InterpolationType& type, const Vector<SVGTransformType>& underlyingTypes)
    {
        return adoptPtr(new UnderlyingTypesChecker(type, underlyingTypes));
    }

    bool isValid(const InterpolationEnvironment&, const UnderlyingValue& underlyingValue) const final
    {
        return m_underlyingTypes == toSVGTransformNonInterpolableValue(underlyingValue->nonInterpolableValue())->transformTypes();
    }

private:
    UnderlyingTypesChecker(const InterpolationType& type, const Vector<SVGTransformType>& underlyingTypes)
        : ConversionChecker(type)
        , m_underlyingTypes(underlyingTypes)
    {}

    Vector<SVGTransformType> m_underlyingTypes;
};

} // namespace

PassOwnPtr<InterpolationValue> SVGTransformListInterpolationType::maybeConvertNeutral(const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
{
    Vector<SVGTransformType> underlyingTypes(toSVGTransformNonInterpolableValue(underlyingValue->nonInterpolableValue())->transformTypes());
    conversionCheckers.append(UnderlyingTypesChecker::create(*this, underlyingTypes));
    if (underlyingTypes.isEmpty())
        return nullptr;
    OwnPtr<InterpolationValue> result = underlyingValue->clone();
    result->mutableComponent().interpolableValue = result->interpolableValue().cloneAndZero();
    return result.release();
}

PassOwnPtr<InterpolationValue> SVGTransformListInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedTransformList)
        return nullptr;

    const SVGTransformList& svgList = toSVGTransformList(svgValue);
    OwnPtr<InterpolableList> result = InterpolableList::create(svgList.length());

    Vector<SVGTransformType> transformTypes;
    for (size_t i = 0; i < svgList.length(); i++) {
        const SVGTransform* transform = svgList.at(i);
        SVGTransformType transformType(transform->transformType());
        if (transformType == SVG_TRANSFORM_MATRIX) {
            // TODO(ericwilligers): Support matrix interpolation.
            return nullptr;
        }
        result->set(i, toInterpolableValue(transform->clone().get(), transformType));
        transformTypes.append(transformType);
    }
    return InterpolationValue::create(*this, result.release(), SVGTransformNonInterpolableValue::create(transformTypes));
}

PassOwnPtr<InterpolationValue> SVGTransformListInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    return maybeConvertSVGValue(environment.svgBaseValue());
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGTransformListInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue) const
{
    RefPtrWillBeRawPtr<SVGTransformList> result = SVGTransformList::create();
    const InterpolableList& list = toInterpolableList(interpolableValue);
    const Vector<SVGTransformType>& transformTypes = toSVGTransformNonInterpolableValue(nonInterpolableValue)->transformTypes();
    for (size_t i = 0; i < list.length(); ++i)
        result->append(fromInterpolableValue(*list.get(i), transformTypes.at(i)));
    return result.release();
}

PassOwnPtr<PairwisePrimitiveInterpolation> SVGTransformListInterpolationType::mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
{
    if (!transformTypesMatch(startValue, endValue))
        return nullptr;

    return PairwisePrimitiveInterpolation::create(*this,
        startValue.mutableComponent().interpolableValue.release(),
        endValue.mutableComponent().interpolableValue.release(),
        endValue.mutableComponent().nonInterpolableValue.release());
}

void SVGTransformListInterpolationType::composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
{
    // TODO(suzyh): Implement addition
    underlyingValue.set(&value);
}

} // namespace blink
