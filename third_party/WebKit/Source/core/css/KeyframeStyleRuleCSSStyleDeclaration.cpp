// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/KeyframeStyleRuleCSSStyleDeclaration.h"

#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"

namespace blink {

KeyframeStyleRuleCSSStyleDeclaration::KeyframeStyleRuleCSSStyleDeclaration(
    MutableCSSPropertyValueSet& property_set_arg,
    CSSKeyframeRule* parent_rule)
    : StyleRuleCSSStyleDeclaration(property_set_arg, parent_rule) {}

void KeyframeStyleRuleCSSStyleDeclaration::DidMutate(MutationType type) {
  StyleRuleCSSStyleDeclaration::DidMutate(type);
  ToCSSKeyframesRule(parent_rule_->parentRule())->StyleChanged();
}

}  // namespace blink
