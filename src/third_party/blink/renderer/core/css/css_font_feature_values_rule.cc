// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_font_feature_values_rule.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSFontFeatureValuesRule::CSSFontFeatureValuesRule(
    StyleRuleFontFeatureValues* font_feature_values_rule,
    CSSStyleSheet* parent)
    : CSSRule(parent), font_feature_values_rule_(font_feature_values_rule) {}

CSSFontFeatureValuesRule::~CSSFontFeatureValuesRule() = default;

void CSSFontFeatureValuesRule::setFontFamily(const String& font_family) {}

String CSSFontFeatureValuesRule::fontFamily() {
  return font_feature_values_rule_->FontFamily().CssText();
}

void CSSFontFeatureValuesRule::setFontDisplay(const String& font_display) {}

String CSSFontFeatureValuesRule::fontDisplay() {
  if (font_feature_values_rule_->FontDisplay())
    return font_feature_values_rule_->FontDisplay()->CssText();
  return "";
}

String CSSFontFeatureValuesRule::cssText() const {
  StringBuilder result;
  result.Append("@font-feature-values ");
  DCHECK(font_feature_values_rule_);
  result.Append(font_feature_values_rule_->FontFamily().CssText());
  result.Append(" { ");
  if (const CSSIdentifierValue* display =
          font_feature_values_rule_->FontDisplay()) {
    result.Append("{ font-display: ");
    result.Append(display->CssText());
    result.Append("; } ");
  }
  result.Append("}");
  return result.ToString();
}

void CSSFontFeatureValuesRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  font_feature_values_rule_ = To<StyleRuleFontFeatureValues>(rule);
}

void CSSFontFeatureValuesRule::Trace(blink::Visitor* visitor) {
  visitor->Trace(font_feature_values_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
