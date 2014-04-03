// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Interpolation_h
#define Interpolation_h

#include "CSSPropertyNames.h"
#include "core/animation/InterpolableValue.h"
#include "heap/Handle.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class StyleResolverState;

class Interpolation : public RefCountedWillBeGarbageCollected<Interpolation> {
    DECLARE_EMPTY_VIRTUAL_DESTRUCTOR_WILL_BE_REMOVED(Interpolation);
public:
    static PassRefPtrWillBeRawPtr<Interpolation> create(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end)
    {
        return adoptRefWillBeNoop(new Interpolation(start, end));
    }

    void interpolate(int iteration, double fraction) const;

    virtual bool isStyleInterpolation() const { return false; }
    virtual bool isLegacyStyleInterpolation() const { return false; }

    virtual void trace(Visitor*);

protected:
    const OwnPtrWillBeMember<InterpolableValue> m_start;
    const OwnPtrWillBeMember<InterpolableValue> m_end;

    mutable double m_cachedFraction;
    mutable int m_cachedIteration;
    mutable OwnPtrWillBeMember<InterpolableValue> m_cachedValue;

    Interpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end);

private:
    InterpolableValue* getCachedValueForTesting() const { return m_cachedValue.get(); }

    friend class AnimationInterpolableValueTest;
    friend class AnimationInterpolationEffectTest;
};

class StyleInterpolation : public Interpolation {
public:
    // 1) convert m_cachedValue into an X
    // 2) shove X into StyleResolverState
    // X can be:
    // (1) a CSSValue (and applied via StyleBuilder::applyProperty)
    // (2) an AnimatableValue (and applied via // AnimatedStyleBuilder::applyProperty)
    // (3) a custom value that is inserted directly into the StyleResolverState.
    virtual void apply(StyleResolverState&) const = 0;

    virtual bool isStyleInterpolation() const OVERRIDE FINAL { return true; }

    CSSPropertyID id() const { return m_id; }

    virtual void trace(Visitor*) OVERRIDE;

protected:
    CSSPropertyID m_id;

    StyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, CSSPropertyID id)
        : Interpolation(start, end)
        , m_id(id)
    {
    }
};

class LegacyStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtrWillBeRawPtr<LegacyStyleInterpolation> create(PassRefPtrWillBeRawPtr<AnimatableValue> start, PassRefPtrWillBeRawPtr<AnimatableValue> end, CSSPropertyID id)
    {
        return adoptRefWillBeNoop(new LegacyStyleInterpolation(InterpolableAnimatableValue::create(start), InterpolableAnimatableValue::create(end), id));
    }

    virtual void apply(StyleResolverState&) const;

    virtual bool isLegacyStyleInterpolation() const OVERRIDE FINAL { return true; }
    PassRefPtrWillBeRawPtr<AnimatableValue> currentValue() const
    {
        InterpolableAnimatableValue* value = static_cast<InterpolableAnimatableValue*>(m_cachedValue.get());
        return value->value();
    }

    virtual void trace(Visitor*) OVERRIDE;

private:
    LegacyStyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, CSSPropertyID id)
        : StyleInterpolation(start, end, id)
    {
    }
};

DEFINE_TYPE_CASTS(StyleInterpolation, Interpolation, value, value->isStyleInterpolation(), value.isStyleInterpolation());
DEFINE_TYPE_CASTS(LegacyStyleInterpolation, Interpolation, value, value->isLegacyStyleInterpolation(), value.isLegacyStyleInterpolation());

}
#endif
