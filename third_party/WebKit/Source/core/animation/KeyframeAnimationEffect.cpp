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

#include "config.h"
#include "core/animation/KeyframeAnimationEffect.h"

#include "wtf/MathExtras.h"
#include "wtf/text/StringHash.h"

namespace {

using namespace WebCore;

class ReplaceCompositableValue : public AnimationEffect::CompositableValue {
public:
    static PassRefPtr<ReplaceCompositableValue> create(const AnimatableValue* value)
    {
        return adoptRef(new ReplaceCompositableValue(value));
    }
    virtual bool dependsOnUnderlyingValue() const
    {
        return false;
    }
    virtual PassRefPtr<AnimatableValue> compositeOnto(const AnimatableValue* underlyingValue) const
    {
        return PassRefPtr<AnimatableValue>(m_value);
    }
private:
    ReplaceCompositableValue(const AnimatableValue* value)
        : m_value(const_cast<AnimatableValue*>(value))
    {
    }
    RefPtr<AnimatableValue> m_value;
};

class AddCompositableValue : public AnimationEffect::CompositableValue {
public:
    static PassRefPtr<AddCompositableValue> create(const AnimatableValue* value)
    {
        return adoptRef(new AddCompositableValue(value));
    }
    virtual bool dependsOnUnderlyingValue() const
    {
        return true;
    }
    virtual PassRefPtr<AnimatableValue> compositeOnto(const AnimatableValue* underlyingValue) const
    {
        return AnimatableValue::add(underlyingValue, m_value.get());
    }
private:
    AddCompositableValue(const AnimatableValue* value)
        : m_value(const_cast<AnimatableValue*>(value))
    {
    }
    RefPtr<AnimatableValue> m_value;
};

class BlendedCompositableValue : public AnimationEffect::CompositableValue {
public:
    static PassRefPtr<BlendedCompositableValue> create(const AnimationEffect::CompositableValue* before, const AnimationEffect::CompositableValue* after, double fraction)
    {
        return adoptRef(new BlendedCompositableValue(before, after, fraction));
    }
    virtual bool dependsOnUnderlyingValue() const
    {
        return m_dependsOnUnderlyingValue;
    }
    virtual PassRefPtr<AnimatableValue> compositeOnto(const AnimatableValue* underlyingValue) const
    {
        return AnimatableValue::interpolate(m_before->compositeOnto(underlyingValue).get(), m_after->compositeOnto(underlyingValue).get(), m_fraction);
    }
private:
    BlendedCompositableValue(const AnimationEffect::CompositableValue* before, const AnimationEffect::CompositableValue* after, double fraction)
        : m_before(const_cast<AnimationEffect::CompositableValue*>(before))
        , m_after(const_cast<AnimationEffect::CompositableValue*>(after))
        , m_fraction(fraction)
        , m_dependsOnUnderlyingValue(before->dependsOnUnderlyingValue() || after->dependsOnUnderlyingValue())
    { }
    RefPtr<AnimationEffect::CompositableValue> m_before;
    RefPtr<AnimationEffect::CompositableValue> m_after;
    double m_fraction;
    bool m_dependsOnUnderlyingValue;
};

} // namespace


namespace WebCore {

Keyframe::Keyframe()
    : m_offset(std::numeric_limits<double>::quiet_NaN())
    , m_composite(AnimationEffect::CompositeReplace)
{ }

void Keyframe::setPropertyValue(CSSPropertyID property, const AnimatableValue* value)
{
    m_propertyValues.add(property, const_cast<AnimatableValue*>(value));
}

const AnimatableValue* Keyframe::propertyValue(CSSPropertyID property) const
{
    ASSERT(m_propertyValues.contains(property));
    return m_propertyValues.get(property);
}

PropertySet Keyframe::properties() const
{
    // This is only used when setting up the keyframe groups, so there's no
    // need to cache the result.
    PropertySet properties;
    for (PropertyValueMap::const_iterator iter = m_propertyValues.begin(); iter != m_propertyValues.end(); ++iter)
        properties.add(*iter.keys());
    return properties;
}

PassRefPtr<Keyframe> Keyframe::cloneWithOffset(double offset) const
{
    RefPtr<Keyframe> clone = Keyframe::create();
    clone->setOffset(offset);
    clone->setComposite(m_composite);
    for (PropertyValueMap::const_iterator iter = m_propertyValues.begin(); iter != m_propertyValues.end(); ++iter)
        clone->setPropertyValue(iter->key, iter->value.get());
    return clone.release();
}


KeyframeAnimationEffect::KeyframeAnimationEffect(const KeyframeVector& keyframes)
    : m_keyframes(keyframes)
{
}

PassOwnPtr<AnimationEffect::CompositableValueMap> KeyframeAnimationEffect::sample(int iteration, double fraction) const
{
    const_cast<KeyframeAnimationEffect*>(this)->ensureKeyframeGroups();
    OwnPtr<CompositableValueMap> map = adoptPtr(new CompositableValueMap());
    for (KeyframeGroupMap::const_iterator iter = m_keyframeGroups->begin(); iter != m_keyframeGroups->end(); ++iter)
        map->add(iter->key, iter->value->sample(iteration, fraction));
    return map.release();
}

KeyframeAnimationEffect::KeyframeVector KeyframeAnimationEffect::normalizedKeyframes() const
{
    KeyframeVector keyframes = m_keyframes;

    // Set offsets at 0.0 and 1.0 at ends if unset.
    if (keyframes.size() >= 2) {
        Keyframe* firstKeyframe = keyframes.first().get();
        if (std::isnan(firstKeyframe->offset()))
            firstKeyframe->setOffset(0.0);
    }
    if (keyframes.size() >= 1) {
        Keyframe* lastKeyframe = keyframes.last().get();
        if (lastKeyframe && std::isnan(lastKeyframe->offset()))
            lastKeyframe->setOffset(1.0);
    }

    // FIXME: Distribute offsets where missing.
    for (KeyframeVector::iterator iter = keyframes.begin(); iter != keyframes.end(); ++iter)
        ASSERT(!std::isnan((*iter)->offset()));

    // Sort by offset.
    std::stable_sort(keyframes.begin(), keyframes.end(), Keyframe::compareOffsets);
    return keyframes;
}

void KeyframeAnimationEffect::ensureKeyframeGroups()
{
    if (m_keyframeGroups)
        return;

    m_keyframeGroups = adoptPtr(new KeyframeGroupMap);
    const KeyframeVector& keyframes = normalizedKeyframes();
    for (KeyframeVector::const_iterator keyframeIter = keyframes.begin(); keyframeIter != keyframes.end(); ++keyframeIter) {
        const Keyframe* keyframe = keyframeIter->get();
        PropertySet keyframeProperties = keyframe->properties();
        for (PropertySet::const_iterator propertyIter = keyframeProperties.begin(); propertyIter != keyframeProperties.end(); ++propertyIter) {
            CSSPropertyID property = *propertyIter;
            KeyframeGroupMap::iterator groupIter = m_keyframeGroups->find(property);
            if (groupIter == m_keyframeGroups->end()) {
                KeyframeGroupMap::AddResult result = m_keyframeGroups->add(property, adoptPtr(new PropertySpecificKeyframeGroup));
                ASSERT(result.isNewEntry);
                groupIter = result.iterator;
            }
            groupIter->value->appendKeyframe(adoptPtr(
                new PropertySpecificKeyframe(keyframe->offset(), keyframe->propertyValue(property), keyframe->composite())));
        }
    }

    // Add synthetic keyframes.
    for (KeyframeGroupMap::iterator iter = m_keyframeGroups->begin(); iter != m_keyframeGroups->end(); ++iter) {
        iter->value->addSyntheticKeyframeIfRequired();
        iter->value->removeRedundantKeyframes();
    }
}


KeyframeAnimationEffect::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, const AnimatableValue* value, CompositeOperation composite)
    : m_offset(offset)
    , m_value(composite == AnimationEffect::CompositeReplace ?
        static_cast<PassRefPtr<CompositableValue> >(ReplaceCompositableValue::create(value)) :
        static_cast<PassRefPtr<CompositableValue> >(AddCompositableValue::create(value)))
{
}

KeyframeAnimationEffect::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<CompositableValue> value)
    : m_offset(offset)
    , m_value(value)
{
    ASSERT(!std::isnan(m_offset));
}

PassOwnPtr<KeyframeAnimationEffect::PropertySpecificKeyframe> KeyframeAnimationEffect::PropertySpecificKeyframe::cloneWithOffset(double offset) const
{
    return adoptPtr(new PropertySpecificKeyframe(offset, PassRefPtr<CompositableValue>(m_value)));
}


void KeyframeAnimationEffect::PropertySpecificKeyframeGroup::appendKeyframe(PassOwnPtr<PropertySpecificKeyframe> keyframe)
{
    ASSERT(m_keyframes.isEmpty() || m_keyframes.last()->offset() <= keyframe->offset());
    m_keyframes.append(keyframe);
}

void KeyframeAnimationEffect::PropertySpecificKeyframeGroup::removeRedundantKeyframes()
{
    // As an optimization, removes keyframes in the following categories, as
    // they will never be used by sample().
    // - End keyframes with the same offset as their neighbor
    // - Interior keyframes with the same offset as both their neighbors
    // Note that synthetic keyframes must be added before this method is
    // called.
    ASSERT(m_keyframes.size() >= 2);
    for (int i = m_keyframes.size() - 1; i >= 0; --i) {
        double offset = m_keyframes[i]->offset();
        bool hasSameOffsetAsPreviousNeighbor = !i || m_keyframes[i - 1]->offset() == offset;
        bool hasSameOffsetAsNextNeighbor = i == m_keyframes.size() - 1 || m_keyframes[i + 1]->offset() == offset;
        if (hasSameOffsetAsPreviousNeighbor && hasSameOffsetAsNextNeighbor)
            m_keyframes.remove(i);
    }
    ASSERT(m_keyframes.size() >= 2);
}

void KeyframeAnimationEffect::PropertySpecificKeyframeGroup::addSyntheticKeyframeIfRequired()
{
    ASSERT(!m_keyframes.isEmpty());
    double offset = m_keyframes.first()->offset();
    bool allOffsetsEqual = true;
    for (PropertySpecificKeyframeVector::const_iterator iter = m_keyframes.begin() + 1; iter != m_keyframes.end(); ++iter) {
        if ((*iter)->offset() != offset) {
            allOffsetsEqual = false;
            break;
        }
    }
    if (!allOffsetsEqual)
        return;

    if (!offset)
        appendKeyframe(m_keyframes.first()->cloneWithOffset(1.0));
    else
        m_keyframes.insert(0, adoptPtr(new PropertySpecificKeyframe(0.0, AnimatableValue::neutralValue(), CompositeAdd)));
}

PassRefPtr<AnimationEffect::CompositableValue> KeyframeAnimationEffect::PropertySpecificKeyframeGroup::sample(int iteration, double offset) const
{
    // FIXME: Implement accumulation.
    ASSERT_UNUSED(iteration, iteration >= 0);

    double minimumOffset = m_keyframes.first()->offset();
    double maximumOffset = m_keyframes.last()->offset();
    ASSERT(minimumOffset != maximumOffset);

    PropertySpecificKeyframeVector::const_iterator before;
    PropertySpecificKeyframeVector::const_iterator after;

    // Note that this algorithm is simpler than that in the spec because we
    // have removed keyframes with equal offsets in
    // removeRedundantKeyframes().
    if (offset < minimumOffset) {
        before = m_keyframes.begin();
        after = before + 1;
        ASSERT((*before)->offset() > offset);
        ASSERT((*after)->offset() > offset);
    } else if (offset >= maximumOffset) {
        after = m_keyframes.end() - 1;
        before = after - 1;
        ASSERT((*before)->offset() < offset);
        ASSERT((*after)->offset() <= offset);
    } else {
        // FIXME: This is inefficient for large numbers of keyframes. Consider
        // using binary search.
        after = m_keyframes.begin();
        while ((*after)->offset() <= offset)
            ++after;
        before = after - 1;
        ASSERT((*before)->offset() <= offset);
        ASSERT((*after)->offset() > offset);
    }

    if ((*before)->offset() == offset)
        return const_cast<CompositableValue*>((*before)->value());
    if ((*after)->offset() == offset)
        return const_cast<CompositableValue*>((*after)->value());
    return BlendedCompositableValue::create((*before)->value(), (*after)->value(),
        (offset - (*before)->offset()) / ((*after)->offset() - (*before)->offset()));
}

} // namespace
