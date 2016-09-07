// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSTranslateInterpolationType.h"

#include "core/animation/LengthInterpolationFunctions.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

class ParentTranslateChecker : public InterpolationType::ConversionChecker {
public:
    ~ParentTranslateChecker() {}

    static std::unique_ptr<ParentTranslateChecker> create(PassRefPtr<TranslateTransformOperation> parentTranslate)
    {
        return wrapUnique(new ParentTranslateChecker(std::move(parentTranslate)));
    }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        const TransformOperation* parentTranslate = environment.state().parentStyle()->translate();
        if (m_parentTranslate == parentTranslate)
            return true;
        if (!m_parentTranslate || !parentTranslate)
            return false;
        return *m_parentTranslate == *parentTranslate;
    }

private:
    ParentTranslateChecker(PassRefPtr<TranslateTransformOperation> parentTranslate)
        : m_parentTranslate(parentTranslate)
    { }

    RefPtr<TransformOperation> m_parentTranslate;
};

enum TranslateComponentIndex : unsigned {
    TranslateX,
    TranslateY,
    TranslateZ,
    TranslateComponentIndexCount,
};

InterpolationValue createNeutralValue()
{
    std::unique_ptr<InterpolableList> result = InterpolableList::create(TranslateComponentIndexCount);
    result->set(TranslateX, LengthInterpolationFunctions::createNeutralInterpolableValue());
    result->set(TranslateY, LengthInterpolationFunctions::createNeutralInterpolableValue());
    result->set(TranslateZ, LengthInterpolationFunctions::createNeutralInterpolableValue());
    return InterpolationValue(std::move(result));
}

InterpolationValue convertTranslateOperation(const TranslateTransformOperation* translate, double zoom)
{
    if (!translate)
        return createNeutralValue();

    std::unique_ptr<InterpolableList> result = InterpolableList::create(TranslateComponentIndexCount);
    result->set(TranslateX, LengthInterpolationFunctions::maybeConvertLength(translate->x(), zoom).interpolableValue);
    result->set(TranslateY, LengthInterpolationFunctions::maybeConvertLength(translate->y(), zoom).interpolableValue);
    result->set(TranslateZ, LengthInterpolationFunctions::maybeConvertLength(Length(translate->z(), Fixed), zoom).interpolableValue);
    return InterpolationValue(std::move(result));
}

} // namespace

InterpolationValue CSSTranslateInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const
{
    return createNeutralValue();
}

InterpolationValue CSSTranslateInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers&) const
{
    return createNeutralValue();
}

InterpolationValue CSSTranslateInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    TranslateTransformOperation* parentTranslate = state.parentStyle()->translate();
    conversionCheckers.append(ParentTranslateChecker::create(parentTranslate));
    return convertTranslateOperation(parentTranslate, state.parentStyle()->effectiveZoom());
}

InterpolationValue CSSTranslateInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    if (!value.isBaseValueList())
        return nullptr;

    const CSSValueList& list = toCSSValueList(value);
    if (list.length() < 1 || list.length() > 3)
        return nullptr;

    std::unique_ptr<InterpolableList> result = InterpolableList::create(TranslateComponentIndexCount);
    for (size_t i = 0; i < TranslateComponentIndexCount; i++) {
        InterpolationValue component = nullptr;
        if (i < list.length()) {
            component = LengthInterpolationFunctions::maybeConvertCSSValue(list.item(i));
            if (!component)
                return nullptr;
        } else {
            component = InterpolationValue(LengthInterpolationFunctions::createNeutralInterpolableValue());
        }
        result->set(i, std::move(component.interpolableValue));
    }
    return InterpolationValue(std::move(result));
}

InterpolationValue CSSTranslateInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    return convertTranslateOperation(environment.state().style()->translate(), environment.state().style()->effectiveZoom());
}

void CSSTranslateInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue*, InterpolationEnvironment& environment) const
{
    const InterpolableList& list = toInterpolableList(interpolableValue);
    const CSSToLengthConversionData& conversionData = environment.state().cssToLengthConversionData();
    Length x = LengthInterpolationFunctions::createLength(*list.get(TranslateX), nullptr, conversionData, ValueRangeAll);
    Length y = LengthInterpolationFunctions::createLength(*list.get(TranslateY), nullptr, conversionData, ValueRangeAll);
    float z = LengthInterpolationFunctions::createLength(*list.get(TranslateZ), nullptr, conversionData, ValueRangeAll).pixels();

    RefPtr<TranslateTransformOperation> result = nullptr;
    if (!x.isZero() || !y.isZero() || z != 0)
        result = TranslateTransformOperation::create(x, y, z, TransformOperation::Translate3D);
    environment.state().style()->setTranslate(result.release());
}

} // namespace blink
