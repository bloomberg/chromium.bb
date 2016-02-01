// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/PerspectiveTransformComponent.h"

#include "bindings/core/v8/ExceptionState.h"

namespace blink {

PerspectiveTransformComponent* PerspectiveTransformComponent::create(const LengthValue* length, ExceptionState& exceptionState)
{
    if (length->containsPercent()) {
        exceptionState.throwTypeError("PerspectiveTransformComponent does not support LengthValues with percent units");
        return nullptr;
    }
    return new PerspectiveTransformComponent(length);
}

PassRefPtrWillBeRawPtr<CSSFunctionValue> PerspectiveTransformComponent::toCSSValue() const
{
    RefPtrWillBeRawPtr<CSSFunctionValue> result = CSSFunctionValue::create(CSSValuePerspective);
    result->append(m_length->toCSSValue());
    return result.release();
}

} // namespace blink
