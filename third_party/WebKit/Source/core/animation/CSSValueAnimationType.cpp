// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/CSSValueAnimationType.h"

#include "core/css/resolver/StyleBuilder.h"

namespace blink {

PassOwnPtrWillBeRawPtr<AnimationValue> CSSValueAnimationType::maybeConvertSingle(const CSSPropertySpecificKeyframe& keyframe, const StyleResolverState*, ConversionCheckers&) const
{
    return AnimationValue::create(*this, InterpolableList::create(0), DefaultNonInterpolableValue::create(keyframe.value()));
}

void CSSValueAnimationType::apply(const InterpolableValue&, const NonInterpolableValue* nonInterpolableValue, StyleResolverState& state) const
{
    StyleBuilder::applyProperty(m_property, state, toDefaultNonInterpolableValue(nonInterpolableValue)->cssValue());
}

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(DefaultNonInterpolableValue);

} // namespace blink
