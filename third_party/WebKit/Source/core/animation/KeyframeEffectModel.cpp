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
#include "core/animation/KeyframeEffectModel.h"

#include "core/animation/TimedItem.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

Keyframe::Keyframe()
    : m_offset(nullValue())
    , m_composite(AnimationEffect::CompositeReplace)
    , m_easing(LinearTimingFunction::preset())
{ }

Keyframe::Keyframe(const Keyframe& copyFrom)
    : m_offset(copyFrom.m_offset)
    , m_composite(copyFrom.m_composite)
    , m_easing(copyFrom.m_easing)
{
    ASSERT(m_easing);
    for (PropertyValueMap::const_iterator iter = copyFrom.m_propertyValues.begin(); iter != copyFrom.m_propertyValues.end(); ++iter)
        setPropertyValue(iter->key, iter->value.get());
}

void Keyframe::setEasing(PassRefPtr<TimingFunction> easing)
{
    ASSERT(easing);
    m_easing = easing;
}

void Keyframe::setPropertyValue(CSSPropertyID property, const AnimatableValue* value)
{
    m_propertyValues.add(property, const_cast<AnimatableValue*>(value));
}

void Keyframe::clearPropertyValue(CSSPropertyID property)
{
    m_propertyValues.remove(property);
}

const AnimatableValue* Keyframe::propertyValue(CSSPropertyID property) const
{
    ASSERT(m_propertyValues.contains(property));
    return m_propertyValues.get(property);
}

PropertySet Keyframe::properties() const
{
    // This is not used in time-critical code, so we probably don't need to
    // worry about caching this result.
    PropertySet properties;
    for (PropertyValueMap::const_iterator iter = m_propertyValues.begin(); iter != m_propertyValues.end(); ++iter)
        properties.add(*iter.keys());
    return properties;
}

PassRefPtrWillBeRawPtr<Keyframe> Keyframe::cloneWithOffset(double offset) const
{
    RefPtrWillBeRawPtr<Keyframe> theClone = clone();
    theClone->setOffset(offset);
    return theClone.release();
}

void Keyframe::trace(Visitor* visitor)
{
    visitor->trace(m_propertyValues);
}

KeyframeEffectModel::KeyframeEffectModel(const KeyframeVector& keyframes)
    : m_keyframes(keyframes)
{
}

PropertySet KeyframeEffectModel::properties() const
{
    PropertySet result;
    if (!m_keyframes.size()) {
        return result;
    }
    result = m_keyframes[0]->properties();
    for (size_t i = 1; i < m_keyframes.size(); i++) {
        PropertySet extras = m_keyframes[i]->properties();
        for (PropertySet::const_iterator it = extras.begin(); it != extras.end(); ++it) {
            result.add(*it);
        }
    }
    return result;
}

PassOwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation> > > KeyframeEffectModel::sample(int iteration, double fraction, double iterationDuration) const
{
    ASSERT(iteration >= 0);
    ASSERT(!isNull(fraction));
    ensureKeyframeGroups();
    ensureInterpolationEffect();

    return m_interpolationEffect->getActiveInterpolations(fraction, iterationDuration);
}

KeyframeEffectModel::KeyframeVector KeyframeEffectModel::normalizedKeyframes(const KeyframeVector& keyframes)
{
    // keyframes [beginIndex, endIndex) will remain after removing all keyframes if they are not
    // loosely sorted by offset, and after removing keyframes with positional offset outide [0, 1].
    size_t beginIndex = 0;
    size_t endIndex = keyframes.size();

    // Becomes the most recent keyframe with an explicit offset.
    size_t lastIndex = endIndex;
    double lastOffset = std::numeric_limits<double>::quiet_NaN();

    for (size_t i = 0; i < keyframes.size(); ++i) {
        double offset = keyframes[i]->offset();
        if (!isNull(offset)) {
            if (lastIndex < i && offset < lastOffset) {
                // The keyframes are not loosely sorted by offset. Exclude all.
                endIndex = beginIndex;
                break;
            }

            if (offset < 0) {
                // Remove all keyframes up to and including this keyframe.
                beginIndex = i + 1;
            } else if (offset > 1) {
                // Remove all keyframes from this keyframe onwards. Note we must complete our checking
                // that the keyframes are loosely sorted by offset, so we can't exit the loop early.
                endIndex = std::min(i, endIndex);
            }

            lastIndex = i;
            lastOffset = offset;
        }
    }

    KeyframeVector result;
    if (beginIndex != endIndex) {
        result.reserveCapacity(endIndex - beginIndex);
        for (size_t i = beginIndex; i < endIndex; ++i) {
            result.append(keyframes[i]->clone());
        }

        if (isNull(result[result.size() - 1]->offset()))
            result[result.size() - 1]->setOffset(1);

        if (result.size() > 1 && isNull(result[0]->offset()))
            result[0]->setOffset(0);

        lastIndex = 0;
        lastOffset = result[0]->offset();
        for (size_t i = 1; i < result.size(); ++i) {
            double offset = result[i]->offset();
            if (!isNull(offset)) {
                if (lastIndex + 1 < i) {
                    for (size_t j = 1; j < i - lastIndex; ++j)
                        result[lastIndex + j]->setOffset(lastOffset + (offset - lastOffset) * j / (i - lastIndex));
                }
                lastIndex = i;
                lastOffset = offset;
            }
        }
    }
    return result;
}

void KeyframeEffectModel::ensureKeyframeGroups() const
{
    if (m_keyframeGroups)
        return;

    m_keyframeGroups = adoptPtrWillBeNoop(new KeyframeGroupMap);
    const KeyframeVector keyframes = normalizedKeyframes(getFrames());
    for (KeyframeVector::const_iterator keyframeIter = keyframes.begin(); keyframeIter != keyframes.end(); ++keyframeIter) {
        const Keyframe* keyframe = keyframeIter->get();
        PropertySet keyframeProperties = keyframe->properties();
        for (PropertySet::const_iterator propertyIter = keyframeProperties.begin(); propertyIter != keyframeProperties.end(); ++propertyIter) {
            CSSPropertyID property = *propertyIter;
            KeyframeGroupMap::iterator groupIter = m_keyframeGroups->find(property);
            PropertySpecificKeyframeGroup* group;
            if (groupIter == m_keyframeGroups->end())
                group = m_keyframeGroups->add(property, adoptPtrWillBeNoop(new PropertySpecificKeyframeGroup)).storedValue->value.get();
            else
                group = groupIter->value.get();

            ASSERT(keyframe->composite() == AnimationEffect::CompositeReplace);
            group->appendKeyframe(adoptPtrWillBeNoop(
                new PropertySpecificKeyframe(keyframe->offset(), keyframe->easing(), keyframe->propertyValue(property), keyframe->composite())));
        }
    }

    // Add synthetic keyframes.
    for (KeyframeGroupMap::iterator iter = m_keyframeGroups->begin(); iter != m_keyframeGroups->end(); ++iter) {
        iter->value->addSyntheticKeyframeIfRequired();
        iter->value->removeRedundantKeyframes();
    }
}

void KeyframeEffectModel::ensureInterpolationEffect() const
{
    if (m_interpolationEffect)
        return;
    m_interpolationEffect = InterpolationEffect::create();

    for (KeyframeGroupMap::const_iterator iter = m_keyframeGroups->begin(); iter != m_keyframeGroups->end(); ++iter) {
        const PropertySpecificKeyframeVector& keyframes = iter->value->keyframes();
        ASSERT(keyframes[0]->composite() == AnimationEffect::CompositeReplace);
        const AnimatableValue* start;
        const AnimatableValue* end = keyframes[0]->value();
        for (size_t i = 0; i < keyframes.size() - 1; i++) {
            ASSERT(keyframes[i + 1]->composite() == AnimationEffect::CompositeReplace);
            start = end;
            end = keyframes[i + 1]->value();
            double applyFrom = i ? keyframes[i]->offset() : (-std::numeric_limits<double>::infinity());
            double applyTo = i == keyframes.size() - 2 ? std::numeric_limits<double>::infinity() : keyframes[i + 1]->offset();
            if (applyTo == 1)
                applyTo = std::numeric_limits<double>::infinity();
            m_interpolationEffect->addInterpolation(
                LegacyStyleInterpolation::create(
                    AnimatableValue::takeConstRef(start),
                    AnimatableValue::takeConstRef(end), iter->key),
                keyframes[i]->easing(), keyframes[i]->offset(), keyframes[i + 1]->offset(), applyFrom, applyTo);
        }
    }
}

bool KeyframeEffectModel::isReplaceOnly()
{
    ensureKeyframeGroups();
    for (KeyframeGroupMap::iterator iter = m_keyframeGroups->begin(); iter != m_keyframeGroups->end(); ++iter) {
        const PropertySpecificKeyframeVector& keyframeVector = iter->value->keyframes();
        for (size_t i = 0; i < keyframeVector.size(); ++i) {
            if (keyframeVector[i]->composite() != AnimationEffect::CompositeReplace)
                return false;
        }
    }
    return true;
}

void KeyframeEffectModel::trace(Visitor* visitor)
{
    visitor->trace(m_keyframes);
    visitor->trace(m_interpolationEffect);
#if ENABLE_OILPAN
    visitor->trace(m_keyframeGroups);
#endif
}

KeyframeEffectModel::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, const AnimatableValue* value, CompositeOperation composite)
    : m_offset(offset)
    , m_easing(easing)
    , m_composite(composite)
{
    m_value = AnimatableValue::takeConstRef(value);
}

KeyframeEffectModel::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, PassRefPtrWillBeRawPtr<AnimatableValue> value, CompositeOperation composite)
    : m_offset(offset)
    , m_easing(easing)
    , m_value(value)
    , m_composite(composite)
{
    ASSERT(!isNull(m_offset));
}

PassOwnPtrWillBeRawPtr<KeyframeEffectModel::PropertySpecificKeyframe> KeyframeEffectModel::PropertySpecificKeyframe::cloneWithOffset(double offset) const
{
    return adoptPtrWillBeNoop(new PropertySpecificKeyframe(offset, m_easing, m_value.get(), m_composite));
}

void KeyframeEffectModel::PropertySpecificKeyframe::trace(Visitor* visitor)
{
    visitor->trace(m_value);
}

void KeyframeEffectModel::PropertySpecificKeyframeGroup::appendKeyframe(PassOwnPtrWillBeRawPtr<PropertySpecificKeyframe> keyframe)
{
    ASSERT(m_keyframes.isEmpty() || m_keyframes.last()->offset() <= keyframe->offset());
    m_keyframes.append(keyframe);
}

void KeyframeEffectModel::PropertySpecificKeyframeGroup::removeRedundantKeyframes()
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
        bool hasSameOffsetAsNextNeighbor = i == static_cast<int>(m_keyframes.size() - 1) || m_keyframes[i + 1]->offset() == offset;
        if (hasSameOffsetAsPreviousNeighbor && hasSameOffsetAsNextNeighbor)
            m_keyframes.remove(i);
    }
    ASSERT(m_keyframes.size() >= 2);
}

void KeyframeEffectModel::PropertySpecificKeyframeGroup::addSyntheticKeyframeIfRequired()
{
    ASSERT(!m_keyframes.isEmpty());
    if (m_keyframes.first()->offset() != 0.0)
        m_keyframes.insert(0, adoptPtrWillBeNoop(new PropertySpecificKeyframe(0, nullptr, AnimatableValue::neutralValue(), CompositeAdd)));
    if (m_keyframes.last()->offset() != 1.0)
        appendKeyframe(adoptPtrWillBeNoop(new PropertySpecificKeyframe(1, nullptr, AnimatableValue::neutralValue(), CompositeAdd)));
}

void KeyframeEffectModel::PropertySpecificKeyframeGroup::trace(Visitor* visitor)
{
#if ENABLE_OILPAN
    visitor->trace(m_keyframes);
#endif
}

} // namespace
