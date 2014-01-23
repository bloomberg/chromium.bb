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
class Keyframe : public RefCounted<Keyframe> {
public:
    static PassRefPtr<Keyframe> create()
    {
        return adoptRef(new Keyframe);
    }
    static bool compareOffsets(const RefPtr<Keyframe>& a, const RefPtr<Keyframe>& b)
    {
        return a->offset() < b->offset();
    }
    void setOffset(double offset) { m_offset = offset; }
    double offset() const { return m_offset; }
    void setComposite(AnimationEffect::CompositeOperation composite) { m_composite = composite; }
    AnimationEffect::CompositeOperation composite() const { return m_composite; }
    void setPropertyValue(CSSPropertyID, const AnimatableValue*);
    void clearPropertyValue(CSSPropertyID);
    const AnimatableValue* propertyValue(CSSPropertyID) const;
    PropertySet properties() const;
    PassRefPtr<Keyframe> clone() const { return adoptRef(new Keyframe(*this)); }
    PassRefPtr<Keyframe> cloneWithOffset(double offset) const;
private:
    Keyframe();
    Keyframe(const Keyframe&);
    double m_offset;
    AnimationEffect::CompositeOperation m_composite;
    typedef HashMap<CSSPropertyID, RefPtr<AnimatableValue> > PropertyValueMap;
    PropertyValueMap m_propertyValues;
};

class KeyframeEffectModel FINAL : public AnimationEffect {
public:
    class PropertySpecificKeyframe;
    typedef Vector<RefPtr<Keyframe> > KeyframeVector;
    typedef Vector<OwnPtr<PropertySpecificKeyframe> > PropertySpecificKeyframeVector;
    // FIXME: Implement accumulation.
    static PassRefPtr<KeyframeEffectModel> create(const KeyframeVector& keyframes)
    {
        return adoptRef(new KeyframeEffectModel(keyframes));
    }

    virtual bool affects(CSSPropertyID property) OVERRIDE
    {
        ensureKeyframeGroups();
        return m_keyframeGroups->contains(property);
    }

    // AnimationEffect implementation.
    virtual PassOwnPtr<CompositableValueList> sample(int iteration, double fraction) const OVERRIDE;

    // FIXME: Implement setFrames()
    const KeyframeVector& getFrames() const { return m_keyframes; }

    virtual bool isKeyframeEffectModel() const OVERRIDE { return true; }

    PropertySet properties() const;

    class PropertySpecificKeyframe {
    public:
        PropertySpecificKeyframe(double offset, const AnimatableValue*, CompositeOperation);
        double offset() const { return m_offset; }
        const CompositableValue* value() const { return m_value.get(); }
        PassOwnPtr<PropertySpecificKeyframe> cloneWithOffset(double offset) const;
    private:
        // Used by cloneWithOffset().
        PropertySpecificKeyframe(double offset, PassRefPtr<CompositableValue>);
        double m_offset;
        RefPtr<CompositableValue> m_value;
    };

    class PropertySpecificKeyframeGroup {
    public:
        void appendKeyframe(PassOwnPtr<PropertySpecificKeyframe>);
        PassRefPtr<CompositableValue> sample(int iteration, double offset) const;
        const PropertySpecificKeyframeVector& keyframes() const { return m_keyframes; }
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

private:
    KeyframeEffectModel(const KeyframeVector& keyframes);

    static KeyframeVector normalizedKeyframes(const KeyframeVector& keyframes);

    // Lazily computes the groups of property-specific keyframes.
    void ensureKeyframeGroups() const;

    KeyframeVector m_keyframes;
    // The spec describes filtering the normalized keyframes at sampling time
    // to get the 'property-specific keyframes'. For efficiency, we cache the
    // property-specific lists.
    typedef HashMap<CSSPropertyID, OwnPtr<PropertySpecificKeyframeGroup> > KeyframeGroupMap;
    mutable OwnPtr<KeyframeGroupMap> m_keyframeGroups;

    friend class KeyframeEffectModelTest;
};

DEFINE_TYPE_CASTS(KeyframeEffectModel, AnimationEffect, value, value->isKeyframeEffectModel(), value.isKeyframeEffectModel());

} // namespace WebCore

#endif // KeyframeEffectModel_h
