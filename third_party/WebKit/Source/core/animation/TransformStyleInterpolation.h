// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformStyleInterpolation_h
#define TransformStyleInterpolation_h

#include "core/animation/StyleInterpolation.h"
#include "core/css/CSSTransformValue.h"

namespace blink {

class TransformStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtrWillBeRawPtr<TransformStyleInterpolation> maybeCreateFrom(CSSValue& start, CSSValue& end, CSSPropertyID id)
    {
        if (!canCreateFrom(start) || !canCreateFrom(end))
            return nullptr;
        ASSERT(!TransformStyleInterpolation::fallBackToLegacy(start, end));
        Vector<CSSTransformValue::TransformOperationType> types;
        if (start.isPrimitiveValue() && end.isPrimitiveValue())
            return adoptRefWillBeNoop(new TransformStyleInterpolation(transformToInterpolableValue(start), transformToInterpolableValue(end), id, types));
        const CSSValueList* startList = toCSSValueList(&start);

        types.reserveCapacity(startList->length());
        for (size_t i = 0; i < startList->length(); i++) {
            types.append(toCSSTransformValue(startList->item(i))->operationType());
        }
        return adoptRefWillBeNoop(new TransformStyleInterpolation(transformToInterpolableValue(start), transformToInterpolableValue(end), id, types));
    }

    static bool fallBackToLegacy(CSSValue&, CSSValue&);

    virtual void apply(StyleResolverState&) const override;
    virtual void trace(Visitor*) override;

private:
    TransformStyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, CSSPropertyID id, Vector<CSSTransformValue::TransformOperationType> types)
        : StyleInterpolation(start, end, id)
        , m_types(types)
    { }

    static bool canCreateFrom(const CSSValue&);

    static PassOwnPtrWillBeRawPtr<InterpolableValue> transformToInterpolableValue(CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToTransform(InterpolableValue*, Vector<CSSTransformValue::TransformOperationType> types);

    Vector<CSSTransformValue::TransformOperationType> m_types;

    friend class AnimationTransformStyleInterpolationTest;
};

} // namespace blink

#endif // TransformStyleInterpolation_h

