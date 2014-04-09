// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/StringKeyframe.h"

#include "core/animation/AnimatableLength.h"
#include "core/animation/Interpolation.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/rendering/style/RenderStyle.h"

namespace WebCore {

StringKeyframe::StringKeyframe(const StringKeyframe& copyFrom)
    : Keyframe(copyFrom.m_offset, copyFrom.m_composite, copyFrom.m_easing)
    , m_propertySet(copyFrom.m_propertySet->mutableCopy())
{
}

void StringKeyframe::setPropertyValue(CSSPropertyID property, const String& value, StyleSheetContents* styleSheetContents)
{
    ASSERT(property != CSSPropertyInvalid);
    m_propertySet->setProperty(property, value, false, styleSheetContents);
}

PropertySet StringKeyframe::properties() const
{
    // This is not used in time-critical code, so we probably don't need to
    // worry about caching this result.
    PropertySet properties;
    for (unsigned i = 0; i < m_propertySet->propertyCount(); ++i) {
        // FIXME: Allow for non-animatable properties.
        CSSPropertyID property = m_propertySet->propertyAt(i).id();
        if (CSSAnimations::isAnimatableProperty(property))
            properties.add(property);
    }
    return properties;
}

PassRefPtrWillBeRawPtr<Keyframe> StringKeyframe::clone() const
{
    return adoptRefWillBeNoop(new StringKeyframe(*this));
}
PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::createPropertySpecificKeyframe(CSSPropertyID property) const
{
    return adoptPtrWillBeNoop(new PropertySpecificKeyframe(offset(), easing(), propertyValue(property), composite()));
}

void StringKeyframe::trace(Visitor* visitor)
{
    visitor->trace(m_propertySet);
    Keyframe::trace(visitor);
}

StringKeyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, CSSValue* value, AnimationEffect::CompositeOperation op)
    : Keyframe::PropertySpecificKeyframe(offset, easing, op)
    , m_value(value)
{ }

StringKeyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, CSSValue* value)
    : Keyframe::PropertySpecificKeyframe(offset, easing, AnimationEffect::CompositeReplace)
    , m_value(value)
{
    ASSERT(!isNull(m_offset));
}

PassRefPtrWillBeRawPtr<Interpolation> StringKeyframe::PropertySpecificKeyframe::createInterpolation(CSSPropertyID property, Keyframe::PropertySpecificKeyframe* end, Element* element) const
{
    CSSValue* fromCSSValue = m_value.get();
    CSSValue* toCSSValue = toStringPropertySpecificKeyframe(end)->value();

    switch (property) {
    case CSSPropertyLeft:
    case CSSPropertyRight:
    case CSSPropertyWidth:
    case CSSPropertyHeight:
        if (AnimatableLength::canCreateFrom(fromCSSValue) && AnimatableLength::canCreateFrom(toCSSValue))
            return LengthStyleInterpolation::create(fromCSSValue, toCSSValue, property);
        break;
    default:
        break;
    }

    // FIXME: Remove the use of AnimatableValues, RenderStyles and Elements here.
    // FIXME: Remove this cache
    if (!m_animatableValueCache)
        m_animatableValueCache = StyleResolver::createAnimatableValueSnapshot(*element, property, fromCSSValue);

    RefPtrWillBeRawPtr<AnimatableValue> to = StyleResolver::createAnimatableValueSnapshot(*element, property, toCSSValue);
    toStringPropertySpecificKeyframe(end)->m_animatableValueCache = to;

    return LegacyStyleInterpolation::create(m_animatableValueCache.get(), to.release(), property);
}

PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::PropertySpecificKeyframe::neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const
{
    return adoptPtrWillBeNoop(new PropertySpecificKeyframe(offset, easing, 0, AnimationEffect::CompositeAdd));
}

PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::PropertySpecificKeyframe::cloneWithOffset(double offset) const
{
    Keyframe::PropertySpecificKeyframe* theClone = new PropertySpecificKeyframe(offset, m_easing, m_value.get());
    toStringPropertySpecificKeyframe(theClone)->m_animatableValueCache = m_animatableValueCache;
    return adoptPtrWillBeNoop(theClone);
}

void StringKeyframe::PropertySpecificKeyframe::trace(Visitor* visitor)
{
    visitor->trace(m_value);
    visitor->trace(m_animatableValueCache);
}

}
