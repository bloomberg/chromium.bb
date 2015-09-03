// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SampledEffect_h
#define SampledEffect_h

#include "core/animation/Animation.h"
#include "core/animation/Interpolation.h"
#include "core/animation/KeyframeEffect.h"
#include "wtf/Allocator.h"
#include "wtf/BitArray.h"
#include "wtf/Vector.h"

namespace blink {

class SVGElement;

// TODO(haraken): Drop Finalized once we ship Oilpan and the OwnPtrWillBeMember
// is gone.
class SampledEffect : public GarbageCollectedFinalized<SampledEffect> {
    WTF_MAKE_NONCOPYABLE(SampledEffect);
public:
    static SampledEffect* create(KeyframeEffect* animation, PassOwnPtr<Vector<RefPtr<Interpolation>>> interpolations)
    {
        return new SampledEffect(animation, interpolations);
    }

    void clear();

    const Vector<RefPtr<Interpolation>>& interpolations() const { return *m_interpolations; }
    PassOwnPtr<Vector<RefPtr<Interpolation>>> mutableInterpolations() { return m_interpolations.release(); }

    void setInterpolations(PassOwnPtr<Vector<RefPtr<Interpolation>>> interpolations) { m_interpolations = interpolations; }

    KeyframeEffect* effect() const { return m_effect; }
    unsigned sequenceNumber() const { return m_sequenceNumber; }
    KeyframeEffect::Priority priority() const { return m_priority; }

    DECLARE_TRACE();

    void applySVGUpdate(SVGElement&);

private:
    SampledEffect(KeyframeEffect*, PassOwnPtr<Vector<RefPtr<Interpolation>>>);

    WeakMember<KeyframeEffect> m_effect;
    Member<Animation> m_animation;
    OwnPtr<Vector<RefPtr<Interpolation>>> m_interpolations;
    const unsigned m_sequenceNumber;
    KeyframeEffect::Priority m_priority;
};

} // namespace blink

#endif // SampledEffect_h
