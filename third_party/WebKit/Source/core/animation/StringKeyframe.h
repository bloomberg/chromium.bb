// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StringKeyframe_h
#define StringKeyframe_h

#include "core/animation/Keyframe.h"
#include "core/css/StylePropertySet.h"

#include "platform/wtf/HashMap.h"

namespace blink {

class StyleSheetContents;

// A specialization of Keyframe that associates user specified keyframe
// properties with either CSS properties or SVG attributes.
class CORE_EXPORT StringKeyframe : public Keyframe {
 public:
  static RefPtr<StringKeyframe> Create() {
    return AdoptRef(new StringKeyframe);
  }

  MutableStylePropertySet::SetResult SetCSSPropertyValue(
      const AtomicString& property_name,
      const PropertyRegistry*,
      const String& value,
      StyleSheetContents*);
  MutableStylePropertySet::SetResult SetCSSPropertyValue(CSSPropertyID,
                                                         const String& value,
                                                         StyleSheetContents*);
  void SetCSSPropertyValue(CSSPropertyID, const CSSValue&);
  void SetPresentationAttributeValue(CSSPropertyID,
                                     const String& value,
                                     StyleSheetContents*);
  void SetSVGAttributeValue(const QualifiedName&, const String& value);

  const CSSValue& CssPropertyValue(const PropertyHandle& property) const {
    int index = -1;
    if (property.IsCSSCustomProperty())
      index =
          css_property_map_->FindPropertyIndex(property.CustomPropertyName());
    else
      index = css_property_map_->FindPropertyIndex(property.CssProperty());
    CHECK_GE(index, 0);
    return css_property_map_->PropertyAt(static_cast<unsigned>(index)).Value();
  }

  const CSSValue& PresentationAttributeValue(CSSPropertyID property) const {
    int index = presentation_attribute_map_->FindPropertyIndex(property);
    CHECK_GE(index, 0);
    return presentation_attribute_map_->PropertyAt(static_cast<unsigned>(index))
        .Value();
  }

  String SvgPropertyValue(const QualifiedName& attribute_name) const {
    return svg_attribute_map_.at(&attribute_name);
  }

  PropertyHandleSet Properties() const override;

  class CSSPropertySpecificKeyframe
      : public Keyframe::PropertySpecificKeyframe {
   public:
    static RefPtr<CSSPropertySpecificKeyframe> Create(
        double offset,
        RefPtr<TimingFunction> easing,
        const CSSValue* value,
        EffectModel::CompositeOperation composite) {
      return AdoptRef(new CSSPropertySpecificKeyframe(offset, std::move(easing),
                                                      value, composite));
    }

    const CSSValue* Value() const { return value_.Get(); }

    bool PopulateAnimatableValue(CSSPropertyID,
                                 Element&,
                                 const ComputedStyle& base_style,
                                 const ComputedStyle* parent_style) const final;
    const AnimatableValue* GetAnimatableValue() const final {
      return animatable_value_cache_.Get();
    }

    bool IsNeutral() const final { return !value_; }
    RefPtr<Keyframe::PropertySpecificKeyframe> NeutralKeyframe(
        double offset,
        RefPtr<TimingFunction> easing) const final;

   private:
    CSSPropertySpecificKeyframe(double offset,
                                RefPtr<TimingFunction> easing,
                                const CSSValue* value,
                                EffectModel::CompositeOperation composite)
        : Keyframe::PropertySpecificKeyframe(offset,
                                             std::move(easing),
                                             composite),
          value_(const_cast<CSSValue*>(value)) {}

    virtual RefPtr<Keyframe::PropertySpecificKeyframe> CloneWithOffset(
        double offset) const;
    bool IsCSSPropertySpecificKeyframe() const override { return true; }

    // TODO(sashab): Make this a const CSSValue.
    Persistent<CSSValue> value_;
    mutable RefPtr<AnimatableValue> animatable_value_cache_;
  };

  class SVGPropertySpecificKeyframe
      : public Keyframe::PropertySpecificKeyframe {
   public:
    static RefPtr<SVGPropertySpecificKeyframe> Create(
        double offset,
        RefPtr<TimingFunction> easing,
        const String& value,
        EffectModel::CompositeOperation composite) {
      return AdoptRef(new SVGPropertySpecificKeyframe(offset, std::move(easing),
                                                      value, composite));
    }

    const String& Value() const { return value_; }

    RefPtr<PropertySpecificKeyframe> CloneWithOffset(double offset) const final;

    const AnimatableValue* GetAnimatableValue() const final { return nullptr; }

    bool IsNeutral() const final { return value_.IsNull(); }
    RefPtr<PropertySpecificKeyframe> NeutralKeyframe(
        double offset,
        RefPtr<TimingFunction> easing) const final;

   private:
    SVGPropertySpecificKeyframe(double offset,
                                RefPtr<TimingFunction> easing,
                                const String& value,
                                EffectModel::CompositeOperation composite)
        : Keyframe::PropertySpecificKeyframe(offset,
                                             std::move(easing),
                                             composite),
          value_(value) {}

    bool IsSVGPropertySpecificKeyframe() const override { return true; }

    String value_;
  };

 private:
  StringKeyframe()
      : css_property_map_(MutableStylePropertySet::Create(kHTMLStandardMode)),
        presentation_attribute_map_(
            MutableStylePropertySet::Create(kHTMLStandardMode)) {}

  StringKeyframe(const StringKeyframe& copy_from);

  RefPtr<Keyframe> Clone() const override;
  RefPtr<Keyframe::PropertySpecificKeyframe> CreatePropertySpecificKeyframe(
      const PropertyHandle&) const override;

  bool IsStringKeyframe() const override { return true; }

  Persistent<MutableStylePropertySet> css_property_map_;
  Persistent<MutableStylePropertySet> presentation_attribute_map_;
  HashMap<const QualifiedName*, String> svg_attribute_map_;
};

using CSSPropertySpecificKeyframe = StringKeyframe::CSSPropertySpecificKeyframe;
using SVGPropertySpecificKeyframe = StringKeyframe::SVGPropertySpecificKeyframe;

DEFINE_TYPE_CASTS(StringKeyframe,
                  Keyframe,
                  value,
                  value->IsStringKeyframe(),
                  value.IsStringKeyframe());
DEFINE_TYPE_CASTS(CSSPropertySpecificKeyframe,
                  Keyframe::PropertySpecificKeyframe,
                  value,
                  value->IsCSSPropertySpecificKeyframe(),
                  value.IsCSSPropertySpecificKeyframe());
DEFINE_TYPE_CASTS(SVGPropertySpecificKeyframe,
                  Keyframe::PropertySpecificKeyframe,
                  value,
                  value->IsSVGPropertySpecificKeyframe(),
                  value.IsSVGPropertySpecificKeyframe());

}  // namespace blink

#endif
