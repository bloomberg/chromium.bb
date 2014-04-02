// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/StringKeyframe.h"

namespace WebCore {

StringKeyframe::StringKeyframe(const StringKeyframe& copyFrom)
    : Keyframe(copyFrom.m_offset, copyFrom.m_composite, copyFrom.m_easing)
{
    for (typename PropertyValueMap::const_iterator iter = copyFrom.m_propertyValues.begin(); iter != copyFrom.m_propertyValues.end(); ++iter)
        setPropertyValue(iter->key, iter->value);
}

PropertySet StringKeyframe::properties() const
{
    // This is not used in time-critical code, so we probably don't need to
    // worry about caching this result.
    PropertySet properties;
    for (typename PropertyValueMap::const_iterator iter = m_propertyValues.begin(); iter != m_propertyValues.end(); ++iter)
        properties.add(*iter.keys());
    return properties;
}

PassRefPtrWillBeRawPtr<Keyframe> StringKeyframe::clone() const OVERRIDE
{
    return adoptRefWillBeNoop(new StringKeyframe(*this));
}
PassOwnPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::createPropertySpecificKeyframe(CSSPropertyID property) const
{
    return adoptPtr(new PropertySpecificKeyframe(offset(), easing(), propertyValue(property), composite()));
}

StringKeyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, const String& value, AnimationEffect::CompositeOperation op)
    : Keyframe::PropertySpecificKeyframe(offset, easing, op)
    , m_value(value)
{ }

StringKeyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, const String& value)
    : Keyframe::PropertySpecificKeyframe(offset, easing, AnimationEffect::CompositeReplace)
    , m_value(value)
{
    ASSERT(!isNull(m_offset));
}

PassRefPtr<Interpolation> StringKeyframe::PropertySpecificKeyframe::createInterpolation(CSSPropertyID property, Keyframe::PropertySpecificKeyframe* end) const
{
    ASSERT_UNUSED(end, end);
    // FIXME: Convert string keyframe value pairs to interpolations.
    return nullptr;
}

PassOwnPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::PropertySpecificKeyframe::neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const
{
    return adoptPtr(new StringKeyframe::PropertySpecificKeyframe(offset, easing, emptyString(), AnimationEffect::CompositeAdd));
}

PassOwnPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::PropertySpecificKeyframe::cloneWithOffset(double offset) const
{
    Keyframe::PropertySpecificKeyframe *theClone = new PropertySpecificKeyframe(offset, m_easing, m_value);
    return adoptPtr(theClone);
}

}
