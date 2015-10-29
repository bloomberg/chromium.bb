// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Interpolation_h
#define Interpolation_h

#include "core/CoreExport.h"
#include "core/animation/InterpolableValue.h"
#include "platform/heap/Handle.h"

namespace blink {

class PropertyHandle;

class CORE_EXPORT Interpolation : public RefCounted<Interpolation> {
    WTF_MAKE_NONCOPYABLE(Interpolation);
public:
    virtual ~Interpolation();

    virtual void interpolate(int iteration, double fraction);

    virtual bool isStyleInterpolation() const { return false; }
    virtual bool isInvalidatableInterpolation() const { return false; }
    virtual bool isLegacyStyleInterpolation() const { return false; }
    virtual bool isSVGInterpolation() const { return false; }

    virtual PropertyHandle property() const = 0;

protected:
    const OwnPtr<InterpolableValue> m_start;
    const OwnPtr<InterpolableValue> m_end;

    mutable double m_cachedFraction;
    mutable int m_cachedIteration;
    mutable OwnPtr<InterpolableValue> m_cachedValue;

    Interpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end);

private:
    InterpolableValue* getCachedValueForTesting() const { return m_cachedValue.get(); }

    friend class AnimationInterpolableValueTest;
    friend class AnimationInterpolationEffectTest;
    friend class AnimationDoubleStyleInterpolationTest;
    friend class AnimationVisibilityStyleInterpolationTest;
    friend class AnimationColorStyleInterpolationTest;
    friend class AnimationSVGStrokeDasharrayStyleInterpolationTest;
};

using ActiveInterpolations = Vector<RefPtr<Interpolation>, 1>;

} // namespace blink

#endif // Interpolation_h
