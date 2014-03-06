// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Interpolation_h
#define Interpolation_h

#include "core/animation/InterpolableValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class Interpolation : public RefCounted<Interpolation> {
public:
    static PassRefPtr<Interpolation> create(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end)
    {
        return adoptRef(new Interpolation(start, end));
    }

    void interpolate(double fraction) const;

protected:
    const OwnPtr<InterpolableValue> m_start;
    const OwnPtr<InterpolableValue> m_end;

    mutable double m_cachedFraction;
    mutable OwnPtr<InterpolableValue> m_cachedValue;

    Interpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end);

private:
    InterpolableValue* getCachedValueForTesting() const { return m_cachedValue.get(); }

    friend class AnimationInterpolableValueTest;
};

class StyleInterpolation : public Interpolation {
public:
    // 1) convert m_cachedValue into an X
    // 2) shove X into StyleResolverState
    // X can be:
    // (1) a CSSValue (and applied via StyleBuilder::applyProperty)
    // (2) an AnimatableValue (and applied via // AnimatedStyleBuilder::applyProperty)
    // (3) a custom value that is inserted directly into the StyleResolverState.
    virtual void apply(StyleResolverState&) = 0;

protected:
    CSSPropertyID m_id;

    StyleInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, CSSPropertyID id)
        : Interpolation(start, end)
        , m_id(id)
    { }
};

}
#endif
