// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShadowStyleInterpolation_h
#define ShadowStyleInterpolation_h

#include "core/animation/StyleInterpolation.h"
#include "core/css/CSSShadowValue.h"
#include "core/rendering/style/RenderStyle.h"
#include "platform/Length.h"

namespace blink {

class ShadowStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtrWillBeRawPtr<ShadowStyleInterpolation> create(const CSSValue& start, const CSSValue& end, CSSPropertyID id, bool styleFlag)
    {
        return adoptRefWillBeNoop(new ShadowStyleInterpolation(shadowToInterpolableValue(start), shadowToInterpolableValue(end), id, styleFlag));
    }

    static bool canCreateFrom(const CSSValue&);

    virtual void apply(StyleResolverState&) const override;
    virtual void trace(Visitor*);

    static PassRefPtrWillBeRawPtr<ShadowStyleInterpolation> maybeCreateFromShadow(const CSSValue& start, const CSSValue& end, CSSPropertyID);
private:
    ShadowStyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, CSSPropertyID id, bool styleFlag)
        : StyleInterpolation(start, end, id), m_styleFlag(styleFlag)
    { }

    bool m_styleFlag;

    static PassOwnPtrWillBeRawPtr<InterpolableValue> shadowToInterpolableValue(const CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToShadow(InterpolableValue*, bool styleFlag);

    friend class AnimationShadowStyleInterpolationTest;
};

} // namespace blink

#endif // ShadowStyleInterpolation_h
