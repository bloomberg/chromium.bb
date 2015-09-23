// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/CSSValueInterpolationType.h"

#include "core/css/resolver/StyleBuilder.h"

namespace blink {

class CSSValueNonInterpolableValue : public NonInterpolableValue {
public:
    ~CSSValueNonInterpolableValue() override { }
    static PassRefPtrWillBeRawPtr<CSSValueNonInterpolableValue> create(PassRefPtrWillBeRawPtr<CSSValue> cssValue)
    {
        return adoptRefWillBeNoop(new CSSValueNonInterpolableValue(cssValue));
    }

    CSSValue* cssValue() const { return m_cssValue.get(); }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        NonInterpolableValue::trace(visitor);
        visitor->trace(m_cssValue);
    }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    CSSValueNonInterpolableValue(PassRefPtrWillBeRawPtr<CSSValue> cssValue)
        : m_cssValue(cssValue)
    { }

    RefPtrWillBeMember<CSSValue> m_cssValue;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSValueNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSValueNonInterpolableValue);

PassOwnPtr<InterpolationValue> CSSValueInterpolationType::maybeConvertSingle(const CSSPropertySpecificKeyframe& keyframe, const StyleResolverState*, ConversionCheckers&) const
{
    return InterpolationValue::create(*this, InterpolableList::create(0), CSSValueNonInterpolableValue::create(keyframe.value()));
}

void CSSValueInterpolationType::apply(const InterpolableValue&, const NonInterpolableValue* nonInterpolableValue, StyleResolverState& state) const
{
    CSSValue* value = toCSSValueNonInterpolableValue(nonInterpolableValue)->cssValue();
    if (value)
        StyleBuilder::applyProperty(m_property, state, value);
}

} // namespace blink
