// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StringKeyframe_h
#define StringKeyframe_h

#include "core/animation/Keyframe.h"
#include "core/css/StylePropertySet.h"

#include "wtf/HashMap.h"

namespace blink {

class StyleSheetContents;

class StringKeyframe : public Keyframe {
public:
    static PassRefPtr<StringKeyframe> create()
    {
        return adoptRef(new StringKeyframe);
    }

    void setCSSPropertyValue(CSSPropertyID, const String& value, Element*, StyleSheetContents*);
    void setCSSPropertyValue(CSSPropertyID, PassRefPtrWillBeRawPtr<CSSValue>);
    void setPresentationAttributeValue(CSSPropertyID, const String& value, Element*, StyleSheetContents*);
    void setSVGAttributeValue(const QualifiedName&, const String& value);

    CSSValue* cssPropertyValue(CSSPropertyID property) const
    {
        int index = m_cssPropertyMap->findPropertyIndex(property);
        RELEASE_ASSERT(index >= 0);
        return m_cssPropertyMap->propertyAt(static_cast<unsigned>(index)).value();
    }

    CSSValue* presentationAttributeValue(CSSPropertyID property) const
    {
        int index = m_presentationAttributeMap->findPropertyIndex(property);
        RELEASE_ASSERT(index >= 0);
        return m_presentationAttributeMap->propertyAt(static_cast<unsigned>(index)).value();
    }

    String svgPropertyValue(const QualifiedName& attributeName) const
    {
        return m_svgAttributeMap.get(&attributeName);
    }

    PropertyHandleSet properties() const override;

    class CSSPropertySpecificKeyframe : public Keyframe::PropertySpecificKeyframe {
    public:
        CSSPropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, CSSValue*, EffectModel::CompositeOperation);

        CSSValue* value() const { return m_value.get(); }

        virtual bool populateAnimatableValue(CSSPropertyID, Element&, const ComputedStyle* baseStyle, bool force) const;
        const PassRefPtr<AnimatableValue> getAnimatableValue() const final { return m_animatableValueCache.get(); }
        void setAnimatableValue(PassRefPtr<AnimatableValue> value) { m_animatableValueCache = value; }

        bool isNeutral() const final { return !m_value; }
        PassOwnPtr<Keyframe::PropertySpecificKeyframe> neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const final;
        PassRefPtr<Interpolation> maybeCreateInterpolation(PropertyHandle, Keyframe::PropertySpecificKeyframe& end, Element*, const ComputedStyle* baseStyle) const final;

    private:
        CSSPropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, CSSValue*);

        virtual PassOwnPtr<Keyframe::PropertySpecificKeyframe> cloneWithOffset(double offset) const;
        bool isCSSPropertySpecificKeyframe() const override { return true; }

        PassRefPtr<Interpolation> createLegacyStyleInterpolation(CSSPropertyID, Keyframe::PropertySpecificKeyframe& end, Element*, const ComputedStyle* baseStyle) const;
        static bool createInterpolationsFromCSSValues(CSSPropertyID, CSSValue* fromCSSValue, CSSValue* toCSSValue, Element*, OwnPtr<Vector<RefPtr<Interpolation>>>& interpolations);

        void populateAnimatableValueCaches(CSSPropertyID, Keyframe::PropertySpecificKeyframe&, Element*, CSSValue& fromCSSValue, CSSValue& toCSSValue) const;

        RefPtrWillBePersistent<CSSValue> m_value;
        mutable RefPtr<AnimatableValue> m_animatableValueCache;
    };

    class SVGPropertySpecificKeyframe : public Keyframe::PropertySpecificKeyframe {
    public:
        SVGPropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, const String&, EffectModel::CompositeOperation);

        const String& value() const { return m_value; }

        PassOwnPtr<PropertySpecificKeyframe> cloneWithOffset(double offset) const final;

        const PassRefPtr<AnimatableValue> getAnimatableValue() const final { return nullptr; }

        bool isNeutral() const final { return m_value.isNull(); }
        PassOwnPtr<PropertySpecificKeyframe> neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const final;
        PassRefPtr<Interpolation> maybeCreateInterpolation(PropertyHandle, Keyframe::PropertySpecificKeyframe& end, Element*, const ComputedStyle* baseStyle) const final;

    private:
        SVGPropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, const String&);

        bool isSVGPropertySpecificKeyframe() const override { return true; }

        String m_value;
    };

private:
    StringKeyframe()
        : m_cssPropertyMap(MutableStylePropertySet::create(HTMLStandardMode))
        , m_presentationAttributeMap(MutableStylePropertySet::create(HTMLStandardMode))
    { }

    StringKeyframe(const StringKeyframe& copyFrom);

    PassRefPtr<Keyframe> clone() const override;
    PassOwnPtr<Keyframe::PropertySpecificKeyframe> createPropertySpecificKeyframe(PropertyHandle) const override;

    bool isStringKeyframe() const override { return true; }

    RefPtrWillBePersistent<MutableStylePropertySet> m_cssPropertyMap;
    RefPtrWillBePersistent<MutableStylePropertySet> m_presentationAttributeMap;
    HashMap<const QualifiedName*, String> m_svgAttributeMap;
};

using CSSPropertySpecificKeyframe = StringKeyframe::CSSPropertySpecificKeyframe;
using SVGPropertySpecificKeyframe = StringKeyframe::SVGPropertySpecificKeyframe;

DEFINE_TYPE_CASTS(StringKeyframe, Keyframe, value, value->isStringKeyframe(), value.isStringKeyframe());
DEFINE_TYPE_CASTS(CSSPropertySpecificKeyframe, Keyframe::PropertySpecificKeyframe, value, value->isCSSPropertySpecificKeyframe(), value.isCSSPropertySpecificKeyframe());
DEFINE_TYPE_CASTS(SVGPropertySpecificKeyframe, Keyframe::PropertySpecificKeyframe, value, value->isSVGPropertySpecificKeyframe(), value.isSVGPropertySpecificKeyframe());

}

#endif
