// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FEATURE_VALUES_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FEATURE_VALUES_RULE_H_

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class StyleRuleFontFeatureValues;

class CSSFontFeatureValuesRule final : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSFontFeatureValuesRule(StyleRuleFontFeatureValues*, CSSStyleSheet* parent);
  ~CSSFontFeatureValuesRule() override;

  void setFontFamily(const String& font_family);
  String fontFamily();
  void setFontDisplay(const String& font_display);
  String fontDisplay();

  String cssText() const override;
  void Reattach(StyleRuleBase*) override;

  StyleRuleFontFeatureValues* StyleRule() const {
    return font_feature_values_rule_.Get();
  }

  void Trace(blink::Visitor*) override;

 private:
  CSSRule::Type type() const override { return kFontFeatureValuesRule; }

  Member<StyleRuleFontFeatureValues> font_feature_values_rule_;
};

template <>
struct DowncastTraits<CSSFontFeatureValuesRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.type() == CSSRule::kFontFeatureValuesRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FEATURE_VALUES_RULE_H_
