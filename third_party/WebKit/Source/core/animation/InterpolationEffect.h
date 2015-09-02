// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationEffect_h
#define InterpolationEffect_h

#include "core/CoreExport.h"
#include "core/animation/Interpolation.h"
#include "core/animation/Keyframe.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/animation/TimingFunction.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class CORE_EXPORT InterpolationEffect : public GarbageCollected<InterpolationEffect> {
public:
    static InterpolationEffect* create()
    {
        return new InterpolationEffect();
    }

    void getActiveInterpolations(double fraction, double iterationDuration, OwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation>>>&) const;

    void addInterpolation(PassRefPtrWillBeRawPtr<Interpolation> interpolation, PassRefPtr<TimingFunction> easing, double start, double end, double applyFrom, double applyTo)
    {
        m_interpolations.append(InterpolationRecord::create(interpolation, easing, start, end, applyFrom, applyTo));
    }

    void addInterpolationsFromKeyframes(PropertyHandle, Element*, const ComputedStyle* baseStyle, Keyframe::PropertySpecificKeyframe& keyframeA, Keyframe::PropertySpecificKeyframe& keyframeB, double applyFrom, double applyTo);

    template<typename T>
    inline void forEachInterpolation(const T& callback)
    {
        for (auto& record : m_interpolations)
            callback(*record->m_interpolation);
    }

    DECLARE_TRACE();

private:
    InterpolationEffect()
    {
    }

    class InterpolationRecord : public GarbageCollectedFinalized<InterpolationRecord> {
    public:
        RefPtrWillBeMember<Interpolation> m_interpolation;
        RefPtr<TimingFunction> m_easing;
        double m_start;
        double m_end;
        double m_applyFrom;
        double m_applyTo;

        static InterpolationRecord* create(PassRefPtrWillBeRawPtr<Interpolation> interpolation, PassRefPtr<TimingFunction> easing, double start, double end, double applyFrom, double applyTo)
        {
            return new InterpolationRecord(interpolation, easing, start, end, applyFrom, applyTo);
        }

        DECLARE_TRACE();

    private:
        InterpolationRecord(PassRefPtrWillBeRawPtr<Interpolation> interpolation, PassRefPtr<TimingFunction> easing, double start, double end, double applyFrom, double applyTo)
            : m_interpolation(interpolation)
            , m_easing(easing)
            , m_start(start)
            , m_end(end)
            , m_applyFrom(applyFrom)
            , m_applyTo(applyTo)
        {
        }
    };

    HeapVector<Member<InterpolationRecord>> m_interpolations;
};

} // namespace blink

#endif // InterpolationEffect_h
