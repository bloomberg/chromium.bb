// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StringKeyframe_h
#define StringKeyframe_h

#include "core/animation/AnimatableValue.h"
#include "core/animation/Keyframe.h"

namespace WebCore {

class StringKeyframe : public Keyframe {
public:
    static PassRefPtr<StringKeyframe> create() { return adoptRef(new StringKeyframe); }
    void setPropertyValue(CSSPropertyID property, const String& value)
    {
        m_propertyValues.add(property, value);
    }
    void clearPropertyValue(CSSPropertyID property) { m_propertyValues.remove(property); }
    String propertyValue(CSSPropertyID property) const
    {
        ASSERT(m_propertyValues.contains(property));
        return m_propertyValues.get(property);
    }
    virtual PropertySet properties() const OVERRIDE;

    class PropertySpecificKeyframe : public Keyframe::PropertySpecificKeyframe {
    public:
        PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, const String& value, AnimationEffect::CompositeOperation);

        const String& value() const { return m_value; }

        virtual PassOwnPtr<Keyframe::PropertySpecificKeyframe> neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const OVERRIDE FINAL;
        virtual PassRefPtr<Interpolation> createInterpolation(CSSPropertyID, WebCore::Keyframe::PropertySpecificKeyframe* end) const OVERRIDE FINAL;
    private:
        PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, const String& value);

        virtual PassOwnPtr<Keyframe::PropertySpecificKeyframe> cloneWithOffset(double offset) const;
        virtual bool isStringPropertySpecificKeyframe() const OVERRIDE { return true; }

        String m_value;
    };

private:
    StringKeyframe() { }

    StringKeyframe(const StringKeyframe& copyFrom);

    virtual PassRefPtrWillBeRawPtr<Keyframe> clone() const OVERRIDE;
    virtual PassOwnPtr<Keyframe::PropertySpecificKeyframe> createPropertySpecificKeyframe(CSSPropertyID) const OVERRIDE;

    virtual bool isStringKeyframe() const OVERRIDE { return true; }

    typedef HashMap<CSSPropertyID, String> PropertyValueMap;
    PropertyValueMap m_propertyValues;
};

typedef StringKeyframe::PropertySpecificKeyframe StringPropertySpecificKeyframe;

}

#endif
