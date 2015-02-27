// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Interpolation_h
#define Interpolation_h

#include "core/animation/InterpolableValue.h"
#include "platform/heap/Handle.h"

namespace blink {

class Interpolation : public RefCountedWillBeGarbageCollectedFinalized<Interpolation> {
public:
    static PassRefPtrWillBeRawPtr<Interpolation> create(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end)
    {
        return adoptRefWillBeNoop(new Interpolation(start, end));
    }

    virtual ~Interpolation();

    void interpolate(int iteration, double fraction) const;

    virtual bool isStyleInterpolation() const { return false; }
    virtual bool isLegacyStyleInterpolation() const { return false; }

    DECLARE_VIRTUAL_TRACE();

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
    friend class AnimationDoubleStyleInterpolationTest;
    friend class AnimationVisibilityStyleInterpolationTest;
    friend class AnimationColorStyleInterpolationTest;
    friend class AnimationSVGStrokeDasharrayStyleInterpolationTest;
};

} // namespace blink

#endif // Interpolation_h
