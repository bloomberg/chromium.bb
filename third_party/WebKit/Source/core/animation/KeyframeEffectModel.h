/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef KeyframeEffectModel_h
#define KeyframeEffectModel_h

#include "core/animation/AnimatableValue.h"
#include "core/animation/AnimationEffect.h"
#include "core/animation/InterpolationEffect.h"
#include "heap/Handle.h"
#include "platform/animation/TimingFunction.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

typedef HashSet<CSSPropertyID> PropertySet;

class KeyframeEffectModelTest;

// Represents the keyframes set through the API.
class Keyframe : public RefCountedWillBeGarbageCollectedFinalized<Keyframe> {
public:
    static PassRefPtrWillBeRawPtr<Keyframe> create()
    {
        return adoptRefWillBeNoop(new Keyframe);
    }
    static bool compareOffsets(const RefPtrWillBeRawPtr<Keyframe>& a, const RefPtrWillBeRawPtr<Keyframe>& b)
    {
        return a->offset() < b->offset();
    }
    void setOffset(double offset) { m_offset = offset; }
    double offset() const { return m_offset; }
    void setComposite(AnimationEffect::CompositeOperation composite) { m_composite = composite; }
    AnimationEffect::CompositeOperation composite() const { return m_composite; }
    void setEasing(PassRefPtr<TimingFunction>);
    TimingFunction* easing() const { return m_easing.get(); }
    void setPropertyValue(CSSPropertyID, const AnimatableValue*);
    void clearPropertyValue(CSSPropertyID);
    const AnimatableValue* propertyValue(CSSPropertyID) const;
    PropertySet properties() const;
    PassRefPtrWillBeRawPtr<Keyframe> clone() const { return adoptRefWillBeNoop(new Keyframe(*this)); }
    PassRefPtrWillBeRawPtr<Keyframe> cloneWithOffset(double offset) const;

    void trace(Visitor*);

private:
    Keyframe();
    Keyframe(const Keyframe&);
    double m_offset;
    AnimationEffect::CompositeOperation m_composite;
    RefPtr<TimingFunction> m_easing;
    typedef WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<AnimatableValue> > PropertyValueMap;
    PropertyValueMap m_propertyValues;
};

class KeyframeEffectModel FINAL : public AnimationEffect {
public:
    class PropertySpecificKeyframe;
    typedef WillBeHeapVector<RefPtrWillBeMember<Keyframe> > KeyframeVector;
    typedef WillBeHeapVector<OwnPtrWillBeMember<PropertySpecificKeyframe> > PropertySpecificKeyframeVector;
    // FIXME: Implement accumulation.
    static PassRefPtrWillBeRawPtr<KeyframeEffectModel> create(const KeyframeVector& keyframes)
    {
        return adoptRefWillBeNoop(new KeyframeEffectModel(keyframes));
    }

    virtual bool affects(CSSPropertyID property) OVERRIDE
    {
        ensureKeyframeGroups();
        return m_keyframeGroups->contains(property);
    }

    // AnimationEffect implementation.
    virtual PassOwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation> > > sample(int iteration, double fraction, double iterationDuration) const OVERRIDE;

    // FIXME: Implement setFrames()
    const KeyframeVector& getFrames() const { return m_keyframes; }

    virtual bool isKeyframeEffectModel() const OVERRIDE { return true; }

    bool isReplaceOnly();

    PropertySet properties() const;

    class PropertySpecificKeyframe : public NoBaseWillBeGarbageCollectedFinalized<PropertySpecificKeyframe> {
    public:
        PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, const AnimatableValue*, CompositeOperation);
        double offset() const { return m_offset; }
        TimingFunction* easing() const { return m_easing.get(); }
        const AnimatableValue* value() const { return m_value.get(); }
        AnimationEffect::CompositeOperation composite() const { return m_composite; }
        PassOwnPtrWillBeRawPtr<PropertySpecificKeyframe> cloneWithOffset(double offset) const;

        void trace(Visitor*);

    private:
        // Used by cloneWithOffset().
        PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, PassRefPtrWillBeRawPtr<AnimatableValue>, CompositeOperation);
        double m_offset;
        RefPtr<TimingFunction> m_easing;
        RefPtrWillBeMember<AnimatableValue> m_value;
        AnimationEffect::CompositeOperation m_composite;
    };

    class PropertySpecificKeyframeGroup : public NoBaseWillBeGarbageCollectedFinalized<PropertySpecificKeyframeGroup> {
    public:
        void appendKeyframe(PassOwnPtrWillBeRawPtr<PropertySpecificKeyframe>);
        const PropertySpecificKeyframeVector& keyframes() const { return m_keyframes; }

        void trace(Visitor*);

    private:
        PropertySpecificKeyframeVector m_keyframes;
        void removeRedundantKeyframes();
        void addSyntheticKeyframeIfRequired();

        friend class KeyframeEffectModel;
    };

    const PropertySpecificKeyframeVector& getPropertySpecificKeyframes(CSSPropertyID id) const
    {
        ensureKeyframeGroups();
        return m_keyframeGroups->get(id)->keyframes();
    }

    virtual void trace(Visitor*) OVERRIDE;

private:
    KeyframeEffectModel(const KeyframeVector& keyframes);

    static KeyframeVector normalizedKeyframes(const KeyframeVector& keyframes);

    // Lazily computes the groups of property-specific keyframes.
    void ensureKeyframeGroups() const;
    void ensureInterpolationEffect() const;

    KeyframeVector m_keyframes;
    // The spec describes filtering the normalized keyframes at sampling time
    // to get the 'property-specific keyframes'. For efficiency, we cache the
    // property-specific lists.
    typedef WillBeHeapHashMap<CSSPropertyID, OwnPtrWillBeMember<PropertySpecificKeyframeGroup> > KeyframeGroupMap;
    mutable OwnPtrWillBeMember<KeyframeGroupMap> m_keyframeGroups;

    mutable RefPtrWillBeMember<InterpolationEffect> m_interpolationEffect;

    friend class KeyframeEffectModelTest;
};

DEFINE_TYPE_CASTS(KeyframeEffectModel, AnimationEffect, value, value->isKeyframeEffectModel(), value.isKeyframeEffectModel());

} // namespace WebCore

#endif // KeyframeEffectModel_h
